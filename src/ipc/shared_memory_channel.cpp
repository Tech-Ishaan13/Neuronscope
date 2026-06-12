#include "ipc/shared_memory_channel.h"
#include <iostream>

#if defined(NEURONSCOPE_WINDOWS) || defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

SharedMemoryChannel::SharedMemoryChannel(const std::string& name) : name_(name) {}

SharedMemoryChannel::~SharedMemoryChannel() {
    detach();
}

bool SharedMemoryChannel::create_and_map() {
    size_t size = sizeof(RingBuffer::Layout);

#if defined(NEURONSCOPE_WINDOWS) || defined(_WIN32)
    std::string pipe_name = "Local\\" + name_;
    handle_ = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        static_cast<DWORD>(size),
        pipe_name.c_str()
    );

    if (!handle_) {
        std::cerr << "Failed to create file mapping: " << GetLastError() << "\n";
        return false;
    }

    mapped_addr_ = MapViewOfFile(
        handle_,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        size
    );

    if (!mapped_addr_) {
        std::cerr << "Failed to map view of file: " << GetLastError() << "\n";
        CloseHandle(handle_);
        handle_ = nullptr;
        return false;
    }
#else
    std::string shm_name = "/" + name_;
    fd_ = shm_open(shm_name.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd_ == -1) {
        std::cerr << "Failed to shm_open\n";
        return false;
    }

    if (ftruncate(fd_, size) == -1) {
        std::cerr << "Failed to ftruncate shm\n";
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    mapped_addr_ = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (mapped_addr_ == MAP_FAILED) {
        std::cerr << "Failed to mmap shm\n";
        ::close(fd_);
        fd_ = -1;
        mapped_addr_ = nullptr;
        return false;
    }
#endif

    // Initialize the RingBuffer layout metadata
    ring_buffer_ = std::make_unique<RingBuffer>(static_cast<RingBuffer::Layout*>(mapped_addr_));
    ring_buffer_->init_metadata();
    return true;
}

bool SharedMemoryChannel::attach_and_map() {
    size_t size = sizeof(RingBuffer::Layout);

#if defined(NEURONSCOPE_WINDOWS) || defined(_WIN32)
    std::string pipe_name = "Local\\" + name_;
    handle_ = OpenFileMappingA(
        FILE_MAP_ALL_ACCESS,
        FALSE,
        pipe_name.c_str()
    );

    if (!handle_) {
        return false;
    }

    mapped_addr_ = MapViewOfFile(
        handle_,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        size
    );

    if (!mapped_addr_) {
        CloseHandle(handle_);
        handle_ = nullptr;
        return false;
    }
#else
    std::string shm_name = "/" + name_;
    fd_ = shm_open(shm_name.c_str(), O_RDWR, 0666);
    if (fd_ == -1) {
        return false;
    }

    mapped_addr_ = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (mapped_addr_ == MAP_FAILED) {
        ::close(fd_);
        fd_ = -1;
        mapped_addr_ = nullptr;
        return false;
    }
#endif

    ring_buffer_ = std::make_unique<RingBuffer>(static_cast<RingBuffer::Layout*>(mapped_addr_));
    return true;
}

void SharedMemoryChannel::detach() {
    if (mapped_addr_) {
#if defined(NEURONSCOPE_WINDOWS) || defined(_WIN32)
        UnmapViewOfFile(mapped_addr_);
#else
        munmap(mapped_addr_, sizeof(RingBuffer::Layout));
#endif
        mapped_addr_ = nullptr;
    }

#if defined(NEURONSCOPE_WINDOWS) || defined(_WIN32)
    if (handle_) {
        CloseHandle(handle_);
        handle_ = nullptr;
    }
#else
    if (fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
        std::string shm_name = "/" + name_;
        shm_unlink(shm_name.c_str());
    }
#endif

    ring_buffer_.reset();
}
