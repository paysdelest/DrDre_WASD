#include "free_combo_system.h"
#include "backend.h"
#include <windows.h>
#include <vector>
#include <mutex>
#include <atomic>
#include <queue>
#include <thread>
#include <condition_variable>
#include <string>
#include <utility>

static std::wstring TrimLine(const std::wstring& s)
{
    size_t start = 0;
    while (start < s.size() && (s[start] == L' ' || s[start] == L'\t' || s[start] == L'\r' || s[start] == L'\n'))
        ++start;
    size_t end = s.size();
    while (end > start && (s[end - 1] == L' ' || s[end - 1] == L'\t' || s[end - 1] == L'\r' || s[end - 1] == L'\n'))
        --end;
    return s.substr(start, end - start);
}

static bool ReadLine(FILE* f, std::wstring& out)
{
    wchar_t buf[2048] = {};
    if (!fgetws(buf, (int)_countof(buf), f))
        return false;
    out = TrimLine(buf);
    if (!out.empty() && out.front() == 0xFEFF)
        out.erase(out.begin());
    return true;
}

// ============================================================
// FREE COMBO SYSTEM - DrDre_WASD v2.0
// ============================================================

// --- VARIABLES INTERNES ---
namespace {
    std::vector<FreeCombo>          g_combos;
    std::mutex                      g_comboMutex;

    // Modifier state (updated by keyboard hook)
    std::atomic<bool> g_ctrlHeld = false;
    std::atomic<bool> g_shiftHeld = false;
    std::atomic<bool> g_altHeld = false;
    std::atomic<bool> g_winHeld = false;

    // Mouse state
    std::atomic<bool> g_leftHeld = false;
    std::atomic<bool> g_rightHeld = false;
    std::atomic<bool> g_middleHeld = false;
    std::atomic<DWORD> g_lastLeftDownTick = 0;
    std::atomic<DWORD> g_lastRightDownTick = 0;

    // Worker thread (reuses the same logic as MouseComboSystem)
    std::thread             g_worker;
    std::atomic<bool>       g_workerRunning = false;
    std::queue<std::vector<ComboAction>> g_queue;
    std::mutex              g_queueMutex;
    std::condition_variable g_queueCv;

    // Trigger CAPTURE
    std::atomic<bool>   g_capturing = false;
    std::atomic<bool>   g_CAPTUREComplete = false;
    std::atomic<bool>   g_CAPTUREWaitingSecond = false;
    FreeTrigger         g_CAPTUREdTrigger = {};
    FreeTrigger         g_CAPTUREFirstInput = {};
    DWORD               g_CAPTUREFirstTick = 0;
    std::mutex          g_CAPTUREMutex;
}

// --- UTILITIES ---

std::wstring FreeTriggerModifierToString(FreeTriggerModifier mod)
{
    switch (mod) {
    case FreeTriggerModifier::Ctrl:  return L"Ctrl";
    case FreeTriggerModifier::Shift: return L"Shift";
    case FreeTriggerModifier::Alt:   return L"Alt";
    case FreeTriggerModifier::Win:   return L"Win";
    default:                          return L"";
    }
}

std::wstring FreeTriggerKeyTypeToString(FreeTriggerKeyType type, WORD vk)
{
    switch (type) {
    case FreeTriggerKeyType::MouseLeft:   return L"Left click";
    case FreeTriggerKeyType::MouseRight:  return L"Right click";
    case FreeTriggerKeyType::MouseMiddle: return L"Middle click";
    case FreeTriggerKeyType::MouseX1:     return L"X1 button";
    case FreeTriggerKeyType::MouseX2:     return L"X2 button";
    case FreeTriggerKeyType::WheelUp:     return L"Wheel up";
    case FreeTriggerKeyType::WheelDown:   return L"Wheel down";
    case FreeTriggerKeyType::MouseDoubleLeft:  return L"Double left click";
    case FreeTriggerKeyType::MouseDoubleRight: return L"Double right click";
    case FreeTriggerKeyType::Keyboard: {
        // Convert VK to readable name
        wchar_t buf[64] = {};
        UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        LONG lParam = (scanCode << 16);
        // Extended keys
        if (vk == VK_INSERT || vk == VK_DELETE || vk == VK_HOME || vk == VK_END ||
            vk == VK_PRIOR || vk == VK_NEXT || vk == VK_LEFT || vk == VK_RIGHT ||
            vk == VK_UP || vk == VK_DOWN || vk == VK_NUMLOCK ||
            vk == VK_RCONTROL || vk == VK_RMENU || vk == VK_DIVIDE)
            lParam |= (1 << 24);
        GetKeyNameTextW(lParam, buf, 64);
        if (buf[0]) return std::wstring(buf);
        // Fallback: generic name
        return L"Key(" + std::to_wstring(vk) + L")";
    }
    default: return L"?";
    }
}

std::wstring FreeTrigger::ToString() const
{
    if (!IsValid()) return L"(not configured)";
    std::wstring result;
    std::wstring modStr = FreeTriggerModifierToString(modifier);
    if (!modStr.empty())
        result = modStr + L" + ";
    if (holdKeyType != FreeTriggerKeyType::None) {
        result += FreeTriggerKeyTypeToString(holdKeyType, holdVkCode);
        result += L" + ";
    }
    result += FreeTriggerKeyTypeToString(keyType, vkCode);
    return result;
}

static bool TriggerKeyEquals(FreeTriggerKeyType aType, WORD aVk, FreeTriggerKeyType bType, WORD bVk)
{
    if (aType != bType) return false;
    if (aType == FreeTriggerKeyType::Keyboard)
        return aVk == bVk;
    return true;
}

static bool IsTriggerKeyHeld(FreeTriggerKeyType type, WORD vk)
{
    switch (type) {
    case FreeTriggerKeyType::MouseLeft:   return g_leftHeld;
    case FreeTriggerKeyType::MouseRight:  return g_rightHeld;
    case FreeTriggerKeyType::MouseMiddle: return g_middleHeld;
    case FreeTriggerKeyType::Keyboard:    return (GetAsyncKeyState(vk) & 0x8000) != 0;
    default: return false;
    }
}

static void SetCAPTUREModifierFromCurrentState(FreeTrigger& out)
{
    if (g_ctrlHeld)  out.modifier = FreeTriggerModifier::Ctrl;
    else if (g_shiftHeld) out.modifier = FreeTriggerModifier::Shift;
    else if (g_altHeld)   out.modifier = FreeTriggerModifier::Alt;
    else if (g_winHeld)   out.modifier = FreeTriggerModifier::Win;
    else                  out.modifier = FreeTriggerModifier::None;
}

static bool IsModifierVk(WORD vk)
{
    return vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_CONTROL ||
        vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_SHIFT ||
        vk == VK_LMENU || vk == VK_RMENU || vk == VK_MENU ||
        vk == VK_LWIN || vk == VK_RWIN;
}

static void NormalizeCAPTUREdTrigger(FreeTrigger& t)
{
    if (t.holdKeyType != FreeTriggerKeyType::None)
        t.modifier = FreeTriggerModifier::None;
    if (t.keyType == FreeTriggerKeyType::Keyboard && IsModifierVk(t.vkCode))
        t.modifier = FreeTriggerModifier::None;
}

static constexpr DWORD kCAPTURESecondInputWindowMs = 900;

static void FinalizeSingleCAPTUREIfTimeoutUnlocked()
{
    if (!g_capturing || !g_CAPTUREWaitingSecond) return;
    DWORD now = GetTickCount();
    if (now - g_CAPTUREFirstTick < kCAPTURESecondInputWindowMs)
        return;

    g_CAPTUREdTrigger = {};
    g_CAPTUREdTrigger.keyType = g_CAPTUREFirstInput.keyType;
    g_CAPTUREdTrigger.vkCode = g_CAPTUREFirstInput.vkCode;
    SetCAPTUREModifierFromCurrentState(g_CAPTUREdTrigger);
    NormalizeCAPTUREdTrigger(g_CAPTUREdTrigger);
    g_CAPTUREWaitingSecond = false;
    g_capturing = false;
    g_CAPTUREComplete = true;
}

static bool HandleCAPTUREInput(FreeTriggerKeyType keyType, WORD vkCode)
{
    if (keyType == FreeTriggerKeyType::None) return false;

    std::lock_guard<std::mutex> lock(g_CAPTUREMutex);
    if (!g_capturing) return false;

    FinalizeSingleCAPTUREIfTimeoutUnlocked();
    if (!g_capturing) return true;

    // Wheel and double-click are standalone triggers.
    if (keyType == FreeTriggerKeyType::WheelUp ||
        keyType == FreeTriggerKeyType::WheelDown ||
        keyType == FreeTriggerKeyType::MouseDoubleLeft ||
        keyType == FreeTriggerKeyType::MouseDoubleRight)
    {
        g_CAPTUREdTrigger = {};
        g_CAPTUREdTrigger.keyType = keyType;
        g_CAPTUREdTrigger.vkCode = vkCode;
        SetCAPTUREModifierFromCurrentState(g_CAPTUREdTrigger);
        NormalizeCAPTUREdTrigger(g_CAPTUREdTrigger);
        g_CAPTUREWaitingSecond = false;
        g_capturing = false;
        g_CAPTUREComplete = true;
        return true;
    }

    if (!g_CAPTUREWaitingSecond)
    {
        g_CAPTUREFirstInput = {};
        g_CAPTUREFirstInput.keyType = keyType;
        g_CAPTUREFirstInput.vkCode = vkCode;
        g_CAPTUREFirstTick = GetTickCount();
        g_CAPTUREWaitingSecond = true;
        return true;
    }

    if (TriggerKeyEquals(g_CAPTUREFirstInput.keyType, g_CAPTUREFirstInput.vkCode, keyType, vkCode))
        return true;

    g_CAPTUREdTrigger = {};
    g_CAPTUREdTrigger.holdKeyType = g_CAPTUREFirstInput.keyType;
    g_CAPTUREdTrigger.holdVkCode = g_CAPTUREFirstInput.vkCode;
    g_CAPTUREdTrigger.keyType = keyType;
    g_CAPTUREdTrigger.vkCode = vkCode;
    g_CAPTUREdTrigger.modifier = FreeTriggerModifier::None;
    NormalizeCAPTUREdTrigger(g_CAPTUREdTrigger);

    g_CAPTUREWaitingSecond = false;
    g_capturing = false;
    g_CAPTUREComplete = true;
    return true;
}

// --- WORKER THREAD ---
static void WorkerFunc()
{
    while (g_workerRunning) {
        std::vector<ComboAction> actions;
        {
            std::unique_lock<std::mutex> lock(g_queueMutex);
            g_queueCv.wait(lock, [] { return !g_queue.empty() || !g_workerRunning; });
            if (!g_workerRunning && g_queue.empty()) break;
            if (!g_queue.empty()) { actions = g_queue.front(); g_queue.pop(); }
        }

        for (const auto& action : actions) {
            if (action.type == ComboActionType::Delay) {
                Sleep(action.delayMs);
                continue;
            }
            if (action.type == ComboActionType::PressKey ||
                action.type == ComboActionType::ReleaseKey ||
                action.type == ComboActionType::TapKey)
            {
                INPUT inp[1] = {};
                WORD vk = HidToVk(action.keyHid);

                if (action.type == ComboActionType::PressKey) {
                    BackendUI_SetAnalogMilli(action.keyHid, 1000);
                    Backend_SetMacroAnalog(action.keyHid, 1000.0f);
                    inp[0].type = INPUT_KEYBOARD; inp[0].ki.wVk = vk;
                    SendInput(1, inp, sizeof(INPUT));
                }
                else if (action.type == ComboActionType::ReleaseKey) {
                    inp[0].type = INPUT_KEYBOARD; inp[0].ki.wVk = vk;
                    inp[0].ki.dwFlags = KEYEVENTF_KEYUP;
                    SendInput(1, inp, sizeof(INPUT));
                    BackendUI_SetAnalogMilli(action.keyHid, 0);
                    Backend_ClearMacroAnalog(action.keyHid);
                }
                else { // TapKey
                    Backend_SetMacroAnalogForMs(action.keyHid, 1.0f, 120);
                    inp[0].type = INPUT_KEYBOARD; inp[0].ki.wVk = vk;
                    SendInput(1, inp, sizeof(INPUT));
                    Sleep(30);
                    inp[0].ki.dwFlags = KEYEVENTF_KEYUP;
                    SendInput(1, inp, sizeof(INPUT));
                }
            }
            else if (action.type == ComboActionType::MouseClick) {
                INPUT inp[2] = {};
                DWORD downFlag = 0, upFlag = 0;
                switch (action.mouseButton) {
                case 0: downFlag = MOUSEEVENTF_LEFTDOWN;   upFlag = MOUSEEVENTF_LEFTUP;   break;
                case 1: downFlag = MOUSEEVENTF_RIGHTDOWN;  upFlag = MOUSEEVENTF_RIGHTUP;  break;
                case 2: downFlag = MOUSEEVENTF_MIDDLEDOWN; upFlag = MOUSEEVENTF_MIDDLEUP; break;
                }
                if (downFlag) {
                    inp[0].type = INPUT_MOUSE; inp[0].mi.dwFlags = downFlag;
                    inp[1].type = INPUT_MOUSE; inp[1].mi.dwFlags = upFlag;
                    SendInput(2, inp, sizeof(INPUT));
                }
            }
            else if (action.type == ComboActionType::TypeText) {
                for (wchar_t c : action.text) {
                    INPUT inp[2] = {};
                    inp[0].type = INPUT_KEYBOARD; inp[0].ki.wVk = 0;
                    inp[0].ki.wScan = c; inp[0].ki.dwFlags = KEYEVENTF_UNICODE;
                    inp[1] = inp[0]; inp[1].ki.dwFlags |= KEYEVENTF_KEYUP;
                    SendInput(2, inp, sizeof(INPUT));
                    Sleep(10);
                }
            }
            if (action.type != ComboActionType::Delay) Sleep(10);
        }
    }
}

// --- Check whether current modifier matches ---
static bool ModifierMatches(FreeTriggerModifier mod)
{
    switch (mod) {
    case FreeTriggerModifier::None:  return true;
    case FreeTriggerModifier::Ctrl:  return g_ctrlHeld;
    case FreeTriggerModifier::Shift: return g_shiftHeld;
    case FreeTriggerModifier::Alt:   return g_altHeld;
    case FreeTriggerModifier::Win:   return g_winHeld;
    }
    return false;
}

static bool TriggerExtraConditionsMatch(const FreeTrigger& trigger)
{
    if (!ModifierMatches(trigger.modifier)) return false;
    if (trigger.holdKeyType != FreeTriggerKeyType::None)
        return IsTriggerKeyHeld(trigger.holdKeyType, trigger.holdVkCode);
    return true;
}

// --- Trigger combo ---
static void FireCombo(FreeCombo& combo)
{
    {
        std::lock_guard<std::mutex> qLock(g_queueMutex);
        g_queue.push(combo.actions);
    }
    g_queueCv.notify_one();
    combo.lastExecTime = GetTickCount();
}

// ============================================================
// FreeComboSystem namespace implementation
// ============================================================
namespace FreeComboSystem
{
    void Initialize()
    {
        if (!g_workerRunning) {
            g_workerRunning = true;
            g_worker = std::thread(WorkerFunc);
        }
    }

    void Shutdown()
    {
        // IMPORTANT : ne pas vider g_combos ici !
        // main.cpp appelle SaveToFile() PUIS Shutdown() dans cet ordre :
        //   ShutdownFreeComboSystem() -> SaveToFile() -> Shutdown()
        // Si on vidait g_combos dans Shutdown(), SaveToFile() SAVErait un fichier vide.
        g_workerRunning = false;
        g_queueCv.notify_all();
        if (g_worker.joinable()) g_worker.join();
        // g_combos est intentionnellement conserve pour que SaveToFile puisse etre appele apres.
        // Le destructeur de g_combos (fin de programme) s occupera du nettoyage.
    }

    // --- CRUD ---
    int CreateCombo(const std::wstring& name)
    {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        FreeCombo c;
        c.name = name;
        g_combos.push_back(c);
        return (int)g_combos.size() - 1;
    }

    bool DeleteCombo(int id)
    {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        if (id < 0 || id >= (int)g_combos.size()) return false;
        g_combos.erase(g_combos.begin() + id);
        return true;
    }

    FreeCombo* GetCombo(int id)
    {
        if (id < 0 || id >= (int)g_combos.size()) return nullptr;
        return &g_combos[id];
    }

    std::vector<int> GetAllIds()
    {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        std::vector<int> ids;
        for (int i = 0; i < (int)g_combos.size(); ++i) ids.push_back(i);
        return ids;
    }

    int GetCount()
    {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        return (int)g_combos.size();
    }

    bool AddAction(int id, const ComboAction& action)
    {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        if (id < 0 || id >= (int)g_combos.size()) return false;
        g_combos[id].actions.push_back(action);
        return true;
    }

    bool RemoveAction(int id, int actionIndex)
    {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        if (id < 0 || id >= (int)g_combos.size()) return false;
        auto& actions = g_combos[id].actions;
        if (actionIndex < 0 || actionIndex >= (int)actions.size()) return false;
        actions.erase(actions.begin() + actionIndex);
        return true;
    }

    bool MoveActionUp(int id, int actionIndex)
    {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        if (id < 0 || id >= (int)g_combos.size()) return false;
        auto& actions = g_combos[id].actions;
        if (actionIndex <= 0 || actionIndex >= (int)actions.size()) return false;
        std::swap(actions[actionIndex], actions[actionIndex - 1]);
        return true;
    }

    bool MoveActionDown(int id, int actionIndex)
    {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        if (id < 0 || id >= (int)g_combos.size()) return false;
        auto& actions = g_combos[id].actions;
        if (actionIndex < 0 || actionIndex >= (int)actions.size() - 1) return false;
        std::swap(actions[actionIndex], actions[actionIndex + 1]);
        return true;
    }

    bool ClearActions(int id)
    {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        if (id < 0 || id >= (int)g_combos.size()) return false;
        g_combos[id].actions.clear();
        return true;
    }

    bool SetTrigger(int id, const FreeTrigger& trigger)
    {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        if (id < 0 || id >= (int)g_combos.size()) return false;
        g_combos[id].trigger = trigger;
        return true;
    }

    bool SetEnabled(int id, bool enabled)
    {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        if (id < 0 || id >= (int)g_combos.size()) return false;
        g_combos[id].enabled = enabled;
        return true;
    }

    bool SetRepeat(int id, bool repeat, uint32_t delayMs)
    {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        if (id < 0 || id >= (int)g_combos.size()) return false;
        g_combos[id].repeatWhileHeld = repeat;
        g_combos[id].repeatDelayMs = delayMs;
        return true;
    }

    // --- CAPTURE ---
    void StartCapture()
    {
        std::lock_guard<std::mutex> lock(g_CAPTUREMutex);
        g_CAPTUREComplete = false;
        g_CAPTUREWaitingSecond = false;
        g_CAPTUREFirstInput = {};
        g_CAPTUREFirstTick = 0;
        g_CAPTUREdTrigger = {};
        g_capturing = true;
    }

    void StopCAPTURE()
    {
        std::lock_guard<std::mutex> lock(g_CAPTUREMutex);
        g_capturing = false;
        g_CAPTUREComplete = false;
        g_CAPTUREWaitingSecond = false;
        g_CAPTUREFirstInput = {};
        g_CAPTUREFirstTick = 0;
    }

    bool IsCapturing() { return g_capturing; }
    bool IsCaptureComplete()
    {
        std::lock_guard<std::mutex> lock(g_CAPTUREMutex);
        FinalizeSingleCAPTUREIfTimeoutUnlocked();
        return g_CAPTUREComplete;
    }

    FreeTrigger GetCapturedTrigger()
    {
        std::lock_guard<std::mutex> lock(g_CAPTUREMutex);
        return g_CAPTUREdTrigger;
    }

    bool IsCaptureWaitingSecondInput()
    {
        std::lock_guard<std::mutex> lock(g_CAPTUREMutex);
        FinalizeSingleCAPTUREIfTimeoutUnlocked();
        return g_CAPTUREWaitingSecond;
    }

    FreeTrigger GetCaptureFirstInput()
    {
        std::lock_guard<std::mutex> lock(g_CAPTUREMutex);
        return g_CAPTUREFirstInput;
    }

    // --- MOUSE EVENTS ---
    void ProcessMouseEvent(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_MOUSEMOVE) return;

        DWORD now = GetTickCount();
        bool isDoubleLeft = false;
        bool isDoubleRight = false;
        if (msg == WM_LBUTTONDOWN)
        {
            DWORD prev = g_lastLeftDownTick.load();
            isDoubleLeft = (prev != 0 && (now - prev) <= GetDoubleClickTime());
            g_lastLeftDownTick = now;
        }
        else if (msg == WM_RBUTTONDOWN)
        {
            DWORD prev = g_lastRightDownTick.load();
            isDoubleRight = (prev != 0 && (now - prev) <= GetDoubleClickTime());
            g_lastRightDownTick = now;
        }

        // Update mouse state
        if (msg == WM_LBUTTONDOWN) g_leftHeld = true;
        else if (msg == WM_LBUTTONUP)   g_leftHeld = false;
        else if (msg == WM_RBUTTONDOWN) g_rightHeld = true;
        else if (msg == WM_RBUTTONUP)   g_rightHeld = false;
        else if (msg == WM_MBUTTONDOWN) g_middleHeld = true;
        else if (msg == WM_MBUTTONUP)   g_middleHeld = false;

        // --- CAPTURE MODE ---
        if (g_capturing) {
            FreeTriggerKeyType keyType = FreeTriggerKeyType::None;
            if (msg == WM_LBUTTONDOWN) keyType = isDoubleLeft ? FreeTriggerKeyType::MouseDoubleLeft : FreeTriggerKeyType::MouseLeft;
            else if (msg == WM_RBUTTONDOWN) keyType = isDoubleRight ? FreeTriggerKeyType::MouseDoubleRight : FreeTriggerKeyType::MouseRight;
            else if (msg == WM_MBUTTONDOWN) keyType = FreeTriggerKeyType::MouseMiddle;
            else if (msg == WM_XBUTTONDOWN) {
                WORD btn = GET_XBUTTON_WPARAM(wParam);
                keyType = (btn == XBUTTON1) ? FreeTriggerKeyType::MouseX1 : FreeTriggerKeyType::MouseX2;
            }
            else if (msg == WM_MOUSEWHEEL) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                keyType = (delta > 0) ? FreeTriggerKeyType::WheelUp : FreeTriggerKeyType::WheelDown;
            }

            if (HandleCAPTUREInput(keyType, 0))
                return;
        }

        // --- NORMAL MODE: combo checks ---
        FreeTriggerKeyType eventType = FreeTriggerKeyType::None;
        FreeTriggerKeyType fallbackEventType = FreeTriggerKeyType::None;
        bool isDown = false;
        if (msg == WM_LBUTTONDOWN) {
            eventType = isDoubleLeft ? FreeTriggerKeyType::MouseDoubleLeft : FreeTriggerKeyType::MouseLeft;
            if (isDoubleLeft) fallbackEventType = FreeTriggerKeyType::MouseLeft;
            isDown = true;
        }
        else if (msg == WM_RBUTTONDOWN) {
            eventType = isDoubleRight ? FreeTriggerKeyType::MouseDoubleRight : FreeTriggerKeyType::MouseRight;
            if (isDoubleRight) fallbackEventType = FreeTriggerKeyType::MouseRight;
            isDown = true;
        }
        else if (msg == WM_MBUTTONDOWN) { eventType = FreeTriggerKeyType::MouseMiddle; isDown = true; }
        else if (msg == WM_MOUSEWHEEL) {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            eventType = (delta > 0) ? FreeTriggerKeyType::WheelUp : FreeTriggerKeyType::WheelDown;
            isDown = true;
        }

        if (!isDown) return;

        std::unique_lock<std::mutex> lock(g_comboMutex, std::try_to_lock);
        if (!lock.owns_lock()) return;

        for (int i = (int)g_combos.size() - 1; i >= 0; --i) {
            auto& combo = g_combos[(size_t)i];
            if (!combo.enabled || !combo.trigger.IsValid()) continue;
            if (combo.trigger.keyType != eventType &&
                combo.trigger.keyType != fallbackEventType) continue;
            if (!TriggerExtraConditionsMatch(combo.trigger)) continue;
            FireCombo(combo);
            break; // Prioritize latest combo and avoid double-fire (e.g. P then G).
        }
    }

    // --- KEYBOARD EVENTS ---
    bool ProcessKeyboardEvent(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        WORD vk = (WORD)wParam;
        bool isDown = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
        bool isUp = (msg == WM_KEYUP || msg == WM_SYSKEYUP);

        // Update modifiers
        if (vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_CONTROL)
            g_ctrlHeld = isDown;
        else if (vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_SHIFT)
            g_shiftHeld = isDown;
        else if (vk == VK_LMENU || vk == VK_RMENU || vk == VK_MENU)
            g_altHeld = isDown;
        else if (vk == VK_LWIN || vk == VK_RWIN)
            g_winHeld = isDown;

        if (!isDown) return false;

        // --- CAPTURE MODE ---
        if (g_capturing)
        {
            if (HandleCAPTUREInput(FreeTriggerKeyType::Keyboard, vk))
                return false;
        }

        // --- MODE NORMAL ---
        std::unique_lock<std::mutex> lock(g_comboMutex, std::try_to_lock);
        if (!lock.owns_lock()) return false;

        for (int i = (int)g_combos.size() - 1; i >= 0; --i) {
            auto& combo = g_combos[(size_t)i];
            if (!combo.enabled || !combo.trigger.IsValid()) continue;
            if (combo.trigger.keyType != FreeTriggerKeyType::Keyboard) continue;
            if (combo.trigger.vkCode != vk) continue;
            if (!TriggerExtraConditionsMatch(combo.trigger)) continue;
            FireCombo(combo);
            return true;
        }
        return false;
    }

    // --- TICK (repeats) ---
    void Tick()
    {
        DWORD now = GetTickCount();
        std::unique_lock<std::mutex> lock(g_comboMutex, std::try_to_lock);
        if (!lock.owns_lock()) return;

        for (auto& combo : g_combos) {
            if (!combo.enabled || !combo.repeatWhileHeld) continue;
            if (!combo.trigger.IsValid()) continue;

            // Check whether trigger key/button is still held
            bool held = IsTriggerKeyHeld(combo.trigger.keyType, combo.trigger.vkCode);
            if (held)
                held = TriggerExtraConditionsMatch(combo.trigger);

            if (held && (now - combo.lastExecTime >= combo.repeatDelayMs)) {
                FireCombo(combo);
            }
        }
    }

    // --- EXAMPLES ---
    bool HasExampleCombos()
    {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        for (const auto& c : g_combos)
            if (c.isExample) return true;
        return false;
    }

    void CreateExampleCombos()
    {
        // Example 1: Right click held + left click -> Tap P
        {
            FreeCombo c;
            c.name = L"[Example] Right click + left click -> Tap P";
            c.isExample = true;
            c.trigger.keyType = FreeTriggerKeyType::MouseLeft;
            c.trigger.holdKeyType = FreeTriggerKeyType::MouseRight;
            ComboAction a; a.type = ComboActionType::TapKey; a.keyHid = VkToHid('P');
            c.actions.push_back(a);
            std::lock_guard<std::mutex> lock(g_comboMutex);
            g_combos.push_back(c);
        }
    }

    // --- SAVE ---
    bool SaveToFile(const wchar_t* path)
    {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        FILE* f = nullptr;
        if (_wfopen_s(&f, path, L"w") != 0 || !f) return false;

        fwprintf(f, L"DRDRE_FREECOMBOS_V3\n");
        fwprintf(f, L"%u\n", (unsigned)g_combos.size());

        for (const auto& combo : g_combos) {
            fwprintf(f, L"%s\n", combo.name.c_str());
            fwprintf(f, L"%d %d %d %d %d\n",
                (int)combo.trigger.modifier,
                (int)combo.trigger.keyType,
                (int)combo.trigger.vkCode,
                (int)combo.trigger.holdKeyType,
                (int)combo.trigger.holdVkCode);
            fwprintf(f, L"%d %d %d %d\n",
                combo.enabled ? 1 : 0,
                combo.repeatWhileHeld ? 1 : 0,
                combo.repeatDelayMs,
                combo.isExample ? 1 : 0);
            fwprintf(f, L"%u\n", (unsigned)combo.actions.size());
            for (const auto& action : combo.actions) {
                std::wstring textLine = action.text;
                for (wchar_t& ch : textLine) {
                    if (ch == L'\r' || ch == L'\n') ch = L' ';
                }
                if (textLine.empty()) textLine = L"__EMPTY__";
                fwprintf(f, L"%d %d %d %d\n",
                    (int)action.type, action.keyHid, action.delayMs, action.mouseButton);
                fwprintf(f, L"%s\n", textLine.c_str());
            }
        }
        fclose(f);
        return true;
    }

    // --- LOAD ---
    bool LoadFromFile(const wchar_t* path)
    {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        FILE* f = nullptr;
        if (_wfopen_s(&f, path, L"r") != 0 || !f) return false;

        std::wstring line;
        if (!ReadLine(f, line)) { fclose(f); return false; }
        bool isV3 = false;
        bool isV2 = false;
        if (line == L"DRDRE_FREECOMBOS_V3") isV3 = true;
        else if (line == L"DRDRE_FREECOMBOS_V2") isV2 = true;
        else if (line == L"DRDRE_FREECOMBOS_V1") {}
        else { fclose(f); return false; }

        unsigned countU = 0;
        if (!ReadLine(f, line) || swscanf_s(line.c_str(), L"%u", &countU) != 1) { fclose(f); return false; }
        size_t count = (size_t)countU;

        std::vector<FreeCombo> loaded;
        bool parseOk = true;

        for (size_t i = 0; i < count; ++i) {
            FreeCombo combo;
            if (!ReadLine(f, combo.name)) { parseOk = false; break; }

            int mod = 0, keyType = 0, vk = 0, holdKeyType = 0, holdVk = 0;
            if (!ReadLine(f, line)) { parseOk = false; break; }
            if (isV3 || isV2) {
                if (swscanf_s(line.c_str(), L"%d %d %d %d %d", &mod, &keyType, &vk, &holdKeyType, &holdVk) != 5)
                {
                    parseOk = false; break;
                }
            }
            else {
                if (swscanf_s(line.c_str(), L"%d %d %d", &mod, &keyType, &vk) != 3)
                {
                    parseOk = false; break;
                }
            }
            combo.trigger.modifier = (FreeTriggerModifier)mod;
            combo.trigger.keyType = (FreeTriggerKeyType)keyType;
            combo.trigger.vkCode = (WORD)vk;
            combo.trigger.holdKeyType = (FreeTriggerKeyType)holdKeyType;
            combo.trigger.holdVkCode = (WORD)holdVk;

            int en = 0, rep = 0, del = 0, isEx = 0;
            if (!ReadLine(f, line) || swscanf_s(line.c_str(), L"%d %d %d %d", &en, &rep, &del, &isEx) != 4)
            {
                parseOk = false; break;
            }
            combo.enabled = (en != 0);
            combo.repeatWhileHeld = (rep != 0);
            combo.repeatDelayMs = del;
            combo.isExample = (isEx != 0);

            unsigned actionCountU = 0;
            if (!ReadLine(f, line) || swscanf_s(line.c_str(), L"%u", &actionCountU) != 1)
            {
                parseOk = false; break;
            }
            size_t actionCount = (size_t)actionCountU;
            for (size_t j = 0; j < actionCount; ++j) {
                ComboAction action;
                int type = 0, hid = 0, d = 0, btn = 0;
                if (!ReadLine(f, line)) { parseOk = false; break; }
                if (swscanf_s(line.c_str(), L"%d %d %d %d", &type, &hid, &d, &btn) != 4)
                {
                    parseOk = false; break;
                }
                action.type = (ComboActionType)type;
                action.keyHid = hid;
                action.delayMs = d;
                action.mouseButton = btn;
                if (isV3) {
                    std::wstring txt;
                    if (!ReadLine(f, txt)) { parseOk = false; break; }
                    action.text = (txt == L"__EMPTY__") ? L"" : txt;
                }
                else {
                    int consumed = 0;
                    if (swscanf_s(line.c_str(), L"%d %d %d %d %n", &type, &hid, &d, &btn, &consumed) == 4 &&
                        consumed > 0 && consumed < (int)line.size())
                    {
                        std::wstring tail = TrimLine(line.substr((size_t)consumed));
                        action.text = (tail == L"__EMPTY__") ? L"" : tail;
                    }
                    else {
                        action.text.clear();
                    }
                }
                combo.actions.push_back(action);
            }
            if (!parseOk) break;
            loaded.push_back(combo);
        }
        fclose(f);

        // FIX : si le parsing a reussi au moins partiellement, on applique ce qu on a.
        // L ancienne condition loaded.size() != count rejetait TOUT si 1 combo etait corrompu.
        // Desormais on accepte un LOAD partiel plutot que de tout perdre.
        if (!parseOk && loaded.empty())
            return false;   // Fichier totalement illisible -> echec propre

        g_combos = std::move(loaded);
        return true;
    }

} // namespace FreeComboSystem


