#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>

class ControlPipe {
public:
    ControlPipe(const std::string& pipe_name);
    ~ControlPipe();

    bool create_and_listen();
    void stop();
    bool write_msg(const std::string& msg);
    
    // Callback for received messages
    void set_callback(std::function<void(const std::string&)> cb) {
        callback_ = cb;
    }

    bool is_connected() const { return connected_; }

private:
    void listen_loop();

    std::string pipe_name_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::thread listen_thread_;
    std::function<void(const std::string&)> callback_;

#if defined(NEURONSCOPE_WINDOWS) || defined(_WIN32)
    void* pipe_handle_ = nullptr;
#else
    int pipe_fd_ = -1;
#endif
};
