#include <cstdlib>
#include <filesystem>
#include <iostream>

#if !defined(_WIN32)
#include <signal.h>
#endif

#include "config.h"
#include "database.h"
#include "hardware.h"
#include "http_server.h"
#include "llama_runtime.h"
#include "machine.h"

namespace {
namespace fs = std::filesystem;

bool env_is_enabled(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return false;
    }

    const std::string text(value);
    return text != "0" && text != "false" && text != "FALSE";
}

bool try_read_env_int(const char* name, int& value) {
    const char* raw = std::getenv(name);
    if (raw == nullptr) {
        return false;
    }

    try {
        value = std::stoi(raw);
        return true;
    } catch (...) {
        return false;
    }
}

RuntimeTarget build_general_target(const AppConfig& config) {
    RuntimeTarget target;
    target.name = "general";
    target.binary_path = config.llama_binary;
    target.model_path = config.model_path;
    target.host = config.llama_host;
    target.port = config.llama_port;
    target.context_size = config.context_size;
    target.parallel_slots = config.parallel_slots;
    target.gpu_layers = config.gpu_layers;
    return target;
}

RuntimeTarget build_coder_target(const AppConfig& config) {
    RuntimeTarget target;
    target.name = "coder";
    target.binary_path = config.llama_binary;
    target.model_path = config.model_coder_path;
    target.host = config.llama_host;
    target.port = config.coder_llama_port;
    target.context_size = config.coder_context_size;
    target.parallel_slots = config.coder_parallel_slots;
    target.gpu_layers = config.coder_gpu_layers >= 0 ? config.coder_gpu_layers : config.gpu_layers;
    return target;
}

void open_browser(int port) {
    const std::string url = "http://127.0.0.1:" + std::to_string(port);
#if defined(__APPLE__)
    const std::string command = "open " + url;
#elif defined(_WIN32)
    const std::string command = "start " + url;
#else
    const std::string command = "xdg-open " + url;
#endif
    std::system(command.c_str());
}
}  // namespace

int main() {
    try {
#if !defined(_WIN32)
        signal(SIGPIPE, SIG_IGN);
#endif
        const std::string usb_root = fs::current_path().string();
        AppConfig config = load_config(usb_root);
        try_read_env_int("AI_ON_A_STICK_APP_PORT", config.app_port);
        try_read_env_int("AI_ON_A_STICK_LLAMA_PORT", config.llama_port);
        try_read_env_int("AI_ON_A_STICK_CODER_PORT", config.coder_llama_port);

        fs::create_directories(config.data_root);
        fs::create_directories(config.logs_root);
        fs::create_directories(fs::path(config.data_root) / "machines");

        const MachineProfile machine = detect_machine();
        const HardwareProfile hardware = detect_hardware(machine);
        const RuntimeSelection selection = select_runtime_profile(config, machine, hardware);
        config.llama_binary = selection.llama_binary;
        config.model_path = selection.model_path;
        config.context_size = selection.context_size;
        config.parallel_slots = selection.parallel_slots;
        config.gpu_layers = selection.gpu_layers;
        config.runtime_profile_name = selection.profile_name;
        config.detected_gpu_backend = hardware.gpu_backend;
        config.detected_logical_cores = hardware.logical_cores;
        config.detected_memory_gb = hardware.memory_gb;

        if (config.coder_context_size <= 0) {
            config.coder_context_size = config.context_size;
        }
        if (config.coder_parallel_slots <= 0) {
            config.coder_parallel_slots = std::max(1, config.parallel_slots / 2);
        }
        if (config.auto_profile && config.runtime_profile_name == "metal-dual-safe") {
            config.coder_context_size = 4096;
            config.coder_parallel_slots = 1;
            config.coder_gpu_layers = 0;
        }

        Database database(config);
        database.initialize();
        database.upsert_machine(machine);
        database.index_scan_paths(machine, config.scan_paths);

        LlamaRuntime general_runtime(build_general_target(config));
        LlamaRuntime coder_runtime(build_coder_target(config));
        if (config.auto_start_llama && !env_is_enabled("AI_ON_A_STICK_NO_LLAMA")) {
            general_runtime.start();
        }

        if (config.auto_open_browser && !env_is_enabled("AI_ON_A_STICK_NO_BROWSER")) {
            open_browser(config.app_port);
        }

        std::cout << "AI-On-A-Stick\n";
        std::cout << "Machine: " << machine.hostname << " (" << machine.id << ")\n";
        std::cout << "Profile: " << config.runtime_profile_name << "\n";
        std::cout << "Hardware: " << config.detected_logical_cores << " cores, " << config.detected_memory_gb
                  << " GB RAM, " << config.detected_gpu_backend << "\n";
        std::cout << "UI: http://127.0.0.1:" << config.app_port << "\n";
        std::cout << "General runtime: " << general_runtime.base_url() << "\n";
        std::cout << "Coder runtime: " << coder_runtime.base_url() << "\n";

        HttpServer server(config, machine, database, general_runtime, coder_runtime);
        server.run();
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
