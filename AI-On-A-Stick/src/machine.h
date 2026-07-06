#pragma once

#include <string>

struct MachineProfile {
    std::string id;
    std::string hostname;
    std::string operating_system;
    std::string architecture;
};

MachineProfile detect_machine();
