#include "ipc/control_pipe.h"
#include <iostream>
#include <vector>

#if defined(NEURONSCOPE_WINDOWS) || defined(_WIN32)
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

ControlPipe::ControlPipe(const std::string& pipe_name) : pipe_name_(pipe_name) {}

ControlPipe::~ControlPipe() {
    stop();
}

bool ControlPipe::create_and_listen() {
    running_ = true;
    
#if defined(NEURONSCOPE_WINDOWS) || defined(_WIN32)
    std::string full_pipe_name = "\\\\.\\pipe\\" + pipe_name_;
    pipe_handle_ = CreateNamedPipeA(
        full_pipe_name.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1,
        4096,
        4096,
        0,
        NULL
    );

    if (pipe_handle_ == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create named pipe: " << GetLastError() << "\n";
        pipe_handle_ = nullptr;
        return false;
    }
#else
    std::string full_pipe_name = "/tmp/" + pipe_name_;
    unlink(full_pipe_name.c_str());
    if (mkfifo(full_pipe_name.c_str(), 0666) == -1) {
        std::cerr << "Failed to create FIFO: " << full_pipe_name << "\n";
        return false;
    }
#endif

    listen_thread_ = std::thread(&ControlPipe::listen_loop, this);
    return true;
}

void ControlPipe::stop() {
    running_ = false;
    connected_ = false;

#if defined(NEURONSCOPE_WINDOWS) || defined(_WIN32)
    if (pipe_handle_) {
        // Force cancellation of pending synchronous ConnectNamedPipe or ReadFile
        CancelIoEx(pipe_handle_, NULL);
        CloseHandle(pipe_handle_);
        pipe_handle_ = nullptr;
    }
#else
    std::string full_pipe_name = "/tmp/" + pipe_name_;
    if (pipe_fd_ != -1) {
        ::close(pipe_fd_);
        pipe_fd_ = -1;
    }
    unlink(full_pipe_name.c_str());
#endif

    if (listen_thread_.joinable()) {
        listen_thread_.join();
    }
}

bool ControlPipe::write_msg(const std::string& msg) {
    if (!connected_) return false;

#if defined(NEURONSCOPE_WINDOWS) || defined(_WIN32)
    if (!pipe_handle_) return false;
    DWORD bytes_written;
    BOOL res = WriteFile(
        pipe_handle_,
        msg.c_str(),
        static_cast<DWORD>(msg.size()),
        &bytes_written,
        NULL
    );
    return res && (bytes_written == msg.size());
#else
    if (pipe_fd_ == -1) return false;
    ssize_t res = write(pipe_fd_, msg.c_str(), msg.size());
    return res == static_cast<ssize_t>(msg.size());
#endif
}

void ControlPipe::listen_loop() {
    while (running_) {
#if defined(NEURONSCOPE_WINDOWS) || defined(_WIN32)
        if (!pipe_handle_) break;
        
        BOOL connected = ConnectNamedPipe(pipe_handle_, NULL) ? 
            TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (connected) {
            connected_ = true;
            char buffer[4096];
            DWORD bytes_read;

            while (running_) {
                BOOL success = ReadFile(
                    pipe_handle_,
                    buffer,
                    sizeof(buffer) - 1,
                    &bytes_read,
                    NULL
                );

                if (!success || bytes_read == 0) {
                    if (GetLastError() == ERROR_BROKEN_PIPE) {
                        // Client disconnected
                        connected_ = false;
                        DisconnectNamedPipe(pipe_handle_);
                        break;
                    }
                    break;
                }

                buffer[bytes_read] = '\0';
                if (callback_) {
                    callback_(std::string(buffer, bytes_read));
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
#else
        std::string full_pipe_name = "/tmp/" + pipe_name_;
        pipe_fd_ = open(full_pipe_name.c_str(), O_RDONLY);
        if (pipe_fd_ != -1) {
            connected_ = true;
            char buffer[4096];
            while (running_) {
                ssize_t bytes_read = read(pipe_fd_, buffer, sizeof(buffer) - 1);
                if (bytes_read <= 0) {
                    connected_ = false;
                    ::close(pipe_fd_);
                    pipe_fd_ = -1;
                    break;
                }
                buffer[bytes_read] = '\0';
                if (callback_) {
                    callback_(std::string(buffer, bytes_read));
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
#endif
    }
}
