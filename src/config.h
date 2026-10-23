#pragma once

#include "common.h"

namespace remix {

struct KeyConfig {
    int toggleMenu = VK_F1;
    int up = VK_NUMPAD8;
    int down = VK_NUMPAD2;
    int left = VK_NUMPAD4;
    int right = VK_NUMPAD6;
    int select = VK_NUMPAD5;
    int selectAlt = VK_RETURN;
    int back = VK_NUMPAD0;
    int backAlt = VK_BACK;
};

struct Config {
    KeyConfig keys;
    std::vector<std::string> vehiclePresets;
};

Config LoadConfig(const std::wstring& iniPath);

}  // namespace remix

