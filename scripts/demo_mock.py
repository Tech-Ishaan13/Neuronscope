import ctypes
import time
import os
import math
import random

# Telemetry flags matching C++ header
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

# Layout of the SPSC Ring Buffer in Shared Memory
CAPACITY = 1024
HEADER_SIZE = 144 # 144 bytes offset before buffer starts
RECORD_SIZE = ctypes.sizeof(TelemetryRecord)
SHM_SIZE = HEADER_SIZE + CAPACITY * RECORD_SIZE

def connect_control_pipe():
    print("Connecting to control pipe...")
    if os.name == 'nt':
        pipe_path = r'\\.\pipe\neuronscope_control'
        while True:
            try:
                handle = open(pipe_path, 'r+b', buffering=0)
                print("Connected to Windows Named Pipe!")
                return handle
            except FileNotFoundError:
                time.sleep(0.5)
    else:
        pipe_path = '/tmp/neuronscope_control'
        while not os.path.exists(pipe_path):
            time.sleep(0.5)
        handle = open(pipe_path, 'w')
        print("Connected to POSIX FIFO!")
        return handle

def open_shared_memory():
    print("Opening shared memory region...")
    if os.name == 'nt':
        import mmap
        # Open existing mapping
        shm_name = "Local\\NeuronScope"
        while True:
            try:
                # Windows mmap attaches to tagname
                shm = mmap.mmap(-1, SHM_SIZE, tagname=shm_name, access=mmap.ACCESS_WRITE)
                print("Attached to Windows Shared Memory!")
                return shm
            except Exception as e:
                time.sleep(0.5)
    else:
        # UNIX shm_open / mmap
        import mmap
        shm_path = "/dev/shm/neuronscope"
        while not os.path.exists(shm_path):
            time.sleep(0.5)
        fd = os.open(shm_path, os.O_RDWR)
        shm = mmap.mmap(fd, SHM_SIZE)
        print("Attached to Linux Shared Memory!")
        return shm

def send_topology(pipe):
    print("Sending model topology...")
    layers = [
        ("model.embed_tokens", LAYER_EMBEDDING),
        ("model.layers.0.input_layernorm", LAYER_NORM),
        ("model.layers.0.self_attn", LAYER_ATTN_SELF),
        ("model.layers.0.self_attn.q_proj", LAYER_LINEAR),
        ("model.layers.0.self_attn.k_proj", LAYER_LINEAR),
        ("model.layers.0.self_attn.v_proj", LAYER_LINEAR),
        ("model.layers.0.self_attn.o_proj", LAYER_LINEAR),
        ("model.layers.0.post_attention_layernorm", LAYER_NORM),
        ("model.layers.0.mlp", LAYER_MLP),
        ("model.layers.1.input_layernorm", LAYER_NORM),
        ("model.layers.1.self_attn", LAYER_ATTN_SELF),
        ("model.layers.1.post_attention_layernorm", LAYER_NORM),
        ("model.layers.1.mlp", LAYER_MLP),
        ("model.norm", LAYER_NORM),
        ("lm_head", LAYER_LINEAR)
    ]
    
    # Send topology metadata
    for path, ltype in layers:
        msg = f"TOPO:{path}:{ltype}\n"
        pipe.write(msg.encode() if os.name == 'nt' else msg)
        time.sleep(0.02)
        
    status_msg = "STATUS:Loaded model:llama-3-8b:llama-3-8b\n"
    pipe.write(status_msg.encode() if os.name == 'nt' else status_msg)

def push_telemetry(shm, record):
    # Read head and tail atomic offsets
    # head offset = 64
    # tail offset = 128
    shm.seek(64)
    head_bytes = shm.read(8)
    head = ctypes.c_uint64.from_buffer_copy(head_bytes).value
    
    shm.seek(128)
    tail_bytes = shm.read(8)
    tail = ctypes.c_uint64.from_buffer_copy(tail_bytes).value
    
    # Check if full (tail - head >= CAPACITY)
    if tail - head >= CAPACITY:
        # Overwrite oldest by incrementing head
        head += 1
        shm.seek(64)
        shm.write(ctypes.c_uint64(head))
        
    # Write record to buffer[tail & (CAPACITY - 1)]
    buf_offset = HEADER_SIZE + (tail & (CAPACITY - 1)) * RECORD_SIZE
    shm.seek(buf_offset)
    shm.write(bytes(record))
    
    # Increment tail
    shm.seek(128)
    shm.write(ctypes.c_uint64(tail + 1))

def main():
    pipe = connect_control_pipe()
    send_topology(pipe)
    shm = open_shared_memory()
    
    print("Starting mock telemetry stream. Press Ctrl+C to stop.")
    packet_id = 1
    
    layers_info = [
        ("model.embed_tokens", LAYER_EMBEDDING, [1, 32, 4096]),
        ("model.layers.0.input_layernorm", LAYER_NORM, [1, 32, 4096]),
        ("model.layers.0.self_attn", LAYER_ATTN_SELF, [1, 32, 4096]),
        ("model.layers.0.post_attention_layernorm", LAYER_NORM, [1, 32, 4096]),
        ("model.layers.0.mlp", LAYER_MLP, [1, 32, 4096]),
        ("model.layers.1.input_layernorm", LAYER_NORM, [1, 32, 4096]),
        ("model.layers.1.self_attn", LAYER_ATTN_SELF, [1, 32, 4096]),
        ("model.layers.1.post_attention_layernorm", LAYER_NORM, [1, 32, 4096]),
        ("model.layers.1.mlp", LAYER_MLP, [1, 32, 4096]),
        ("model.norm", LAYER_NORM, [1, 32, 4096]),
        ("lm_head", LAYER_LINEAR, [1, 32, 128256])
    ]
    
    try:
        while True:
            # Simulate a full forward pass
            for idx, (name, ltype, shape) in enumerate(layers_info):
                rec = TelemetryRecord()
                rec.id = packet_id
                rec.timestamp_ns = time.time_ns()
                rec.layer_index = idx
                rec.layer_type = ltype
                rec.flags = FLAG_NONE
                rec.layer_name = name.encode()
                
                # Device allocation simulation
                # LayerNorm 1.108 OOM Fallback test case
                if name == "model.norm":
                    rec.device = b"CPU (Fallback)"
                    rec.flags |= FLAG_CPU_FALLBACK
                else:
                    rec.device = b"CUDA [GPU 0]"
                
                # Shape
                for s_idx, s_val in enumerate(shape):
                    rec.shape[s_idx] = s_val
                
                rec.dtype = b"float16"
                
                # Simulated stats
                if name == "model.layers.0.mlp":
                    # Outlier feature simulation
                    rec.mean = 0.85
                    rec.std_dev = 1.2
                    rec.min_val = -3.2
                    rec.max_val = 6.42 # Max > 6.0 Outlier warning!
                else:
                    rec.mean = random.uniform(-0.5, 0.5)
                    rec.std_dev = random.uniform(0.5, 1.5)
                    rec.min_val = rec.mean - 2 * rec.std_dev
                    rec.max_val = rec.mean + 2 * rec.std_dev
                    
                rec.sparsity = random.uniform(0.1, 0.6)
                if name.endswith("mlp"):
                    rec.sparsity = 0.542 # Matches reference mockup 54.2%
                
                rec.latency_us = random.uniform(500.0, 1500.0)
                if name == "model.norm":
                    # CPU fallback is slower
                    rec.latency_us = 4500.0
                
                rec.memory_bytes = 4210964480 if "CUDA" in str(rec.device) else 0
                
                # Attention weights for self_attn layers
                if ltype == LAYER_ATTN_SELF:
                    rec.flags |= FLAG_HAS_ATTN
                    rec.attn_num_heads = 32
                    rec.attn_seq_len = 8
                    # Generate a diagonal attention matrix pattern
                    for y in range(8):
                        for x in range(8):
                            # Softmax-like weight distribution
                            dist = abs(x - y)
                            weight = math.exp(-dist) / 2.0
                            if x == y:
                                weight = 0.8
                            rec.attn_weights[y * 64 + x] = weight
                
                # Randomly inject NaN in embed_tokens to test CRITICAL ledger
                if name == "model.embed_tokens" and random.random() < 0.05:
                    rec.flags |= FLAG_HAS_NAN
                
                push_telemetry(shm, rec)
                packet_id += 1
                time.sleep(0.05) # Delay between layers to simulate pipeline processing
                
            time.sleep(1.0) # Delay between complete forward passes
    except KeyboardInterrupt:
        print("\nStopping mock telemetry stream.")
    finally:
        pipe.close()
        shm.close()

if __name__ == "__main__":
    main()
