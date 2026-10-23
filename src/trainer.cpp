#include "trainer.h"

#include "logger.h"
#include "native_invoker.h"
#include "overlay.h"

#include <cmath>

namespace remix {
namespace {

constexpr const char* kWeatherNames[] = {
    "EXTRASUNNY",
    "SUNNY",
    "SUNNY_WINDY",
    "CLOUDY",
    "RAIN",
    "DRIZZLE",
    "FOGGY",
    "LIGHTNING",
};

std::string BoolText(bool value) {
    return value ? "ON" : "OFF";
}

std::string TwoDigit(int value) {
    char buffer[8]{};
    snprintf(buffer, sizeof(buffer), "%02d", value);
    return buffer;
}

std::string CleanModelName(std::string model) {
    model = ToUpperAscii(Trim(model));
    std::string cleaned;
    cleaned.reserve(model.size());
    for (char ch : model) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_') {
            cleaned.push_back(ch);
        }
    }
    return cleaned;
}

}  // namespace

Trainer& Trainer::Instance() {
    static Trainer trainer;
    return trainer;
}

void Trainer::Configure(Config config) {
    config_ = std::move(config);
}

void Trainer::Tick() {
    if (!native::IsReady()) {
        return;
    }

    UpdateInput();
    UpdateWeatherFromGame();
    UpdateTimeFromGame();

    if (weatherLocked_) {
        native::InvokeVoid("FORCE_WEATHER_NOW", weather_);
    }
    if (timeLocked_) {
        native::InvokeVoid("FORCE_TIME_OF_DAY", static_cast<u32>(hour_), static_cast<u32>(minute_));
    }

    ProcessPendingSpawn();

    overlay::SetMenuState(menuOpen_, BuildCurrentLines(), selected_, status_,
                          !status_.empty() && GetTickCount() < statusUntil_);
}

void Trainer::UpdateInput() {
    if (KeyPressed(config_.keys.toggleMenu)) {
        menuOpen_ = !menuOpen_;
        log::Info("Menu toggled: %s", menuOpen_ ? "open" : "closed");
        if (menuOpen_) {
            SetPage(Page::Main);
            SetStatus("Trainer menu opened", 1200);
        }
    }

    if (!menuOpen_) {
        return;
    }

    if (page_ == Page::VehicleInput) {
        HandleVehicleTextInput();
    } else {
        HandlePageInput();
    }
}

void Trainer::HandlePageInput() {
    const int count = CurrentItemCount();
    if (UpPressed()) {
        MoveSelection(-1, count);
    }
    if (DownPressed()) {
        MoveSelection(1, count);
    }

    if (BackPressed()) {
        if (page_ == Page::Main) {
            menuOpen_ = false;
        } else {
            SetPage(Page::Main);
        }
        return;
    }

    if (page_ == Page::Weather && selected_ == 0) {
        if (LeftPressed()) {
            NextWeather(-1);
        }
        if (RightPressed()) {
            NextWeather(1);
        }
    }

    if (page_ == Page::Time && selected_ == 0) {
        if (LeftPressed()) {
            ChangeHour(-1);
        }
        if (RightPressed()) {
            ChangeHour(1);
        }
    }

    if (!SelectPressed()) {
        return;
    }

    switch (page_) {
    case Page::Main:
        if (selected_ == 0) SetPage(Page::Weather);
        if (selected_ == 1) SetPage(Page::Time);
        if (selected_ == 2) SetPage(Page::Vehicles);
        if (selected_ == 3) menuOpen_ = false;
        break;
    case Page::Weather:
        if (selected_ == 0 || selected_ == 1) NextWeather(1);
        if (selected_ == 2) NextWeather(-1);
        if (selected_ == 3) ToggleWeatherLock();
        if (selected_ == 4) {
            weatherLocked_ = false;
            native::InvokeVoid("RELEASE_WEATHER");
            SetStatus("Weather released");
        }
        if (selected_ == 5) SetPage(Page::Main);
        break;
    case Page::Time:
        if (selected_ == 0) ChangeHour(1);
        if (selected_ == 1) ToggleTimeLock();
        if (selected_ == 2) {
            timeLocked_ = false;
            native::InvokeVoid("RELEASE_TIME_OF_DAY");
            SetStatus("Time released");
        }
        if (selected_ == 3) SetPage(Page::Main);
        break;
    case Page::Vehicles:
        if (selected_ == 0) SetPage(Page::VehicleInput);
        if (selected_ == 1) RequestSpawn(typedModel_);
        if (selected_ == 2) SetPage(Page::Presets);
        if (selected_ == 3) SetPage(Page::History);
        if (selected_ == 4) SetPage(Page::Main);
        break;
    case Page::Presets:
        if (selected_ >= 0 && selected_ < static_cast<int>(config_.vehiclePresets.size())) {
            RequestSpawn(config_.vehiclePresets[static_cast<size_t>(selected_)]);
            SetPage(Page::Vehicles);
        } else {
            SetPage(Page::Vehicles);
        }
        break;
    case Page::History:
        if (selected_ >= 0 && selected_ < static_cast<int>(history_.size())) {
            RequestSpawn(history_[static_cast<size_t>(selected_)]);
            SetPage(Page::Vehicles);
        } else {
            SetPage(Page::Vehicles);
        }
        break;
    case Page::VehicleInput:
        break;
    }
}

void Trainer::HandleVehicleTextInput() {
    for (int vk = 'A'; vk <= 'Z'; ++vk) {
        if (!IsMenuKey(vk) && KeyPressed(vk) && typedModel_.size() < 24) {
            typedModel_.push_back(static_cast<char>(vk));
        }
    }
    for (int vk = '0'; vk <= '9'; ++vk) {
        if (!IsMenuKey(vk) && KeyPressed(vk) && typedModel_.size() < 24) {
            typedModel_.push_back(static_cast<char>(vk));
        }
    }
    for (int vk = VK_NUMPAD0; vk <= VK_NUMPAD9; ++vk) {
        if (!IsMenuKey(vk) && KeyPressed(vk) && typedModel_.size() < 24) {
            typedModel_.push_back(static_cast<char>('0' + (vk - VK_NUMPAD0)));
        }
    }
    if (KeyPressed(VK_OEM_MINUS) && typedModel_.size() < 24) {
        typedModel_.push_back('_');
    }

    if (KeyPressed(VK_BACK)) {
        if (!typedModel_.empty()) {
            typedModel_.pop_back();
        } else {
            SetPage(Page::Vehicles);
        }
    }

    if (SelectPressed()) {
        typedModel_ = CleanModelName(typedModel_);
        if (typedModel_.empty()) {
            typedModel_ = "POLICE";
        }
        RequestSpawn(typedModel_);
        SetPage(Page::Vehicles);
    }

    if (KeyPressed(VK_ESCAPE) || KeyPressed(config_.keys.back)) {
        typedModel_ = CleanModelName(typedModel_);
        if (typedModel_.empty()) {
            typedModel_ = "POLICE";
        }
        SetPage(Page::Vehicles);
    }
}

bool Trainer::IsMenuKey(int vk) const {
    return vk == config_.keys.toggleMenu || vk == config_.keys.select ||
           vk == config_.keys.selectAlt || vk == config_.keys.back || vk == config_.keys.backAlt;
}

void Trainer::SetPage(Page page) {
    page_ = page;
    selected_ = 0;
}

void Trainer::MoveSelection(int direction, int itemCount) {
    if (itemCount <= 0) {
        selected_ = 0;
        return;
    }
    selected_ = (selected_ + direction + itemCount) % itemCount;
}

bool Trainer::KeyPressed(int vk) {
    if (vk < 0 || vk >= static_cast<int>(previousKeys_.size())) {
        return false;
    }
    const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
    const bool pressed = down && !previousKeys_[static_cast<size_t>(vk)];
    previousKeys_[static_cast<size_t>(vk)] = down;
    return pressed;
}

bool Trainer::UpPressed() {
    return KeyPressed(config_.keys.up) || KeyPressed(VK_UP);
}

bool Trainer::DownPressed() {
    return KeyPressed(config_.keys.down) || KeyPressed(VK_DOWN);
}

bool Trainer::LeftPressed() {
    return KeyPressed(config_.keys.left) || KeyPressed(VK_LEFT);
}

bool Trainer::RightPressed() {
    return KeyPressed(config_.keys.right) || KeyPressed(VK_RIGHT);
}

bool Trainer::SelectPressed() {
    return KeyPressed(config_.keys.select) || KeyPressed(config_.keys.selectAlt);
}

bool Trainer::BackPressed() {
    return KeyPressed(config_.keys.back) || KeyPressed(config_.keys.backAlt);
}

void Trainer::NextWeather(int delta) {
    weather_ = (weather_ + delta + 8) % 8;
    ApplyWeather(true);
}

void Trainer::ApplyWeather(bool forceNow) {
    if (forceNow) {
        native::InvokeVoid("FORCE_WEATHER_NOW", weather_);
    } else {
        native::InvokeVoid("FORCE_WEATHER", weather_);
    }
    SetStatus(std::string("Weather: ") + kWeatherNames[weather_]);
}

void Trainer::ToggleWeatherLock() {
    weatherLocked_ = !weatherLocked_;
    if (weatherLocked_) {
        ApplyWeather(true);
        SetStatus("Weather lock ON");
    } else {
        native::InvokeVoid("RELEASE_WEATHER");
        SetStatus("Weather lock OFF");
    }
}

void Trainer::UpdateWeatherFromGame() {
    const DWORD now = GetTickCount();
    if (weatherLocked_ || now - lastWeatherRead_ < 1000) {
        return;
    }
    lastWeatherRead_ = now;
    int current = weather_;
    native::InvokeVoid("GET_CURRENT_WEATHER", &current);
    if (current >= 0 && current < 8) {
        weather_ = current;
    }
}

void Trainer::ChangeHour(int delta) {
    hour_ = (hour_ + delta + 24) % 24;
    ApplyTime(timeLocked_);
}

void Trainer::ApplyTime(bool force) {
    if (force) {
        native::InvokeVoid("FORCE_TIME_OF_DAY", static_cast<u32>(hour_), static_cast<u32>(minute_));
    } else {
        native::InvokeVoid("SET_TIME_OF_DAY", static_cast<u32>(hour_), static_cast<u32>(minute_));
    }
    SetStatus("Time: " + TwoDigit(hour_) + ":" + TwoDigit(minute_));
}

void Trainer::ToggleTimeLock() {
    timeLocked_ = !timeLocked_;
    if (timeLocked_) {
        ApplyTime(true);
        SetStatus("Time lock ON");
    } else {
        native::InvokeVoid("RELEASE_TIME_OF_DAY");
        SetStatus("Time lock OFF");
    }
}

void Trainer::UpdateTimeFromGame() {
    const DWORD now = GetTickCount();
    if (timeLocked_ || now - lastTimeRead_ < 1000) {
        return;
    }
    lastTimeRead_ = now;
    u32 h = static_cast<u32>(hour_);
    u32 m = static_cast<u32>(minute_);
    native::InvokeVoid("GET_TIME_OF_DAY", &h, &m);
    if (h < 24 && m < 60) {
        hour_ = static_cast<int>(h);
        minute_ = static_cast<int>(m);
    }
}

void Trainer::RequestSpawn(std::string model) {
    model = CleanModelName(std::move(model));
    if (model.empty()) {
        SetStatus("Vehicle model is empty");
        return;
    }

    typedModel_ = model;
    pendingSpawn_.active = true;
    pendingSpawn_.model = model;
    pendingSpawn_.hash = native::Joaat(model.c_str());
    pendingSpawn_.startedAt = GetTickCount();
    pendingSpawn_.lastRequestAt = 0;
    SetStatus("Loading model: " + model);
    log::Info("Spawn requested: %s hash 0x%08X", model.c_str(), pendingSpawn_.hash);
}

void Trainer::ProcessPendingSpawn() {
    if (!pendingSpawn_.active) {
        return;
    }

    const DWORD now = GetTickCount();
    if (now - pendingSpawn_.startedAt > 8000) {
        SetStatus("Model failed to load: " + pendingSpawn_.model, 5000);
        log::Error("Model load timed out: %s hash 0x%08X", pendingSpawn_.model.c_str(), pendingSpawn_.hash);
        pendingSpawn_.active = false;
        return;
    }

    if (now - pendingSpawn_.lastRequestAt > 250) {
        native::InvokeVoid("REQUEST_MODEL", pendingSpawn_.hash);
        pendingSpawn_.lastRequestAt = now;
    }

    if (!native::Invoke<int>("HAS_MODEL_LOADED", pendingSpawn_.hash)) {
        return;
    }

    u32 player = native::Invoke<u32>("GET_PLAYER_ID");
    int ped = 0;
    native::InvokeVoid("GET_PLAYER_CHAR", player, &ped);
    if (!ped) {
        SetStatus("Player ped unavailable");
        pendingSpawn_.active = false;
        return;
    }

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float heading = 0.0f;
    native::InvokeVoid("GET_OFFSET_FROM_CHAR_IN_WORLD_COORDS", ped, 0.0f, 7.0f, 0.25f, &x, &y, &z);
    native::InvokeVoid("GET_CHAR_HEADING", ped, &heading);

    int vehicle = 0;
    native::InvokeVoid("CREATE_CAR", pendingSpawn_.hash, x, y, z, &vehicle, 1);
    if (vehicle) {
        native::InvokeVoid("SET_CAR_HEADING", vehicle, heading);
        native::InvokeVoid("SET_CAR_ON_GROUND_PROPERLY", vehicle);
        native::InvokeVoid("MARK_MODEL_AS_NO_LONGER_NEEDED", pendingSpawn_.hash);
        AddHistory(pendingSpawn_.model);
        SetStatus("Spawned: " + pendingSpawn_.model);
        log::Info("Spawned vehicle: %s handle %d", pendingSpawn_.model.c_str(), vehicle);
    } else {
        SetStatus("CREATE_CAR failed: " + pendingSpawn_.model);
        log::Error("CREATE_CAR returned null handle for %s", pendingSpawn_.model.c_str());
    }

    pendingSpawn_.active = false;
}

void Trainer::AddHistory(const std::string& model) {
    history_.erase(std::remove(history_.begin(), history_.end(), model), history_.end());
    history_.insert(history_.begin(), model);
    if (history_.size() > 8) {
        history_.resize(8);
    }
}

void Trainer::SetStatus(std::string text, DWORD durationMs) {
    status_ = std::move(text);
    statusUntil_ = GetTickCount() + durationMs;
}

std::vector<std::string> Trainer::BuildCurrentLines() const {
    switch (page_) {
    case Page::Main:
        return {"Weather", "Time", "Vehicles", "Close"};
    case Page::Weather:
        return {
            std::string("Current: ") + kWeatherNames[weather_] + "  < / >",
            "Next weather",
            "Previous weather",
            "Lock weather: " + BoolText(weatherLocked_),
            "Release weather",
            "Back",
        };
    case Page::Time:
        return {
            "Hour: " + TwoDigit(hour_) + ":" + TwoDigit(minute_) + "  < / >",
            "Lock time: " + BoolText(timeLocked_),
            "Release time",
            "Back",
        };
    case Page::Vehicles:
        return {
            "Edit model: " + typedModel_,
            "Spawn typed model",
            "Preset models",
            history_.empty() ? "History (empty)" : "History",
            "Back",
        };
    case Page::VehicleInput:
        return {
            "Type model name:",
            typedModel_.empty() ? std::string("_") : typedModel_ + "_",
            "Enter/Num5: spawn | Backspace: erase | Num0/Esc: cancel",
        };
    case Page::Presets: {
        std::vector<std::string> lines = config_.vehiclePresets;
        lines.push_back("Back");
        return lines;
    }
    case Page::History: {
        std::vector<std::string> lines = history_;
        if (lines.empty()) {
            lines.push_back("(no spawned vehicles yet)");
        }
        lines.push_back("Back");
        return lines;
    }
    }
    return {};
}

int Trainer::CurrentItemCount() const {
    return static_cast<int>(BuildCurrentLines().size());
}

}  // namespace remix
