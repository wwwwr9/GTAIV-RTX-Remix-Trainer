#include "config.h"

#include "logger.h"

namespace remix {
namespace {

constexpr const char* kDefaultPresets =
    "POLICE,TAXI,INFERNUS,TURISMO,SULTANRS,COMET,COGNOSCENTI,BUFFALO,AMBULANCE,FBI,"
    "FIRETRUK,NOOSE,NRG900,PCJ,ANNIHILATOR";

void WriteDefaultIni(const std::wstring& path) {
    const char* text =
        "; Remix Trainer configuration\r\n"
        "; Values in [Keys] are Windows Virtual-Key codes in decimal.\r\n"
        "\r\n"
        "[Keys]\r\n"
        "ToggleMenu=112\r\n"
        "Up=104\r\n"
        "Down=98\r\n"
        "Left=100\r\n"
        "Right=102\r\n"
        "Select=101\r\n"
        "SelectAlt=13\r\n"
        "Back=96\r\n"
        "BackAlt=8\r\n"
        "\r\n"
        "[Vehicles]\r\n"
        "Presets=POLICE,TAXI,INFERNUS,TURISMO,SULTANRS,COMET,COGNOSCENTI,BUFFALO,AMBULANCE,FBI,FIRETRUK,NOOSE,NRG900,PCJ,ANNIHILATOR\r\n";

    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
    CloseHandle(file);
}

std::vector<std::string> ParseCsv(const std::wstring& wide) {
    std::vector<std::string> result;
    std::string utf8 = WideToUtf8(wide);
    size_t start = 0;
    while (start <= utf8.size()) {
        const size_t comma = utf8.find(',', start);
        std::string item = Trim(utf8.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        item = ToUpperAscii(item);
        if (!item.empty()) {
            result.push_back(item);
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return result;
}

}  // namespace

Config LoadConfig(const std::wstring& iniPath) {
    if (GetFileAttributesW(iniPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        WriteDefaultIni(iniPath);
    }

    Config cfg;
    cfg.keys.toggleMenu = GetPrivateProfileIntW(L"Keys", L"ToggleMenu", cfg.keys.toggleMenu, iniPath.c_str());
    cfg.keys.up = GetPrivateProfileIntW(L"Keys", L"Up", cfg.keys.up, iniPath.c_str());
    cfg.keys.down = GetPrivateProfileIntW(L"Keys", L"Down", cfg.keys.down, iniPath.c_str());
    cfg.keys.left = GetPrivateProfileIntW(L"Keys", L"Left", cfg.keys.left, iniPath.c_str());
    cfg.keys.right = GetPrivateProfileIntW(L"Keys", L"Right", cfg.keys.right, iniPath.c_str());
    cfg.keys.select = GetPrivateProfileIntW(L"Keys", L"Select", cfg.keys.select, iniPath.c_str());
    cfg.keys.selectAlt = GetPrivateProfileIntW(L"Keys", L"SelectAlt", cfg.keys.selectAlt, iniPath.c_str());
    cfg.keys.back = GetPrivateProfileIntW(L"Keys", L"Back", cfg.keys.back, iniPath.c_str());
    cfg.keys.backAlt = GetPrivateProfileIntW(L"Keys", L"BackAlt", cfg.keys.backAlt, iniPath.c_str());

    wchar_t presets[2048]{};
    GetPrivateProfileStringW(L"Vehicles", L"Presets", L"", presets, static_cast<DWORD>(std::size(presets)),
                             iniPath.c_str());
    cfg.vehiclePresets = ParseCsv(presets[0] ? presets : std::wstring(kDefaultPresets, kDefaultPresets + strlen(kDefaultPresets)));
    if (cfg.vehiclePresets.empty()) {
        cfg.vehiclePresets = ParseCsv(std::wstring(kDefaultPresets, kDefaultPresets + strlen(kDefaultPresets)));
    }

    log::Info("Loaded config from %s", WideToUtf8(iniPath).c_str());
    return cfg;
}

}  // namespace remix

