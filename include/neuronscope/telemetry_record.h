#pragma once
#include <cstdint>

// Flags bitfield
enum TelemetryFlags : uint16_t {
    FLAG_NONE         = 0,
    FLAG_HAS_NAN      = 1 << 0,
    FLAG_HAS_INF      = 1 << 1,
    FLAG_OUTLIER      = 1 << 2,
    FLAG_CPU_FALLBACK = 1 << 3,
    FLAG_HAS_ATTN     = 1 << 4,  // attn_weights field is populated
};

// Layer type enum
enum LayerType : uint16_t {
    LAYER_EMBEDDING = 0,
    LAYER_ATTN_SELF = 1,
    LAYER_MLP       = 2,
    LAYER_NORM      = 3,
    LAYER_LINEAR    = 4,
    LAYER_OTHER     = 255,
};

#pragma pack(push, 1)
struct TelemetryRecord {
    uint64_t id;                  // Monotonic packet ID
    uint64_t timestamp_ns;        // time_point as nanoseconds since epoch
    uint32_t layer_index;         // Which layer (0-based)
    LayerType layer_type;         // Enum: ATTN, MLP, NORM, etc.
    uint16_t flags;               // Bitfield: has_nan, has_inf, outlier, etc.
    char layer_name[64];          // Null-terminated layer name string
    char device[16];              // "CUDA:0", "CPU", etc.
    uint32_t shape[4];            // Tensor dimensions (padded with 0s)
    char dtype[8];                // "fp16", "fp32", "bf16"
    float mean;                   // Activation mean
    float std_dev;                // Activation std deviation
    float min_val;                // Activation min
    float max_val;                // Activation max
    float sparsity;               // Fraction of zeros [0.0, 1.0]
    float latency_us;             // Layer execution time in microseconds
    uint64_t memory_bytes;        // CUDA memory allocated at this point
    // Attention weights (optional, FLAG_HAS_ATTN)
    uint16_t attn_num_heads;      // Number of attention heads
    uint16_t attn_seq_len;        // Sequence length
    float attn_weights[64 * 64];  // Flattened attention matrix for 1 head (64x64)
};
#pragma pack(pop)
