#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "config.h"

struct RuntimeTarget {
    std::string name;
    std::string binary_path;
    std::string model_path;
    std::string host = "127.0.0.1";
    int port = 8080;
    int context_size = 4096;
    int parallel_slots = 2;
    int gpu_layers = 0;
};

class LlamaRuntime {
  public:
    explicit LlamaRuntime(RuntimeTarget target);
    ~LlamaRuntime();

    bool can_start() const;
    void start();
    void stop();
    bool is_running() const;
    bool is_healthy() const;
    bool has_started() const;
    std::string base_url() const;
    const std::string& name() const;
    const std::string& model_path() const;
    const std::string& binary_path() const;
    const std::string& host() const;
    int port() const;
    int context_size() const;
    int parallel_slots() const;
    int gpu_layers() const;
    bool model_available() const;
    bool binary_available() const;
    std::int64_t startup_elapsed_ms() const;
    std::int64_t ready_elapsed_ms() const;
    int last_exit_status() const;

  private:
    RuntimeTarget target_;
    mutable std::mutex lifecycle_mutex_;
    std::thread process_thread_;
    std::atomic<bool> running_{false};
    std::atomic<int> child_pid_{0};
    mutable std::atomic<std::int64_t> started_at_ms_{0};
    mutable std::atomic<std::int64_t> ready_at_ms_{0};
    std::atomic<int> last_exit_status_{0};
};
