#include "machine.h"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {
std::string fnv1a_hex(const std::string& value) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : value) {
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= 1099511628211ULL;
    }

    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << hash;
    return stream.str();
}

std::string detect_hostname() {
    char buffer[256] = {};

#if defined(_WIN32)
    DWORD size = sizeof(buffer);
    if (GetComputerNameA(buffer, &size) != 0) {
        return std::string(buffer, size);
    }
#else
    if (gethostname(buffer, sizeof(buffer) - 1) == 0) {
        return buffer;
    }
#endif

    return "unknown-host";
}

std::string detect_os() {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown-os";
#endif
}

std::string detect_arch() {
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x64";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#else
    return "unknown-arch";
#endif
}
}  // namespace

MachineProfile detect_machine() {
    MachineProfile machine;
    machine.hostname = detect_hostname();
    machine.operating_system = detect_os();
    machine.architecture = detect_arch();
    machine.id = fnv1a_hex(machine.hostname + "|" + machine.operating_system + "|" + machine.architecture);
    return machine;
}
