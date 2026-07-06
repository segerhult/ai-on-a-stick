#include "hardware.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <mach/mach.h>
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <sys/sysinfo.h>
#endif

namespace {
namespace fs = std::filesystem;

struct MemorySnapshot {
    std::uint64_t total_gb = 0;
    double used_gb = 0.0;
    double available_gb = 0.0;
};

std::string trim(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }
    return value;
}

std::string exec_capture(const char* command) {
#if defined(_WIN32)
    FILE* pipe = _popen(command, "r");
#else
    FILE* pipe = popen(command, "r");
#endif
    if (pipe == nullptr) {
        return "";
    }

    std::array<char, 256> buffer{};
    std::string output;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

#if defined(_WIN32)
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return trim(output);
}

MemorySnapshot detect_memory_snapshot() {
#if defined(_WIN32)
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status) != 0) {
        const double total_gb = static_cast<double>(status.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
        const double available_gb = static_cast<double>(status.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);
        MemorySnapshot snapshot;
        snapshot.total_gb = static_cast<std::uint64_t>(total_gb);
        snapshot.available_gb = available_gb;
        snapshot.used_gb = std::max(0.0, total_gb - available_gb);
        return snapshot;
    }
    return {};
#elif defined(__APPLE__)
    std::uint64_t bytes = 0;
    size_t size = sizeof(bytes);
    if (sysctlbyname("hw.memsize", &bytes, &size, nullptr, 0) == 0) {
        vm_statistics64_data_t vm_stats{};
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        const kern_return_t vm_result =
            host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm_stats), &count);
        if (vm_result == KERN_SUCCESS) {
            vm_size_t page_size = 0;
            host_page_size(mach_host_self(), &page_size);
            const double total_gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
            const std::uint64_t used_bytes = (static_cast<std::uint64_t>(vm_stats.active_count) +
                                              static_cast<std::uint64_t>(vm_stats.wire_count) +
                                              static_cast<std::uint64_t>(vm_stats.compressor_page_count)) *
                                             static_cast<std::uint64_t>(page_size);
            const std::uint64_t available_bytes =
                (static_cast<std::uint64_t>(vm_stats.free_count) + static_cast<std::uint64_t>(vm_stats.inactive_count)) *
                static_cast<std::uint64_t>(page_size);
            MemorySnapshot snapshot;
            snapshot.total_gb = static_cast<std::uint64_t>(total_gb);
            snapshot.used_gb = static_cast<double>(used_bytes) / (1024.0 * 1024.0 * 1024.0);
            snapshot.available_gb = static_cast<double>(available_bytes) / (1024.0 * 1024.0 * 1024.0);
            return snapshot;
        }
        MemorySnapshot snapshot;
        snapshot.total_gb = bytes / (1024ULL * 1024ULL * 1024ULL);
        return snapshot;
    }
    return {};
#elif defined(__linux__)
    struct sysinfo info {};
    if (sysinfo(&info) == 0) {
        const double total_gb = (static_cast<double>(info.totalram) * info.mem_unit) / (1024.0 * 1024.0 * 1024.0);
        const double available_gb = (static_cast<double>(info.freeram) * info.mem_unit) / (1024.0 * 1024.0 * 1024.0);
        MemorySnapshot snapshot;
        snapshot.total_gb = static_cast<std::uint64_t>(total_gb);
        snapshot.available_gb = available_gb;
        snapshot.used_gb = std::max(0.0, total_gb - available_gb);
        return snapshot;
    }
    return {};
#else
    return {};
#endif
}

std::string detect_cpu_vendor() {
#if defined(__APPLE__)
    const std::string brand = exec_capture("sysctl -n machdep.cpu.brand_string 2>/dev/null");
    return brand.empty() ? "apple" : brand;
#elif defined(__linux__)
    const std::string brand = exec_capture("sh -c \"grep -m1 'model name' /proc/cpuinfo | cut -d: -f2\"");
    return brand.empty() ? "linux-cpu" : trim(brand);
#elif defined(_WIN32)
    const std::string brand = exec_capture("wmic cpu get name | more +1");
    return brand.empty() ? "windows-cpu" : trim(brand);
#else
    return "unknown-cpu";
#endif
}

bool binary_exists(const std::string& path) {
    return !path.empty() && fs::exists(fs::path(path));
}

std::string choose_binary(const AppConfig& config, const MachineProfile& machine) {
    if (!config.llama_binary.empty() && binary_exists(config.llama_binary)) {
        return config.llama_binary;
    }

    if (machine.operating_system == "macos" && machine.architecture == "arm64") {
        return config.binary_macos_arm64;
    }
    if (machine.operating_system == "macos" && machine.architecture == "x64") {
        return config.binary_macos_x64;
    }
    if (machine.operating_system == "linux" && machine.architecture == "x64") {
        return config.binary_linux_x64;
    }
    if (machine.operating_system == "linux" && machine.architecture == "arm64") {
        return config.binary_linux_arm64;
    }
    if (machine.operating_system == "windows" && machine.architecture == "x64") {
        return config.binary_windows_x64;
    }

    return config.llama_binary;
}

std::string choose_model(const AppConfig& config, const std::string& preferred_tier) {
    const auto exists = [](const std::string& path) { return !path.empty() && fs::exists(fs::path(path)); };

    if (preferred_tier == "large" && exists(config.model_large_path)) {
        return config.model_large_path;
    }
    if ((preferred_tier == "large" || preferred_tier == "medium") && exists(config.model_medium_path)) {
        return config.model_medium_path;
    }
    if (exists(config.model_small_path)) {
        return config.model_small_path;
    }
    if (exists(config.model_medium_path)) {
        return config.model_medium_path;
    }
    if (exists(config.model_large_path)) {
        return config.model_large_path;
    }
    return config.model_path;
}
}  // namespace

HardwareProfile detect_hardware(const MachineProfile& machine) {
    HardwareProfile hardware;
    hardware.cpu_vendor = detect_cpu_vendor();
    hardware.logical_cores = std::max(1u, std::thread::hardware_concurrency());
    const MemorySnapshot memory = detect_memory_snapshot();
    hardware.memory_gb = memory.total_gb;
    hardware.memory_used_gb = memory.used_gb;
    hardware.memory_available_gb = memory.available_gb;

    if (machine.operating_system == "macos" && machine.architecture == "arm64") {
        hardware.has_gpu = true;
        hardware.gpu_backend = "metal";
        return hardware;
    }

    const std::string nvidia_smi = exec_capture("nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null");
    if (!nvidia_smi.empty()) {
        hardware.has_gpu = true;
        hardware.gpu_backend = "cuda";
        return hardware;
    }

#if defined(__linux__)
    if (fs::exists("/dev/dri/renderD128")) {
        hardware.has_gpu = true;
        hardware.gpu_backend = "vulkan";
    }
#endif

    return hardware;
}

RuntimeSelection select_runtime_profile(const AppConfig& config, const MachineProfile& machine,
                                        const HardwareProfile& hardware) {
    RuntimeSelection selection;
    selection.llama_binary = choose_binary(config, machine);

    const bool gpu_enabled = config.prefer_gpu && hardware.has_gpu;
    const bool metal_dual_safe = machine.operating_system == "macos" && machine.architecture == "arm64" &&
                                 hardware.memory_gb > 0 && hardware.memory_gb <= 16;
    const bool large_capable = gpu_enabled && hardware.memory_gb >= 24;
    const bool medium_capable = hardware.memory_gb >= 12 || gpu_enabled;

    if (metal_dual_safe) {
        selection.profile_name = "metal-dual-safe";
        selection.model_path = choose_model(config, "small");
        selection.context_size = 2048;
        selection.parallel_slots = 1;
        selection.gpu_layers = 0;
    } else if (large_capable) {
        selection.profile_name = hardware.gpu_backend + "-large";
        selection.model_path = choose_model(config, "large");
        selection.context_size = 8192;
        selection.parallel_slots = hardware.logical_cores >= 16 ? 6 : 4;
        selection.gpu_layers = 999;
    } else if (medium_capable) {
        selection.profile_name = gpu_enabled ? hardware.gpu_backend + "-medium" : "cpu-medium";
        selection.model_path = choose_model(config, "medium");
        selection.context_size = 4096;
        selection.parallel_slots = hardware.logical_cores >= 8 ? 4 : 2;
        selection.gpu_layers = gpu_enabled ? 999 : 0;
    } else {
        selection.profile_name = "cpu-small";
        selection.model_path = choose_model(config, "small");
        selection.context_size = 2048;
        selection.parallel_slots = hardware.logical_cores >= 4 ? 2 : 1;
        selection.gpu_layers = 0;
    }

    if (!config.auto_profile) {
        if (!config.llama_binary.empty()) {
            selection.llama_binary = config.llama_binary;
        }
        if (!config.model_path.empty()) {
            selection.model_path = config.model_path;
        }
        selection.context_size = config.context_size;
        selection.parallel_slots = config.parallel_slots;
        selection.gpu_layers = config.gpu_layers;
        selection.profile_name = "manual";
    }

    if (selection.model_path.empty()) {
        selection.model_path = config.model_path;
    }
    if (selection.llama_binary.empty()) {
        selection.llama_binary = config.llama_binary;
    }

    return selection;
}
