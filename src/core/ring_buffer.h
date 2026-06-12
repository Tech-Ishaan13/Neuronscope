#pragma once
#include <atomic>
#include <cstdint>
#include <stdexcept>

template <typename T, uint32_t Capacity>
class SPSCRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

public:
    struct Layout {
        uint32_t magic;
        uint32_t version;
        uint32_t capacity;
        uint32_t record_size;
        alignas(64) std::atomic<uint64_t> head;
        alignas(64) std::atomic<uint64_t> tail;
        uint64_t flags;
        T buffer[Capacity];
    };

    explicit SPSCRingBuffer(Layout* layout) : layout_(layout) {
        if (!layout_) {
            throw std::invalid_argument("Layout pointer cannot be null");
        }
    }

    void init_metadata() {
        layout_->magic = 0x4E53434F; // "NSCO"
        layout_->version = 1;
        layout_->capacity = Capacity;
        layout_->record_size = sizeof(T);
        layout_->head.store(0, std::memory_order_relaxed);
        layout_->tail.store(0, std::memory_order_relaxed);
        layout_->flags = 0;
    }

    bool try_push(const T& item) {
        uint64_t t = layout_->tail.load(std::memory_order_relaxed);
        uint64_t h = layout_->head.load(std::memory_order_acquire);

        if ((t - h) >= Capacity) {
            // Full. In telemetry, we can overwrite oldest by advancing head.
            layout_->head.store(h + 1, std::memory_order_release);
        }

        layout_->buffer[t & (Capacity - 1)] = item;
        layout_->tail.store(t + 1, std::memory_order_release);
        return true;
    }

    bool try_pop(T& item) {
        uint64_t h = layout_->head.load(std::memory_order_relaxed);
        uint64_t t = layout_->tail.load(std::memory_order_acquire);

        if (h == t) {
            return false; // Empty
        }

        item = layout_->buffer[h & (Capacity - 1)];
        layout_->head.store(h + 1, std::memory_order_release);
        return true;
    }

    uint64_t size() const {
        uint64_t t = layout_->tail.load(std::memory_order_relaxed);
        uint64_t h = layout_->head.load(std::memory_order_relaxed);
        return (t > h) ? (t - h) : 0;
    }

    bool empty() const {
        return size() == 0;
    }

private:
    Layout* layout_;
};
