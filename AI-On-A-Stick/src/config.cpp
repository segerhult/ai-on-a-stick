#include "config.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
namespace fs = std::filesystem;

std::string trim(std::string value) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

bool parse_bool(const std::string& value) {
    const std::string lowered = [&value]() {
        std::string output = value;
        std::transform(output.begin(), output.end(), output.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return output;
    }();

    return lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on";
}

std::vector<std::string> split_paths(const std::string& value) {
    std::vector<std::string> paths;
    std::stringstream stream(value);
    std::string item;

    while (std::getline(stream, item, ';')) {
        item = trim(item);
        if (!item.empty()) {
            paths.push_back(item);
        }
    }

    return paths;
}

std::string resolve_relative_path(const std::string& usb_root, const std::string& value) {
    if (value.empty()) {
        return "";
    }
    return (fs::path(usb_root) / value).string();
}
}  // namespace

AppConfig load_config(const std::string& usb_root) {
    AppConfig config;
    config.usb_root = usb_root;
    config.ui_root = (fs::path(usb_root) / "ui").string();
    config.data_root = (fs::path(usb_root) / "data").string();
    config.logs_root = (fs::path(usb_root) / "logs").string();

    const auto config_path = fs::path(usb_root) / "config" / "runtime.ini";
    if (!fs::exists(config_path)) {
        return config;
    }

    std::ifstream input(config_path);
    if (!input) {
        throw std::runtime_error("Unable to open config file: " + config_path.string());
    }

    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const std::string key = trim(line.substr(0, separator));
        const std::string value = trim(line.substr(separator + 1));

        if (key == "llama_binary") {
            config.llama_binary = resolve_relative_path(usb_root, value);
        } else if (key == "binary_macos_arm64") {
            config.binary_macos_arm64 = resolve_relative_path(usb_root, value);
        } else if (key == "binary_macos_x64") {
            config.binary_macos_x64 = resolve_relative_path(usb_root, value);
        } else if (key == "binary_linux_x64") {
            config.binary_linux_x64 = resolve_relative_path(usb_root, value);
        } else if (key == "binary_linux_arm64") {
            config.binary_linux_arm64 = resolve_relative_path(usb_root, value);
        } else if (key == "binary_windows_x64") {
            config.binary_windows_x64 = resolve_relative_path(usb_root, value);
        } else if (key == "model_path") {
            config.model_path = resolve_relative_path(usb_root, value);
        } else if (key == "model_small_path") {
            config.model_small_path = resolve_relative_path(usb_root, value);
        } else if (key == "model_medium_path") {
            config.model_medium_path = resolve_relative_path(usb_root, value);
        } else if (key == "model_large_path") {
            config.model_large_path = resolve_relative_path(usb_root, value);
        } else if (key == "model_coder_path") {
            config.model_coder_path = resolve_relative_path(usb_root, value);
        } else if (key == "llama_host") {
            config.llama_host = value;
        } else if (key == "llama_port") {
            config.llama_port = std::stoi(value);
        } else if (key == "coder_llama_port") {
            config.coder_llama_port = std::stoi(value);
        } else if (key == "app_port") {
            config.app_port = std::stoi(value);
        } else if (key == "context_size") {
            config.context_size = std::stoi(value);
        } else if (key == "parallel_slots") {
            config.parallel_slots = std::stoi(value);
        } else if (key == "gpu_layers") {
            config.gpu_layers = std::stoi(value);
        } else if (key == "coder_context_size") {
            config.coder_context_size = std::stoi(value);
        } else if (key == "coder_parallel_slots") {
            config.coder_parallel_slots = std::stoi(value);
        } else if (key == "coder_gpu_layers") {
            config.coder_gpu_layers = std::stoi(value);
        } else if (key == "auto_profile") {
            config.auto_profile = parse_bool(value);
        } else if (key == "prefer_gpu") {
            config.prefer_gpu = parse_bool(value);
        } else if (key == "auto_open_browser") {
            config.auto_open_browser = parse_bool(value);
        } else if (key == "auto_start_llama") {
            config.auto_start_llama = parse_bool(value);
        } else if (key == "scan_paths") {
            config.scan_paths = split_paths(value);
        }
    }

    return config;
}
