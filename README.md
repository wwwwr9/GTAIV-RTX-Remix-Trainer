# Remix Trainer for GTA IV

A lightweight trainer for **GTA IV: The Complete Edition** that works together with **RTX Remix**,
including the [GTA IV RTX Remix Compatibility Mod](https://github.com/xoxor4d/gta4-rtx).

Typical GTA IV trainers depend on ScriptHook, .NET script loaders or Direct3D hooks to run and draw their menus,
and these break under RTX Remix. Remix Trainer avoids all of that: it is a single `.asi` plugin with no runtime
dependencies, it does not touch Direct3D, and its menu is drawn by Windows in a separate transparent window above
the game, so nothing is added to the frames that RTX Remix captures and renders.

[Русская версия](README.ru.md)

## Features

- **Weather** — switch between all eight weather types instantly, lock the chosen weather or hand control back
  to the game.
- **Time** — change the hour, freeze the time of day or release it.
- **Vehicles** — spawn any vehicle by model name, from a configurable preset list or from the history of recently
  spawned vehicles. The vehicle appears a few metres in front of the player, facing the same direction.
- Configurable hotkeys and vehicle presets in a plain `.ini` file.
- A log file that reports exactly what was found in the game executable, for quick troubleshooting.

## Requirements

- GTA IV: The Complete Edition (32-bit `GTAIV.exe`).
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader).
  `d3d9.dll` is taken by RTX Remix, so install the loader under another name, for example `dinput8.dll`.
  If an ASI loader is already installed by another mod, nothing else is needed; the
  [GTA IV RTX Remix Compatibility Mod](https://github.com/xoxor4d/gta4-rtx) already includes one.
- The game running in **windowed** or **borderless windowed** mode. The menu lives in its own always-on-top
  window, and exclusive fullscreen can hide it.

RTX Remix itself is not required: the trainer works the same way without it.

## Installation

1. Download `remix_trainer.asi` from the [Releases](../../releases) page.
2. Put it in the GTA IV folder (next to `GTAIV.exe`), or in its `plugins` or `scripts` subfolder.
3. Start the game and load a save. Press **F1** to open the menu.

On first launch the trainer creates `remix_trainer.ini` next to the `.asi` file.

To uninstall, delete `remix_trainer.asi`, `remix_trainer.ini` and `remix_trainer.log`.

## Controls

Default keys (all of them can be changed in the `.ini` file):

| Action | Keys |
| --- | --- |
| Open / close the menu | `F1` |
| Move up / down | `Num 8` / `Num 2`, or `↑` / `↓` |
| Change the value on the first line of the Weather and Time pages | `Num 4` / `Num 6`, or `←` / `→` |
| Select | `Num 5` or `Enter` |
| Back (closes the menu on the main page) | `Num 0` or `Backspace` |

Keys are read while the menu is open, but they are not blocked from the game: if a key also has an in-game
action, the game reacts to it as well.

### Typing a vehicle model

**Vehicles → Edit model** opens the input line:

- letters `A`–`Z` and digits (top row and numpad) are typed; `-` types `_`;
- `Backspace` erases the last character, and on an empty line returns to the Vehicles page;
- `Enter` or `Num 5` spawns the typed model;
- `Esc` or `Num 0` returns without spawning, keeping the typed name.

Model names are up to 24 characters. Keys used for menu actions are not typed.

## Menu

**Weather**

| Item | Effect |
| --- | --- |
| Current | The weather in effect; `←` / `→` switch it |
| Next weather / Previous weather | Switch the weather immediately |
| Lock weather | Keeps the selected weather until unlocked |
| Release weather | Unlocks and returns weather control to the game |

Weather types: `EXTRASUNNY`, `SUNNY`, `SUNNY_WINDY`, `CLOUDY`, `RAIN`, `DRIZZLE`, `FOGGY`, `LIGHTNING`.

**Time**

| Item | Effect |
| --- | --- |
| Hour | The current game time; `←` / `→` or Select change the hour, minutes stay the same |
| Lock time | Freezes the clock at the shown time |
| Release time | Unfreezes the clock |

While weather or time are not locked, the menu follows the game's own values.

**Vehicles**

| Item | Effect |
| --- | --- |
| Edit model | Type a model name (see above) |
| Spawn typed model | Spawns the model from the input line |
| Preset models | The list from `[Vehicles] Presets` in the `.ini` |
| History | The last eight spawned models, newest first |

The result of every action is shown for a few seconds as a status line under the menu (or in the top-left corner
of the game window when the menu is closed): the chosen weather or time, `Spawned: <model>`, or an error such as `Model failed to load: <model>` when the name is not a valid model
or the model did not load within 8 seconds.

## Configuration

`remix_trainer.ini` is created next to `remix_trainer.asi`:

```ini
[Keys]
ToggleMenu=112
Up=104
Down=98
Left=100
Right=102
Select=101
SelectAlt=13
Back=96
BackAlt=8

[Vehicles]
Presets=POLICE,TAXI,INFERNUS,TURISMO,SULTANRS,COMET,COGNOSCENTI,BUFFALO,AMBULANCE,FBI,FIRETRUK,NOOSE,NRG900,PCJ,ANNIHILATOR
```

- `[Keys]` values are decimal
  [Windows virtual-key codes](https://learn.microsoft.com/windows/win32/inputdev/virtual-key-codes):
  `112` = F1, `104` = Num 8, `98` = Num 2, `100` = Num 4, `102` = Num 6, `101` = Num 5, `13` = Enter,
  `96` = Num 0, `8` = Backspace. The arrow keys always work in addition to `Up`, `Down`, `Left` and `Right`.
- `Presets` is a comma-separated list of vehicle model names in any letter case. If the list is empty, the
  default list above is used.

Changes take effect the next time the game starts. The key hints shown in the menu always list the default keys.

## Log

Every launch writes `remix_trainer.log` next to `GTAIV.exe` (and a copy next to the `.asi` if it is in another
folder). The log is overwritten on each start.

| Message | Meaning |
| --- | --- |
| `remix_trainer initialized` | Everything was found; the menu is available |
| `Game tick pattern not found` | This `GTAIV.exe` version is not supported |
| `Native not resolved: …` | A game function the trainer needs was not found; the trainer stays inactive |
| `Failed to create GDI menu overlay window` | Windows refused to create the menu window |

## Troubleshooting

- **No log file appears.** The `.asi` is not being loaded: check that Ultimate ASI Loader is installed under a
  name other than `d3d9.dll` and that the trainer is in the game folder, `plugins` or `scripts`.
- **The log ends with `remix_trainer initialized`, but the menu is not visible.** Switch the game to windowed or
  borderless windowed mode.
- **`Model failed to load`.** The name is not a GTA IV vehicle model. Use model names (`SULTANRS`, `NRG900`),
  not display names (`Sultan RS`, `NRG 900`).

## Building from source

Requirements: Visual Studio with the **Desktop development with C++** workload (MSVC and a Windows SDK).

Open `remix_trainer.sln`, choose `Release | Win32` and build, or run from a Developer PowerShell in the
repository folder:

```powershell
msbuild remix_trainer.sln /p:Configuration=Release /p:Platform=Win32
```

The result is `bin\Win32\Release\remix_trainer.asi`. The project must be built as 32-bit (`Win32`), because
GTA IV is a 32-bit game.

## How it works

- The plugin is a regular DLL with the `.asi` extension; Ultimate ASI Loader loads it into the game process.
- A short hook is placed on the game's per-frame update, found by a byte pattern in `GTAIV.exe`, so the trainer
  runs on the game's own thread.
- Game script functions (natives) are located at runtime by their hashes in the game's native registration
  code, so no fixed addresses are tied to a specific executable build. Every call is guarded, and a failure is
  written to the log instead of crashing the game.
- The menu is drawn with Windows GDI in a transparent, click-through, always-on-top window that follows the game
  window. The game's Direct3D 9 output is never touched.

## Source layout

| File | Purpose |
| --- | --- |
| `src/dllmain.cpp` | Entry point, initialization thread, game update hook |
| `src/trainer.cpp` | Menu pages, input, weather, time and vehicle logic |
| `src/overlay.cpp` | GDI menu window |
| `src/native_invoker.*` | Native lookup and calls |
| `src/memory.cpp` | Executable sections, pattern search, code patching |
| `src/config.cpp` | `.ini` loading and defaults |
| `src/logger.cpp` | Log file |

## License

[MIT](LICENSE).

Grand Theft Auto IV is a trademark of Take-Two Interactive Software. This project is not affiliated with or
endorsed by Take-Two Interactive, Rockstar Games, or the authors of RTX Remix and the GTA IV RTX Remix
Compatibility Mod.
