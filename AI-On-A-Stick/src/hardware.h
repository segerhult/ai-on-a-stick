#pragma once

#include <cstdint>
#include <string>

#include "config.h"
#include "machine.h"

struct HardwareProfile {
    std::string cpu_vendor;
    std::uint32_t logical_cores = 1;
    std::uint64_t memory_gb = 0;
    double memory_used_gb = 0.0;
    double memory_available_gb = 0.0;
    bool has_gpu = false;
    std::string gpu_backend = "cpu";
};

struct RuntimeSelection {
    std::string profile_name = "cpu-small";
    std::string llama_binary;
    std::string model_path;
    int context_size = 2048;
    int parallel_slots = 2;
    int gpu_layers = 0;
};

HardwareProfile detect_hardware(const MachineProfile& machine);
RuntimeSelection select_runtime_profile(const AppConfig& config, const MachineProfile& machine,
                                        const HardwareProfile& hardware);
