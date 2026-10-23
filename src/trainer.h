#pragma once

#include "common.h"
#include "config.h"

namespace remix {

class Trainer {
public:
    static Trainer& Instance();

    void Configure(Config config);
    void Tick();

private:
    enum class Page {
        Main,
        Weather,
        Time,
        Vehicles,
        VehicleInput,
        Presets,
        History,
    };

    struct PendingSpawn {
        bool active = false;
        std::string model;
        u32 hash = 0;
        DWORD startedAt = 0;
        DWORD lastRequestAt = 0;
    };

    Trainer() = default;

    void UpdateInput();
    void HandlePageInput();
    void HandleVehicleTextInput();
    bool IsMenuKey(int vk) const;

    void SetPage(Page page);
    void MoveSelection(int direction, int itemCount);
    bool KeyPressed(int vk);
    bool UpPressed();
    bool DownPressed();
    bool LeftPressed();
    bool RightPressed();
    bool SelectPressed();
    bool BackPressed();

    void NextWeather(int delta);
    void ApplyWeather(bool forceNow);
    void ToggleWeatherLock();
    void UpdateWeatherFromGame();

    void ChangeHour(int delta);
    void ApplyTime(bool force);
    void ToggleTimeLock();
    void UpdateTimeFromGame();

    void RequestSpawn(std::string model);
    void ProcessPendingSpawn();
    void AddHistory(const std::string& model);
    void SetStatus(std::string text, DWORD durationMs = 3500);

    std::vector<std::string> BuildCurrentLines() const;
    int CurrentItemCount() const;

    Config config_{};
    std::array<bool, 256> previousKeys_{};
    bool menuOpen_ = false;
    Page page_ = Page::Main;
    int selected_ = 0;

    int weather_ = 0;
    bool weatherLocked_ = false;
    DWORD lastWeatherRead_ = 0;

    int hour_ = 12;
    int minute_ = 0;
    bool timeLocked_ = false;
    DWORD lastTimeRead_ = 0;

    std::string typedModel_ = "POLICE";
    std::vector<std::string> history_;
    PendingSpawn pendingSpawn_{};

    std::string status_;
    DWORD statusUntil_ = 0;
};

}  // namespace remix

