\# DrDre\_WASD



> \*\*Based on \[HallJoy by PashOK7](https://github.com/PashOK7/HallJoy)\*\* — significantly extended with a full Combo/Macro system, analog keyboard stability fixes, and advanced gaming features.



DrDre\_WASD is a Windows desktop application that transforms a \*\*Hall Effect analog keyboard\*\* into one or more virtual Xbox 360 controllers — with an advanced Combo/Macro system on top.



It reads per-key analog values via the Wooting Analog SDK and publishes XInput-compatible gamepads via ViGEmBus.



📺 \*\*Original HallJoy video overview\*\*: \[YouTube](https://youtu.be/MI\_ZTS6UFhM?si=Cpn9DY95S9no9ncJ)



---



\## 🆕 What's new compared to PashOK7/HallJoy



\- ✅ \*\*Full Combo/Macro system\*\* — trigger-based combos and recorded macros with JSON import/export

\- ✅ \*\*Mouse combo triggers\*\* — right-click held + left click, middle click combos, scroll wheel combos, double/triple click, simultaneous clicks

\- ✅ \*\*Emergency stop\*\* — `Ctrl+Shift+Alt+F12` kills all running macros/combos instantly

\- ✅ \*\*abiv1.dll deadlock fix\*\* — resolved a crash after ~4 hours of runtime caused by a mutex deadlock in `unload()` inside the universal-analog-plugin

\- ✅ \*\*Thread-safe analog backend\*\* — mutex-protected Wooting analog reads, atomic state management

\- ✅ \*\*Improved shutdown sequence\*\* — proper hook cleanup on exit to prevent crashes



---



\## ✨ Key Features



\- Analog keyboard → virtual gamepad bridge with real-time updates

\- Up to 4 virtual gamepads simultaneously

\- Full remap UI for sticks, triggers, ABXY, bumpers, D-pad, Start/Back/Home

\- Advanced per-key curve and deadzone tuning

\- Last Key Priority and Snap Stick options

\- Optional block of physical key output when bound to gamepad input

\- Keyboard layout editor (move/add/remove keys, labels, HID codes, sizes, positions)

\- Custom layout presets with fast switching

\- \*\*Full Combo/Macro system\*\* (see below)

\- Settings saved next to the executable



---



\## ⌨️ Keyboard Support



DrDre\_WASD uses:

\- \[Wooting Analog SDK](https://github.com/WootingKb/wooting-analog-sdk)

\- \[Universal Analog Plugin](https://github.com/AnalogSense/universal-analog-plugin)



This means it works with many HE keyboards supported by that stack — not only Wooting.



\*\*Aula Keyboards\*\*: Experimental support is available but limited. Proper native support would require firmware-level changes or direct help from Aula firmware developers.



---



\## 📦 DLL Installation — REQUIRED



The repository includes pre-compiled `abiv0.dll` and `abiv1.dll` from the universal-analog-plugin, \*\*with the deadlock fix applied\*\* (stable 12h+ runtime).



> ⚠️ \*\*You MUST place the DLL files in this exact folder:\*\*

>

> ```

> C:\\Program Files\\WootingAnalogPlugins\\

> ```

>

> Create the folder if it does not exist. Without this step, the analog keyboard will not be detected.



| File | Destination |

|------|------------|

| `abiv0.dll` | `C:\\Program Files\\WootingAnalogPlugins\\abiv0.dll` |

| `abiv1.dll` | `C:\\Program Files\\WootingAnalogPlugins\\abiv1.dll` |



> 💡 If you prefer to compile the DLL yourself, the source is at \[universal-analog-plugin](https://github.com/calamity-inc/universal-analog-plugin). Apply the `unload()` fix described in the Technical Details section below.



---



\## 🚀 Getting Started



1\. Copy `abiv0.dll` and `abiv1.dll` to `C:\\Program Files\\WootingAnalogPlugins\\`

2\. Install \[ViGEmBus](https://github.com/ViGEm/ViGEmBus)

3\. Install \[Wooting Analog SDK](https://github.com/WootingKb/wooting-analog-sdk)

4\. Run `HallJoy.exe` as administrator

5\. Select or create a keyboard layout

6\. Map keys to gamepad controls in the \*\*Remap\*\* tab

7\. Adjust curves and behaviour in \*\*Configuration\*\*

8\. Create combos or record macros in \*\*Combo/Macro\*\*



> If dependencies are missing, HallJoy may offer to download/install them automatically.



---



\## ⚡ Combo System



The Combo system lets you bind complex action sequences to specific \*\*mouse or keyboard trigger combinations\*\* that fire automatically during gameplay — no dedicated macro keys needed.



\### Available Trigger Types



| Trigger | Example use |

|--------|-------------|

| Right-click held + Left click | Fire while aiming (ADS + shoot) |

| Left-click held + Right click | Alternative ADS combo |

| Middle-click held + Left click | Custom utility |

| Middle-click held + Right click | Custom utility |

| Double left click | Fast double-tap action |

| Double right click | Fast double-tap action |

| Triple left click | Triple-tap sequence |

| Left + Right click simultaneously | Two-button safety combo |

| Scroll up + Right click held | Scope cycle while aiming |

| Scroll down + Right click held | Reload cancel / weapon swap |



\### Action Types



Each combo contains a sequence of:

\- \*\*Press key\*\* — hold a key down

\- \*\*Release key\*\* — release a held key

\- \*\*Tap key\*\* — press and release instantly

\- \*\*Type text\*\* — type a full string

\- \*\*Mouse click\*\* — left, right, or middle

\- \*\*Wait (ms)\*\* — delay between actions



\### Combo Options

\- \*\*Automatic repetition\*\* while trigger is held

\- \*\*Repeat delay\*\* in milliseconds

\- \*\*JSON Export/Import\*\* for sharing configurations



---



\## 🎮 Gaming Use Cases



\### FPS

\- \*\*Right-click held + Left click → Tap `P`\*\* — ping/mark an enemy while staying in ADS

\- \*\*Double left click → Press `Shift` + Tap `W` + Release `Shift`\*\* — instant sprint burst

\- \*\*Left + Right simultaneously → Tap `G`\*\* — throw grenade with two-button safety

\- \*\*Scroll down + Right click held → Tap `1` + Wait 100ms + Tap `1`\*\* — reload cancel



\### Building Games (Fortnite, Minecraft...)

\- \*\*Right-click held + Left click → Tap `F1` + Wait 50ms + Tap `F1`\*\* — instant build confirm

\- \*\*Triple left click → Tap `Q` + Wait 30ms + Tap `E` + Wait 30ms + Tap `R`\*\* — rapid material cycle

\- \*\*Middle click + Left click → Tap `Z` + Wait 200ms + Tap `Z`\*\* — build-edit-build loop



\### MMO / ARPG

\- \*\*Right-click held + Left click → Tap `1` + Wait 100ms + Tap `2` + Wait 100ms + Tap `3`\*\* — 3-skill opener while targeting

\- \*\*Scroll up + Right click held → Tap `4` + Wait 50ms + Tap `5`\*\* — burst combo from scroll

\- \*\*Double right click → Tap `F` + Wait 500ms + Tap `F`\*\* — charge / detonate pattern



---



\## 🎬 Macro System



\### Recording

1\. Click \*\*"+ New Macro"\*\* to create a new macro

2\. Click \*\*"Record"\*\* — all keyboard and mouse actions are captured with millisecond timestamps

3\. Perform your actions

4\. Click \*\*"Record"\*\* again to stop



\### Playback Controls

\- \*\*Play\*\* — execute the recorded sequence

\- \*\*Loop\*\* — continuous playback

\- \*\*Speed Control\*\* — 0.1x to 10x

\- \*\*Block Keys During Playback\*\* — prevent accidental input



\### Features

\- Visual action list with full timestamps

\- Emergency Stop: `Ctrl+Shift+Alt+F12`

\- JSON Export/Import

\- Real-time recording and playback status



---



\## 🖥️ Interface Tabs



\- \*\*Remap\*\* — drag and drop gamepad buttons onto keyboard keys, up to 4 virtual gamepads

\- \*\*Configuration\*\* — per-key analog curve editor (Linear, Segments, Custom), deadzone, activation point

\- \*\*Combo/Macro\*\* — full combo and macro management

\- \*\*Gamepad Tester\*\* — real-time monitor of all virtual gamepad axes and button states

\- \*\*Global Settings\*\* — keyboard layout, polling rate (1ms default), UI refresh interval



---



\## 🔧 Build



1\. Open `HallJoy.sln` in Visual Studio 2022

2\. Select `Release | x64`

3\. Build



---



\## 🔩 Technical Details — abiv1.dll Deadlock Fix



A critical bug was identified and fixed in the \*\*universal-analog-plugin\*\* (`abiv1.dll`). After ~4 hours of runtime, the plugin crashed with `0xc0000005 INVALID\_POINTER\_READ` inside `unload()`.



\*\*Root cause\*\*: `awaitCompletion()` was called while `devices\_mtx` was still held. The internal device thread would then call `discover\_devices()`, which also tries to acquire `devices\_mtx` — deadlock. Windows force-terminated the thread, corrupting internal pointers.



\*\*Fix\*\*: split `unload()` into two passes — `cancelReceiveReport()` with lock held, then `awaitCompletion()` after releasing the lock. The fixed DLL runs stably for 12h+.



A fix has been submitted upstream to \[universal-analog-plugin](https://github.com/calamity-inc/universal-analog-plugin).



---



\## 🛠️ Troubleshooting



\- \*\*All analog values at 0\*\* — check your keyboard firmware mode. Some keyboards disable analog SDK output when \*\*Turbo mode\*\* is enabled. Disable it and restart.

\- \*\*Analog stops working after a plugin update\*\* — reinstall the DLL files from this repo and keep only one plugin variant in `C:\\Program Files\\WootingAnalogPlugins\\`

\- \*\*Macro not playing\*\* — check if emergency stop was triggered (`Ctrl+Shift+Alt+F12`)

\- \*\*Trigger not working\*\* — ensure the combination is not captured by another application



\### Enable Logging (for debugging)



By default, logging is disabled to preserve performance.

To enable it, open `settings.ini` and set:



```ini

\[Main]

Logging=1

```



A file `HallJoy\_log.txt` will be created next to the executable.

Set back to `Logging=0` (or remove the line) once done.



\### Rollback Wooting SDK (if needed)



If a newer Wooting SDK version causes unstable input or flickering:



```powershell

\# List available tags

powershell -ExecutionPolicy Bypass -File .\\tools\\rollback-wooting-sdk.ps1 -ListOnly



\# Install a specific tag (example v0.8.0)

powershell -ExecutionPolicy Bypass -File .\\tools\\rollback-wooting-sdk.ps1 -Tag v0.8.0

```



Then rebuild in VS (`Release | x64`) and run again. The script updates DLLs in `runtime\\`, `x64\\Release\\`, and `x64\\Debug\\`, and creates automatic backups under `runtime\\backup\\`.



---



\## 📁 Configuration Files



Stored next to the executable:



| File / Folder | Content |

|--------------|---------|

| `settings.ini` | Global settings |

| `bindings.ini` | Key-to-gamepad bindings |

| `Layouts/` | Keyboard layout presets (1 file = 1 preset) |

| `CurvePresets/` | Curve preset files |

| `\*.json` | Macro and combo configurations |



---



\## 📋 Requirements



\- Windows 10/11 (x64)

\- \[ViGEmBus](https://github.com/ViGEm/ViGEmBus)

\- \[Wooting Analog SDK](https://github.com/WootingKb/wooting-analog-sdk)

\- `abiv0.dll` + `abiv1.dll` in `C:\\Program Files\\WootingAnalogPlugins\\`



---



\## 🙏 Credits



\- \*\*\[PashOK7](https://github.com/PashOK7/HallJoy)\*\* — original HallJoy project

\- \*\*\[calamity-inc](https://github.com/calamity-inc/universal-analog-plugin)\*\* — universal-analog-plugin (Soup / abiv1.dll)

\- \*\*\[ViGEmClient](https://github.com/nefarius/ViGEmClient)\*\* — virtual gamepad emulation

\- \*\*\[Wooting](https://github.com/WootingKb/wooting-analog-sdk)\*\* — Analog SDK



---



\## 📄 License



MIT — free to use, modify, and redistribute. See \[LICENSE](LICENSE).



---



\*DrDre\_WASD — Precision Input Automation for Hall Effect Keyboards\*



