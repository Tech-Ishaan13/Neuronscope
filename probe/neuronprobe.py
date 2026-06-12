import ctypes
import time
import os
import torch
import torch.nn as nn
from transformers import AutoModelForCausalLM, AutoTokenizer

# Telemetry flags
FLAG_NONE         = 0
FLAG_HAS_NAN      = 1 << 0
FLAG_HAS_INF      = 1 << 1
FLAG_OUTLIER      = 1 << 2
FLAG_CPU_FALLBACK = 1 << 3
FLAG_HAS_ATTN     = 1 << 4

# Layer Type Enums
LAYER_EMBEDDING = 0
LAYER_ATTN_SELF = 1
LAYER_MLP       = 2
LAYER_NORM      = 3
LAYER_LINEAR    = 4
LAYER_OTHER     = 255

class TelemetryRecord(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("id", ctypes.c_uint64),
        ("timestamp_ns", ctypes.c_uint64),
        ("layer_index", ctypes.c_uint32),
        ("layer_type", ctypes.c_uint16),
        ("flags", ctypes.c_uint16),
        ("layer_name", ctypes.c_char * 64),
        ("device", ctypes.c_char * 16),
        ("shape", ctypes.c_uint32 * 4),
        ("dtype", ctypes.c_char * 8),
        ("mean", ctypes.c_float),
        ("std_dev", ctypes.c_float),
        ("min_val", ctypes.c_float),
        ("max_val", ctypes.c_float),
        ("sparsity", ctypes.c_float),
        ("latency_us", ctypes.c_float),
        ("memory_bytes", ctypes.c_uint64),
        ("attn_num_heads", ctypes.c_uint16),
        ("attn_seq_len", ctypes.c_uint16),
        ("attn_weights", ctypes.c_float * (64 * 64))
    ]

CAPACITY = 1024
HEADER_SIZE = 144
RECORD_SIZE = ctypes.sizeof(TelemetryRecord)
SHM_SIZE = HEADER_SIZE + CAPACITY * RECORD_SIZE

class PyTorchInstrumentor:
    def __init__(self, model, pipe_handle, shm):
        self.model = model
        self.pipe = pipe_handle
        self.shm = shm
        self.hooks = []
        self.packet_id = 1
        self.start_times = {}

        # Discover topology
        self.layers = []
        for name, module in model.named_modules():
            if not name:
                continue
            ltype = self.get_layer_type(module)
            self.layers.append((name, ltype))

    def get_layer_type(self, module):
        m_name = module.__class__.__name__.lower()
        if "embed" in m_name:
            return LAYER_EMBEDDING
        elif "attention" in m_name or "selfattn" in m_name:
            return LAYER_ATTN_SELF
        elif "mlp" in m_name or "swiglu" in m_name:
            return LAYER_MLP
        elif "norm" in m_name or "layernorm" in m_name:
            return LAYER_NORM
        elif "linear" in m_name:
            return LAYER_LINEAR
        return LAYER_OTHER

    def send_topology(self):
        print("Sending discovered model topology...")
        for name, ltype in self.layers:
            msg = f"TOPO:{name}:{ltype}\n"
            self.pipe.write(msg.encode() if os.name == 'nt' else msg)
            time.sleep(0.01)
        
        status_msg = f"STATUS:Loaded model:{self.model.__class__.__name__}:{self.model.__class__.__name__}\n"
        self.pipe.write(status_msg.encode() if os.name == 'nt' else status_msg)

    def attach_hooks(self):
        for idx, (name, ltype) in enumerate(self.layers):
            module = dict(self.model.named_modules())[name]
            
            # Register pre-hook for timing
            pre_handle = module.register_forward_pre_hook(
                lambda mod, inp, n=name: self._pre_hook(n)
            )
            self.hooks.append(pre_handle)

            # Register post-hook for stats and telemetry
            post_handle = module.register_forward_hook(
                lambda mod, inp, out, n=name, lt=ltype, i=idx: self._post_hook(n, lt, i, mod, inp, out)
            )
            self.hooks.append(post_handle)

    def _pre_hook(self, name):
        if torch.cuda.is_available():
            torch.cuda.synchronize()
        self.start_times[name] = time.perf_counter_ns()

    def _post_hook(self, name, ltype, idx, module, input, output):
        if torch.cuda.is_available():
            torch.cuda.synchronize()
        end_time = time.perf_counter_ns()
        start_time = self.start_times.get(name, end_time)
        latency_us = (end_time - start_time) / 1000.0

        # Unpack output tensor
        out_tensor = None
        attn_weights = None
        
        if isinstance(output, torch.Tensor):
            out_tensor = output
        elif isinstance(output, tuple):
            if len(output) > 0 and isinstance(output[0], torch.Tensor):
                out_tensor = output[0]
            if len(output) > 1 and isinstance(output[1], torch.Tensor):
                attn_weights = output[1]

        if out_tensor is None:
            return

        # Prepare record
        rec = TelemetryRecord()
        rec.id = self.packet_id
        rec.timestamp_ns = time.time_ns()
        rec.layer_index = idx
        rec.layer_type = ltype
        rec.flags = FLAG_NONE
        rec.layer_name = name.encode()
        
        dev_str = str(out_tensor.device)
        rec.device = dev_str.encode()
        
        # CPU Fallback check (mock logic / heuristic: if model is partially on GPU but this layer is on CPU)
        if "cpu" in dev_str and torch.cuda.is_available():
            rec.flags |= FLAG_CPU_FALLBACK
            
        rec.dtype = str(out_tensor.dtype).split('.')[-1].encode()
        rec.latency_us = latency_us

        # Shape
        shape = list(out_tensor.shape)
        for s_idx in range(min(4, len(shape))):
            rec.shape[s_idx] = shape[s_idx]

        # Stats
        with torch.no_grad():
            flat = out_tensor.detach().float()
            
            # Anomaly pre-screens
            has_nan = torch.isnan(flat).any().item()
            has_inf = torch.isinf(flat).any().item()
            if has_nan: rec.flags |= FLAG_HAS_NAN
            if has_inf: rec.flags |= FLAG_HAS_INF

            rec.mean = flat.mean().item()
            rec.std_dev = flat.std().item()
            rec.min_val = flat.min().item()
            rec.max_val = flat.max().item()
            
            # Sparsity
            num_elements = flat.numel()
            if num_elements > 0:
                rec.sparsity = (flat == 0).sum().item() / num_elements
            else:
                rec.sparsity = 0.0

            if rec.max_val > 6.0:
                rec.flags |= FLAG_OUTLIER

            if torch.cuda.is_available():
                rec.memory_bytes = torch.cuda.memory_allocated()
            else:
                rec.memory_bytes = 0

            # Attention weights
            if attn_weights is not None and ltype == LAYER_ATTN_SELF:
                rec.flags |= FLAG_HAS_ATTN
                # attn_weights shape: [batch, heads, seq_len, seq_len] or similar
                shape = list(attn_weights.shape)
                if len(shape) >= 3:
                    rec.attn_num_heads = shape[-3]
                    rec.attn_seq_len = shape[-2]
                    
                    # Extract head 0
                    h0_weights = attn_weights[0, 0].cpu().numpy()
                    h_h, h_w = h0_weights.shape
                    for y in range(min(64, h_h)):
                        for x in range(min(64, h_w)):
                            rec.attn_weights[y * 64 + x] = float(h0_weights[y, x])

        # Push to Ring Buffer
        self._push_telemetry(rec)
        self.packet_id += 1

    def _push_telemetry(self, record):
        self.shm.seek(64)
        head_bytes = self.shm.read(8)
        head = ctypes.c_uint64.from_buffer_copy(head_bytes).value
        
        self.shm.seek(128)
        tail_bytes = self.shm.read(8)
        tail = ctypes.c_uint64.from_buffer_copy(tail_bytes).value
        
        if tail - head >= CAPACITY:
            head += 1
            self.shm.seek(64)
            self.shm.write(ctypes.c_uint64(head))
            
        buf_offset = HEADER_SIZE + (tail & (CAPACITY - 1)) * RECORD_SIZE
        self.shm.seek(buf_offset)
        self.shm.write(bytes(record))
        
        self.shm.seek(128)
        self.shm.write(ctypes.c_uint64(tail + 1))

    def detach(self):
        for h in self.hooks:
            h.remove()
        self.hooks.clear()

def main():
    import argparse
    parser = argparse.ArgumentParser(description="NeuronProbe PyTorch telemetry hook agent")
    parser.add_argument("--model", type=str, default="gpt2", help="Hugging Face causal model name to test")
    args = parser.parse_args()

    # 1. Connect to control pipe
    if os.name == 'nt':
        pipe_path = r'\\.\pipe\neuronscope_control'
        print(f"Connecting to named pipe: {pipe_path}")
        pipe = open(pipe_path, 'r+b', buffering=0)
    else:
        pipe_path = '/tmp/neuronscope_control'
        print(f"Connecting to FIFO: {pipe_path}")
        while not os.path.exists(pipe_path):
            time.sleep(0.5)
        pipe = open(pipe_path, 'w')

    # 2. Open Shared memory
    if os.name == 'nt':
        import mmap
        shm = mmap.mmap(-1, SHM_SIZE, tagname="Local\\NeuronScope", access=mmap.ACCESS_WRITE)
    else:
        import mmap
        shm_path = "/dev/shm/neuronscope"
        while not os.path.exists(shm_path):
            time.sleep(0.5)
        fd = os.open(shm_path, os.O_RDWR)
        shm = mmap.mmap(fd, SHM_SIZE)

    print(f"Loading model: {args.model}...")
    tokenizer = AutoTokenizer.from_pretrained(args.model)
    model = AutoModelForCausalLM.from_pretrained(args.model)
    model.eval()

    instrumentor = PyTorchInstrumentor(model, pipe, shm)
    instrumentor.send_topology()
    instrumentor.attach_hooks()

    print("Model hooked! Input sentences to simulate inference telemetry. Press Ctrl+C to quit.")
    try:
        while True:
            text = input("\nEnter prompt for inference: ")
            if not text.strip():
                continue
            
            inputs = tokenizer(text, return_tensors="pt")
            
            print("Running forward pass...")
            with torch.no_grad():
                # Enable outputting attentions so we can inspect them
                model(**inputs, output_attentions=True)
            print("Forward pass completed. Telemetry generated.")
            
    except KeyboardInterrupt:
        print("\nDetaching hooks...")
    finally:
        instrumentor.detach()
        pipe.close()
        shm.close()

if __name__ == "__main__":
    main()
