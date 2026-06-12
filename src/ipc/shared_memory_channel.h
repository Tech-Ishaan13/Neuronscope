#pragma once
#include "core/ring_buffer.h"
#include "neuronscope/telemetry_record.h"
#include <string>
#include <memory>

class SharedMemoryChannel {
public:
    using RingBuffer = SPSCRingBuffer<TelemetryRecord, 1024>;

    SharedMemoryChannel(const std::string& name);
    ~SharedMemoryChannel();

    bool create_and_map();
    bool attach_and_map();
    void detach();

    RingBuffer* get_ring_buffer() { return ring_buffer_.get(); }
    bool is_mapped() const { return mapped_addr_ != nullptr; }

private:
    std::string name_;
    void* mapped_addr_ = nullptr;
    std::unique_ptr<RingBuffer> ring_buffer_;

#if defined(NEURONSCOPE_WINDOWS) || defined(_WIN32)
    void* handle_ = nullptr;
#else
    int fd_ = -1;
#endif
};
