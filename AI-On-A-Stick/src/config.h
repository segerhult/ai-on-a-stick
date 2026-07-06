#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct AppConfig {
    std::string usb_root;
    std::string ui_root;
    std::string data_root;
    std::string logs_root;
    std::string binary_macos_arm64;
    std::string binary_macos_x64;
    std::string binary_linux_x64;
    std::string binary_linux_arm64;
    std::string binary_windows_x64;
    std::string llama_binary;
    std::string model_small_path;
    std::string model_medium_path;
    std::string model_large_path;
    std::string model_coder_path;
    std::string model_path;
    std::string llama_host = "127.0.0.1";
    int llama_port = 8080;
    int coder_llama_port = 8081;
    int app_port = 8090;
    int context_size = 4096;
    int parallel_slots = 2;
    int gpu_layers = 0;
    int coder_context_size = 8192;
    int coder_parallel_slots = 2;
    int coder_gpu_layers = -1;
    std::string runtime_profile_name = "manual";
    std::string detected_gpu_backend = "cpu";
    std::uint32_t detected_logical_cores = 1;
    std::uint64_t detected_memory_gb = 0;
    bool auto_profile = true;
    bool prefer_gpu = true;
    bool auto_open_browser = true;
    bool auto_start_llama = true;
    std::vector<std::string> scan_paths;
};

AppConfig load_config(const std::string& usb_root);
