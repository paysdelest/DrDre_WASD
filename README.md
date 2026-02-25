# DrDre_WASD v2.0

> **Based on [HallJoy by PashOK7](https://github.com/PashOK7/HallJoy)** — significantly extended with a full Custom Macro system, analog keyboard stability fixes, and advanced gaming features.

DrDre_WASD is a Windows desktop application that transforms a **Hall Effect analog keyboard** into one or more virtual Xbox 360 controllers — with an advanced Custom Macro system on top.

It reads per-key analog values via the Wooting Analog SDK and publishes XInput-compatible gamepads via ViGEmBus.

📺 **Original HallJoy video overview**: [YouTube](https://youtu.be/MI_ZTS6UFhM?si=Cpn9DY95S9no9ncJ)

---

## 🆕 What's new in v2.0

- ✅ **Custom Macro tab** — free-trigger macro editor replacing the old Combo/Macro tab, fully redesigned with a premium dark UI
- ✅ **Enable / Disable macros** — toggle any macro on or off via the UI button or right-click on the list, without deleting it
- ✅ **Mouse combo triggers** — right-click held + left click, middle click combos, scroll wheel combos, double-click, simultaneous clicks
- ✅ **Emergency stop** — `Ctrl+Shift+Alt+F12` kills all running macros instantly
- ✅ **abiv1.dll deadlock fix** — resolved a crash after ~4 hours of runtime caused by a mutex deadlock in `unload()` inside the universal-analog-plugin
- ✅ **Thread-safe analog backend** — mutex-protected Wooting analog reads, atomic state management
- ✅ **Improved shutdown sequence** — proper hook cleanup on exit to prevent crashes
- ✅ **PreBuildEvent** — automatically copies Wooting SDK DLLs from `runtime\` to the output directory at build time

---

## ✨ Key Features

- Analog keyboard → virtual gamepad bridge with real-time updates
- Up to 4 virtual gamepads simultaneously
- Full remap UI for sticks, triggers, ABXY, bumpers, D-pad, Start/Back/Home
- Advanced per-key curve and deadzone tuning
- Last Key Priority and Snap Stick options
- Optional block of physical key output when bound to gamepad input
- Keyboard layout editor (move/add/remove keys, labels, HID codes, sizes, positions)
- Custom layout presets with fast switching
- **Full Custom Macro system** (see below)
- Settings saved next to the executable

---

## ⌨️ Keyboard Support

DrDre_WASD uses:
- [Wooting Analog SDK](https://github.com/WootingKb/wooting-analog-sdk)
- [Universal Analog Plugin](https://github.com/AnalogSense/universal-analog-plugin)

This means it works with many HE keyboards supported by that stack — not only Wooting.

**Aula Keyboards**: Experimental support is available but limited. Proper native support would require firmware-level changes or direct help from Aula firmware developers.

---

## 📦 DLL Installation — REQUIRED

The repository includes pre-compiled `abiv0.dll` and `abiv1.dll` from the universal-analog-plugin, **with the deadlock fix applied** (stable 24h+ runtime).

⚠️ **You MUST place the DLL files in this exact folder:** `C:\Program Files\WootingAnalogPlugins\`

Create the folder if it does not exist. Without this step, the analog keyboard will not be detected.

| File | Destination |
|------|------------|
| `abiv0.dll` | `C:\Program Files\WootingAnalogPlugins\abiv0.dll` |
| `abiv1.dll` | `C:\Program Files\WootingAnalogPlugins\abiv1.dll` |

> 💡 If you prefer to compile the DLL yourself, the source is at [universal-analog-plugin](https://github.com/calamity-inc/universal-analog-plugin). Apply the `unload()` fix described in the Technical Details section below.

---

## 🚀 Getting Started

1. Copy `abiv0.dll` and `abiv1.dll` to `C:\Program Files\WootingAnalogPlugins\`
2. Install [ViGEmBus](https://github.com/ViGEm/ViGEmBus)
3. Install [Wooting Analog SDK](https://github.com/WootingKb/wooting-analog-sdk)
4. Run `DrDre_WASD.exe`
5. Select or create a keyboard layout
6. Map keys to gamepad controls in the **Remap** tab
7. Adjust curves and behaviour in **Configuration**
8. Create and manage macros in the **Custom Macro** tab

> If dependencies are missing, the app may offer to download/install them automatically.

---

## 🎬 Custom Macro Tab — Full Guide

The **Custom Macro** tab lets you bind complex action sequences to any **mouse or keyboard trigger combination**, fired automatically during gameplay — no dedicated macro keys needed.

### Creating a Macro

1. Click **`+ New`** to create a new macro entry
2. Give it a name in the **Combo Name** field
3. Click **`● Capture trigger`** — press your desired key or mouse button combination (1 or 2 inputs)
4. Build your action sequence in the **Actions** card
5. Click **`💾 Save combo`** to confirm

### Enabling / Disabling a Macro

- **Click the checkbox** `Combo enabled` in the Options card, then Save — or
- **Right-click** the macro in the list for an instant toggle

The left list shows a **green dot** (active) or **grey dot** (inactive) next to each macro at a glance.

### Available Trigger Types

#### Mouse triggers
| Trigger | Description |
|---------|-------------|
| Right-click held + Left click | Fire while aiming (ADS + shoot) |
| Left-click held + Right click | Alternative ADS combo |
| Middle-click held + Left click | Custom utility |
| Middle-click held + Right click | Custom utility |
| Double left click | Fast double-tap action |
| Double right click | Fast double-tap action |
| Left + Right click simultaneously | Two-button safety combo |
| Scroll up + Right click held | Scope cycle while aiming |
| Scroll down + Right click held | Reload cancel / weapon swap |

#### Keyboard triggers
Any single key or modifier + key combination captured via the **Capture trigger** button.

### Action Types

Each macro is a sequence of steps — mix and match:

| Action | Icon | Description |
|--------|------|-------------|
| Press key | 🔵 | Hold a key down until a Release action |
| Release key | ⚫ | Release a previously held key |
| Tap key | 🟢 | Instant press + release |
| Type text | 🟡 | Type a full string of characters |
| Mouse click | 🟣 | Left, right, or middle click |
| Wait (ms) | ⬜ | Pause between actions |

### Options

| Option | Description |
|--------|-------------|
| Combo enabled | Activate or deactivate without deleting |
| Repeat while held | Keep firing the sequence as long as the trigger is held |
| Repeat delay | Milliseconds between each repetition |

---

## 🎮 Gaming Use Cases

### 🎯 FPS (Valorant, CS2, Apex, Warzone...)

The most powerful use: bind complex mechanics to your **existing mouse buttons** — no extra keys required.

| Trigger | Action sequence | Result |
|---------|----------------|--------|
| Right-click held + Left click | Tap `P` | Ping an enemy while staying in ADS |
| Double left click | Press `Shift` + Tap `W` + Release `Shift` | Instant sprint burst |
| Left + Right simultaneously | Tap `G` | Throw grenade with two-button safety |
| Scroll down + Right click held | Tap `1` + Wait 100ms + Tap `1` | Reload cancel / fast weapon swap |
| Middle click + Left click | Tap `E` | Interact / loot while shooting |

### 🏗️ Building Games (Fortnite, Minecraft, Valheim...)

Bind build/edit/material cycles to mouse combos and free your keyboard fingers.

| Trigger | Action sequence | Result |
|---------|----------------|--------|
| Right-click held + Left click | Tap `F1` + Wait 50ms + Tap `F1` | Instant build confirm |
| Triple left click | Tap `Q` + Wait 30ms + Tap `E` + Wait 30ms + Tap `R` | Rapid material cycle |
| Middle click + Left click | Tap `Z` + Wait 200ms + Tap `Z` | Build → edit → build loop |
| Scroll up + Right click held | Tap `F2` | Switch to wall instantly while aiming |

### ⚔️ MMO / ARPG (WoW, Final Fantasy XIV, Path of Exile, Diablo...)

Chain skill rotations to a single trigger. No more fumbling across the keyboard mid-fight.

| Trigger | Action sequence | Result |
|---------|----------------|--------|
| Right-click held + Left click | Tap `1` + Wait 100ms + Tap `2` + Wait 100ms + Tap `3` | 3-skill opener while targeting |
| Scroll up + Right click held | Tap `4` + Wait 50ms + Tap `5` | Burst combo from scroll |
| Double right click | Tap `F` + Wait 500ms + Tap `F` | Charge / detonate pattern |
| Middle click + Right click | Tap `6` + Wait 200ms + Tap `7` + Wait 200ms + Tap `8` | Defensive cooldown chain |

### 🗺️ Adventure / Open World (Elden Ring, Zelda, RDR2, GTA...)

Automate sequences that normally require split-second timing.

| Trigger | Action sequence | Result |
|---------|----------------|--------|
| Double left click | Tap `X` + Wait 80ms + Tap `X` | Double dodge / roll |
| Right-click held + Left click | Tap `C` + Wait 100ms + Tap `V` | Block then parry |
| Scroll down + Right click held | Tap `H` + Wait 300ms + Tap `H` | Horse call + gallop |
| Middle click + Left click | Press `Shift` + Tap `F` + Release `Shift` | Power attack |

### 💬 Text Macros — Type Lines / Code at Speed

The **Type text** action turns any macro into a **text expander**. Fire a full sentence, code snippet, or command with a single trigger.

| Trigger | Action sequence | Result |
|---------|----------------|--------|
| Any keyboard shortcut | Type text `gg well played, thanks for the game!` | GG message in one keystroke |
| Any keyboard shortcut | Type text `sudo apt update && sudo apt upgrade -y` | Full terminal command instantly |
| Any keyboard shortcut | Type text `console.log('DEBUG:', JSON.stringify(data, null, 2));` | Code snippet in any editor |
| Any keyboard shortcut | Type text + Wait 200ms + Tap `Enter` | Auto-submit a form / command |
| Repeat while held | Type text `#` | Fill a line with characters for formatting |

> **Tip**: combine **Type text** with **Wait** and **Tap Enter** to auto-send chat messages, commands, or multi-line snippets in any application.

---

## 🖥️ Interface Tabs

| Tab | Description |
|-----|-------------|
| **Remap** | Drag and drop gamepad buttons onto keyboard keys, up to 4 virtual gamepads |
| **Configuration** | Per-key analog curve editor (Linear, Segments, Custom), deadzone, activation point |
| **Custom Macro** | Free-trigger macro editor with full action sequencer, enable/disable toggle, right-click menu |
| **Gamepad Tester** | Real-time monitor of all virtual gamepad axes and button states |
| **Global Settings** | Keyboard layout, polling rate (1ms default), UI refresh interval |

---

## 🔧 Build

1. Open `HallJoy.sln` in Visual Studio 2022
2. Select `Release | x64`
3. Build — the PreBuildEvent automatically copies `wooting_analog_sdk.dll` and `wooting_analog_wrapper.dll` from `runtime\` to the output directory

> ⚠️ **Note**: `wooting_analog_sdk.dll` and `wooting_analog_wrapper.dll` are included in the `runtime\` folder of this repository but are not bundled in the source zip.
> Download them directly from the [runtime folder](https://github.com/paysdelest/DrDre_WASD/tree/main/runtime)
> or from the [Wooting Analog SDK releases](https://github.com/WootingKb/wooting-analog-sdk/releases)
> and place them in the `runtime\` folder before building.

---

## 🔩 Technical Details — abiv1.dll Deadlock Fix

A critical bug was identified and fixed in the **universal-analog-plugin** (`abiv1.dll`). After ~4 hours of runtime, the plugin crashed with `0xc0000005 INVALID_POINTER_READ` inside `unload()`.

**Root cause**: `awaitCompletion()` was called while `devices_mtx` was still held. The internal device thread would then call `discover_devices()`, which also tries to acquire `devices_mtx` — deadlock. Windows force-terminated the thread, corrupting internal pointers.

**Fix**: split `unload()` into two passes — `cancelReceiveReport()` with lock held, then `awaitCompletion()` after releasing the lock. The fixed DLL runs stably for 12h+.

A fix has been submitted upstream to [universal-analog-plugin](https://github.com/calamity-inc/universal-analog-plugin).

---

## 🛠️ Troubleshooting

- **All analog values at 0** — check your keyboard firmware mode. Some keyboards disable analog SDK output when **Turbo mode** is enabled. Disable it and restart.
- **Analog stops working after a plugin update** — reinstall the DLL files from this repo and keep only one plugin variant in `C:\Program Files\WootingAnalogPlugins\`
- **Macro not firing** — check if emergency stop was triggered (`Ctrl+Shift+Alt+F12`), or verify the macro is enabled (green dot in the list)
- **Macro enabled but not triggering** — ensure the trigger combination is not captured by another application or system shortcut
- **Controls not resizing properly** — make sure you are using v2.0 or later; earlier builds had a WM_SIZE layout bug when the tab was hidden

### Enable Logging (for debugging)

By default, logging is disabled to preserve performance.
To enable it, open `settings.ini` and set:

```ini
[Main]
Logging=1
```

A file `HallJoy_log.txt` will be created next to the executable.
Set back to `Logging=0` (or remove the line) once done.

### Rollback Wooting SDK (if needed)

If a newer Wooting SDK version causes unstable input or flickering:

```powershell
# List available tags
powershell -ExecutionPolicy Bypass -File .\tools\rollback-wooting-sdk.ps1 -ListOnly

# Install a specific tag (example v0.8.0)
powershell -ExecutionPolicy Bypass -File .\tools\rollback-wooting-sdk.ps1 -Tag v0.8.0
```

Then rebuild in VS (`Release | x64`) and run again. The script updates DLLs in `runtime\`, `x64\Release\`, and `x64\Debug\`, and creates automatic backups under `runtime\backup\`.

---

## 📁 Configuration Files

Stored next to the executable:

| File / Folder | Content |
|--------------|---------|
| `settings.ini` | Global settings |
| `bindings.ini` | Key-to-gamepad bindings |
| `Layouts/` | Keyboard layout presets (1 file = 1 preset) |
| `CurvePresets/` | Curve preset files |
| `free_combos.dat` | Custom Macro configurations |

---

## 📋 Requirements

- Windows 10/11 (x64)
- [ViGEmBus](https://github.com/ViGEm/ViGEmBus)
- [Wooting Analog SDK](https://github.com/WootingKb/wooting-analog-sdk)
- `abiv0.dll` + `abiv1.dll` in `C:\Program Files\WootingAnalogPlugins\`

---

## 🙏 Credits

- **[PashOK7](https://github.com/PashOK7/HallJoy)** — original HallJoy project
- **[calamity-inc](https://github.com/calamity-inc/universal-analog-plugin)** — universal-analog-plugin (Soup / abiv1.dll)
- **[ViGEmClient](https://github.com/nefarius/ViGEmClient)** — virtual gamepad emulation
- **[Wooting](https://github.com/WootingKb/wooting-analog-sdk)** — Analog SDK

---

## 📄 License

MIT — free to use, modify, and redistribute. See [LICENSE](LICENSE).

---

*DrDre_WASD v2.0 — Precision Input Automation for Hall Effect Keyboards*
