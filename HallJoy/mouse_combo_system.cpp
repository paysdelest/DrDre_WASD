#include "mouse_combo_system.h"
#include "backend.h" // NÉCESSAIRE pour parler à l'UI et au Backend
#include <windows.h>
#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <map>
#include <string>
#include <stdarg.h>
#include <stdio.h>

// --- OUTILS DE DEBUG ---
void DebugLog(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    OutputDebugStringA(buffer);
}

// --- VARIABLES GLOBALES INTERNES ---
namespace {
    std::vector<MouseCombo> g_combos;
    std::mutex g_comboMutex; // Protège l'accès à la liste des combos

    // État des boutons de la souris
    std::atomic<bool> g_rightButtonHeld = false;
    std::atomic<bool> g_leftButtonHeld = false;
    std::atomic<bool> g_middleButtonHeld = false;

    // --- SYSTÈME ASYNCHRONE (WORKER THREAD) ---
    std::thread g_workerThread;
    std::atomic<bool> g_workerRunning = false;

    // File d'attente des actions à exécuter
    std::queue<std::vector<ComboAction>> g_actionQueue;
    std::mutex g_queueMutex;
    std::condition_variable g_queueCv;
}

// --- FONCTION DU WORKER THREAD ---
// C'est ici que les actions sont exécutées pour NE PAS BLOQUER la souris
void WorkerThreadFunc() {
    DebugLog("[COMBO SYSTEM] Worker Thread Démarré.\n");

    while (g_workerRunning) {
        std::vector<ComboAction> actionsToExecute;

        {
            // Attente passive d'une nouvelle commande
            std::unique_lock<std::mutex> lock(g_queueMutex);
            g_queueCv.wait(lock, [] { return !g_actionQueue.empty() || !g_workerRunning; });

            if (!g_workerRunning && g_actionQueue.empty()) break;

            if (!g_actionQueue.empty()) {
                actionsToExecute = g_actionQueue.front();
                g_actionQueue.pop();
            }
        }

        // Exécution des actions (HORS du lock, donc la souris reste libre)
        if (!actionsToExecute.empty()) {
            DebugLog("[COMBO WORKER] Exécution de %d actions...\n", (int)actionsToExecute.size());

            for (const auto& action : actionsToExecute) {
                if (action.type == ComboActionType::Delay) {
                    Sleep(action.delayMs);
                }
                else if (action.type == ComboActionType::TapKey ||
                    action.type == ComboActionType::PressKey ||
                    action.type == ComboActionType::ReleaseKey) {

                    INPUT inputs[2] = {};

                    // --- CAS 1 : PRESS (Maintenir) ---
                    if (action.type == ComboActionType::PressKey) {
                        // 1. Visuel UI + Backend HallJoy
                        BackendUI_SetAnalogMilli(action.keyHid, 1000);
                        Backend_SetMacroAnalog(action.keyHid, 1000.0f);

                        // 2. Envoi Windows
                        inputs[0].type = INPUT_KEYBOARD;
                        inputs[0].ki.wVk = HidToVk(action.keyHid);
                        SendInput(1, inputs, sizeof(INPUT));
                    }
                    // --- CAS 2 : RELEASE (Relâcher) ---
                    else if (action.type == ComboActionType::ReleaseKey) {
                        // 1. Envoi Windows
                        inputs[0].type = INPUT_KEYBOARD;
                        inputs[0].ki.wVk = HidToVk(action.keyHid);
                        inputs[0].ki.dwFlags = KEYEVENTF_KEYUP;
                        SendInput(1, inputs, sizeof(INPUT));

                        // 2. Nettoyage UI + Backend
                        BackendUI_SetAnalogMilli(action.keyHid, 0);
                        Backend_ClearMacroAnalog(action.keyHid);
                    }
                    // --- CAS 3 : TAP (Appuyer + Relâcher) ---
                    else if (action.type == ComboActionType::TapKey) {
                        // A. Appui
                        BackendUI_SetAnalogMilli(action.keyHid, 1000);
                        Backend_SetMacroAnalog(action.keyHid, 1000.0f);

                        inputs[0].type = INPUT_KEYBOARD;
                        inputs[0].ki.wVk = HidToVk(action.keyHid);
                        SendInput(1, inputs, sizeof(INPUT));

                        // B. Délai pour que ce soit visible et pris en compte
                        Sleep(30);

                        // C. Relâchement
                        inputs[0].ki.dwFlags = KEYEVENTF_KEYUP;
                        SendInput(1, inputs, sizeof(INPUT));

                        // D. Nettoyage
                        BackendUI_SetAnalogMilli(action.keyHid, 0);
                        Backend_ClearMacroAnalog(action.keyHid);
                    }
                }

                // Petit délai de sécurité entre les actions
                if (action.type != ComboActionType::Delay) Sleep(10);
            }
        }
    }
    DebugLog("[COMBO SYSTEM] Worker Thread Arrêté.\n");
}

namespace MouseComboSystem
{
    void Initialize() {
        if (!g_workerRunning) {
            g_workerRunning = true;
            g_workerThread = std::thread(WorkerThreadFunc);
            DebugLog("[COMBO SYSTEM] Initialisé.\n");
        }
    }

    void Shutdown() {
        g_workerRunning = false;
        g_queueCv.notify_all();
        if (g_workerThread.joinable()) {
            g_workerThread.join();
        }

        std::lock_guard<std::mutex> lock(g_comboMutex);
        g_combos.clear();
    }

    // --- CŒUR DU PROBLÈME : LE HOOK ---
    void ProcessMouseEvent(UINT msg, WPARAM wParam, LPARAM lParam) {
        // 1. FILTRAGE CRITIQUE DES AXES
        if (msg == WM_MOUSEMOVE) {
            return; // ON SORT IMMÉDIATEMENT
        }

        // Mise à jour rapide de l'état des boutons
        bool stateChanged = false;
        if (msg == WM_RBUTTONDOWN) { g_rightButtonHeld = true; stateChanged = true; }
        else if (msg == WM_RBUTTONUP) { g_rightButtonHeld = false; stateChanged = true; }
        else if (msg == WM_LBUTTONDOWN) { g_leftButtonHeld = true; stateChanged = true; }
        else if (msg == WM_LBUTTONUP) { g_leftButtonHeld = false; stateChanged = true; }
        else if (msg == WM_MBUTTONDOWN) { g_middleButtonHeld = true; stateChanged = true; }
        else if (msg == WM_MBUTTONUP) { g_middleButtonHeld = false; stateChanged = true; }

        if (!stateChanged) return;

        // Vérification des combos
        std::unique_lock<std::mutex> lock(g_comboMutex, std::try_to_lock);
        if (!lock.owns_lock()) {
            return;
        }

        DWORD currentTime = GetTickCount();

        for (auto& combo : g_combos) {
            if (!combo.enabled) continue;

            bool triggered = false;

            // Logique de déclenchement
            switch (combo.trigger) {
            case ComboTriggerType::RightClickHeld_LeftClick:
                if (g_rightButtonHeld && msg == WM_LBUTTONDOWN) triggered = true;
                break;
            case ComboTriggerType::LeftClickHeld_RightClick:
                if (g_leftButtonHeld && msg == WM_RBUTTONDOWN) triggered = true;
                break;
            case ComboTriggerType::DoubleLeftClick:
                if (msg == WM_LBUTTONDBLCLK) triggered = true;
                break;
                // ... autres cas ...
            }

            if (triggered) {
                DebugLog("[COMBO] Déclenchement : %ws\n", combo.name.c_str());

                // IMPORTANT : On n'exécute pas l'action ici !
                // On l'envoie à la file d'attente du Worker Thread.
                {
                    std::lock_guard<std::mutex> qLock(g_queueMutex);
                    g_actionQueue.push(combo.actions);
                }
                g_queueCv.notify_one();
            }
        }
    }

    void Tick() {
        // Gestion des répétitions automatiques (TapKey répété, etc.)
        DWORD currentTime = GetTickCount();
        
        std::unique_lock<std::mutex> lock(g_comboMutex, std::try_to_lock);
        if (!lock.owns_lock()) return;

        for (auto& combo : g_combos) {
            if (!combo.enabled || !combo.repeatWhileHeld) continue;

            bool active = false;
            switch (combo.trigger) {
                case ComboTriggerType::RightClickHeld_LeftClick:
                    active = (g_rightButtonHeld && g_leftButtonHeld);
                    break;
                case ComboTriggerType::LeftClickHeld_RightClick:
                    active = (g_leftButtonHeld && g_rightButtonHeld);
                    break;
                case ComboTriggerType::MiddleClickHeld_LeftClick:
                    active = (g_middleButtonHeld && g_leftButtonHeld);
                    break;
                case ComboTriggerType::MiddleClickHeld_RightClick:
                    active = (g_middleButtonHeld && g_rightButtonHeld);
                    break;
                case ComboTriggerType::LeftAndRightTogether:
                    active = (g_leftButtonHeld && g_rightButtonHeld);
                    break;
                default:
                    break;
            }

            if (active) {
                if (currentTime - combo.lastExecutionTime >= combo.repeatDelayMs) {
                    combo.lastExecutionTime = currentTime;
                    {
                        std::lock_guard<std::mutex> qLock(g_queueMutex);
                        g_actionQueue.push(combo.actions);
                    }
                    g_queueCv.notify_one();
                }
            }
        }
    }

    // --- GESTION DES COMBOS (CRUD) ---

    int CreateCombo(const std::wstring& name, ComboTriggerType trigger) {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        MouseCombo combo;
        combo.name = name;
        combo.trigger = trigger;
        // Throttle par défaut à 400ms comme demandé
        combo.repeatDelayMs = 400; 
        combo.repeatWhileHeld = true;
        combo.lastExecutionTime = 0;
        g_combos.push_back(combo);
        return (int)g_combos.size() - 1;
    }

    bool AddAction(int comboId, const ComboAction& action) {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        if (comboId >= 0 && comboId < (int)g_combos.size()) {
            g_combos[comboId].actions.push_back(action);
            return true;
        }
        return false;
    }

    bool SetComboRepeat(int comboId, bool repeat, uint32_t delayMs) {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        if (comboId >= 0 && comboId < (int)g_combos.size()) {
            g_combos[comboId].repeatWhileHeld = repeat;
            g_combos[comboId].repeatDelayMs = delayMs;
            return true;
        }
        return false;
    }

    // --- IMPLÉMENTATIONS ---

    bool DeleteCombo(int comboId) {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        if (comboId >= 0 && comboId < (int)g_combos.size()) {
            g_combos.erase(g_combos.begin() + comboId);
            return true;
        }
        return false;
    }

    MouseCombo* GetCombo(int comboId) {
        if (comboId >= 0 && comboId < (int)g_combos.size()) {
            return &g_combos[comboId];
        }
        return nullptr;
    }

    std::vector<int> GetAllComboIds() {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        std::vector<int> ids;
        for (int i = 0; i < (int)g_combos.size(); ++i) {
            ids.push_back(i);
        }
        return ids;
    }

    int GetComboCount() {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        return (int)g_combos.size();
    }

    bool ClearActions(int comboId) {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        if (comboId >= 0 && comboId < (int)g_combos.size()) {
            g_combos[comboId].actions.clear();
            return true;
        }
        return false;
    }

    bool SetComboEnabled(int comboId, bool enabled) {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        if (comboId >= 0 && comboId < (int)g_combos.size()) {
            g_combos[comboId].enabled = enabled;
            return true;
        }
        return false;
    }

    bool IsComboEnabled(int comboId) {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        if (comboId >= 0 && comboId < (int)g_combos.size()) {
            return g_combos[comboId].enabled;
        }
        return false;
    }

    // --- HELPERS ---

    ComboAction MakeTapKeyAction(uint16_t hid) {
        ComboAction a; a.type = ComboActionType::TapKey; a.keyHid = hid; return a;
    }
    ComboAction MakeDelayAction(uint32_t ms) {
        ComboAction a; a.type = ComboActionType::Delay; a.delayMs = ms; return a;
    }

    void ProcessKeyboardEvent(UINT msg, WPARAM wParam, LPARAM lParam) {}
    bool IsRightButtonHeld() { return g_rightButtonHeld; }
    bool IsLeftButtonHeld() { return g_leftButtonHeld; }
    bool IsMiddleButtonHeld() { return g_middleButtonHeld; }
    
    bool SaveToFile(const wchar_t* filePath) {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        FILE* f = nullptr;
        // Utilisation de _wfopen_s pour une meilleure compatibilité avec les chemins Windows
        if (_wfopen_s(&f, filePath, L"w") != 0 || !f) {
            DebugLog("SaveToFile: Impossible d'ouvrir le fichier\n");
            return false;
        }

        fwprintf(f, L"HALLJOY_COMBOS_V1\n");
        fwprintf(f, L"%zu\n", g_combos.size());

        for (const auto& combo : g_combos) {
            // Nom sur une ligne séparée
            fwprintf(f, L"%s\n", combo.name.c_str());
            
            fwprintf(f, L"%d\n", (int)combo.trigger);
            fwprintf(f, L"%d %d %d %d\n", combo.enabled, combo.repeatWhileHeld, combo.repeatDelayMs, combo.useLocalRepeat);

            fwprintf(f, L"%zu\n", combo.actions.size());
            for (const auto& action : combo.actions) {
                const wchar_t* txt = action.text.empty() ? L"__EMPTY__" : action.text.c_str();
                fwprintf(f, L"%d %d %d %d %s\n", (int)action.type, action.keyHid, action.delayMs, action.mouseButton, txt);
            }
        }
        fclose(f);
        return true;
    }

    bool LoadFromFile(const wchar_t* filePath) {
        std::lock_guard<std::mutex> lock(g_comboMutex);
        FILE* f = nullptr;
        if (_wfopen_s(&f, filePath, L"r") != 0 || !f) return false;

        wchar_t buf[1024];
        if (fwscanf_s(f, L"%1023s", buf, (unsigned)_countof(buf)) != 1) { fclose(f); return false; }
        if (wcscmp(buf, L"HALLJOY_COMBOS_V1") != 0) { fclose(f); return false; }

        size_t count = 0;
        if (fwscanf_s(f, L"%zu", &count) != 1) { fclose(f); return false; }
        fgetwc(f); // Consommer le saut de ligne après le nombre

        g_combos.clear();

        for (size_t i = 0; i < count; ++i) {
            MouseCombo combo;
            // Lire le nom (fgetws lit jusqu'à la fin de la ligne)
            if (!fgetws(buf, 1024, f)) break;
            combo.name = buf;
            // Nettoyer le saut de ligne à la fin
            while (!combo.name.empty() && (combo.name.back() == L'\n' || combo.name.back() == L'\r')) combo.name.pop_back();

            int trig = 0;
            int en = 0, rep = 0, del = 0, loc = 0;
            fwscanf_s(f, L"%d", &trig);
            fwscanf_s(f, L"%d %d %d %d", &en, &rep, &del, &loc);
            
            combo.trigger = (ComboTriggerType)trig;
            combo.enabled = (en != 0);
            combo.repeatWhileHeld = (rep != 0);
            combo.repeatDelayMs = del;
            combo.useLocalRepeat = (loc != 0);

            size_t actionCount = 0;
            fwscanf_s(f, L"%zu", &actionCount);
            
            for (size_t j = 0; j < actionCount; ++j) {
                ComboAction action;
                int type = 0, hid = 0, d = 0, btn = 0;
                fwscanf_s(f, L"%d %d %d %d", &type, &hid, &d, &btn);
                
                action.type = (ComboActionType)type;
                action.keyHid = hid;
                action.delayMs = d;
                action.mouseButton = btn;
                
                fwscanf_s(f, L"%255s", buf, (unsigned)_countof(buf));
                action.text = buf;
                if (action.text == L"__EMPTY__") action.text.clear();
                
                combo.actions.push_back(action);
            }
            fgetwc(f); // Consommer le saut de ligne après la dernière action
            g_combos.push_back(combo);
        }
        fclose(f);
        return true;
    }

    ComboAction MakePressKeyAction(uint16_t hid) { ComboAction a; a.type = ComboActionType::PressKey; a.keyHid = hid; return a; }
    ComboAction MakeReleaseKeyAction(uint16_t hid) { ComboAction a; a.type = ComboActionType::ReleaseKey; a.keyHid = hid; return a; }
    ComboAction MakeTypeTextAction(const std::wstring& text) { ComboAction a; a.type = ComboActionType::TypeText; a.text = text; return a; }
}

// --- CONVERSION HID/VK ---
uint16_t VkToHid(WORD vk) {
    if (vk >= 'A' && vk <= 'Z') return (uint16_t)(vk - 'A' + 4);
    if (vk >= '1' && vk <= '9') return (uint16_t)(vk - '1' + 30);
    if (vk == '0') return 39;
    
    switch (vk) {
        case VK_RETURN: return 40;
        case VK_ESCAPE: return 41;
        case VK_BACK: return 42;
        case VK_TAB: return 43;
        case VK_SPACE: return 44;
        case VK_OEM_MINUS: return 45;
        case VK_OEM_PLUS: return 46;
        case VK_OEM_4: return 47; // [
        case VK_OEM_6: return 48; // ]
        case VK_OEM_5: return 49; // \ 
        case VK_OEM_1: return 51; // ;
        case VK_OEM_7: return 52; // '
        case VK_OEM_3: return 53; // `
        case VK_OEM_COMMA: return 54;
        case VK_OEM_PERIOD: return 55;
        case VK_OEM_2: return 56; // /
        case VK_CAPITAL: return 57;
        case VK_F1: return 58;
        case VK_F2: return 59;
        case VK_F3: return 60;
        case VK_F4: return 61;
        case VK_F5: return 62;
        case VK_F6: return 63;
        case VK_F7: return 64;
        case VK_F8: return 65;
        case VK_F9: return 66;
        case VK_F10: return 67;
        case VK_F11: return 68;
        case VK_F12: return 69;
        case VK_PRINT: return 70;
        case VK_SCROLL: return 71;
        case VK_PAUSE: return 72;
        case VK_INSERT: return 73;
        case VK_HOME: return 74;
        case VK_PRIOR: return 75;
        case VK_DELETE: return 76;
        case VK_END: return 77;
        case VK_NEXT: return 78;
        case VK_RIGHT: return 79;
        case VK_LEFT: return 80;
        case VK_DOWN: return 81;
        case VK_UP: return 82;
        case VK_NUMLOCK: return 83;
        case VK_DIVIDE: return 84;
        case VK_MULTIPLY: return 85;
        case VK_SUBTRACT: return 86;
        case VK_ADD: return 87;
        case VK_NUMPAD1: return 89;
        case VK_NUMPAD2: return 90;
        case VK_NUMPAD3: return 91;
        case VK_NUMPAD4: return 92;
        case VK_NUMPAD5: return 93;
        case VK_NUMPAD6: return 94;
        case VK_NUMPAD7: return 95;
        case VK_NUMPAD8: return 96;
        case VK_NUMPAD9: return 97;
        case VK_NUMPAD0: return 98;
        case VK_DECIMAL: return 99;
        case VK_LCONTROL: return 224;
        case VK_LSHIFT: return 225;
        case VK_LMENU: return 226;
        case VK_LWIN: return 227;
        case VK_RCONTROL: return 228;
        case VK_RSHIFT: return 229;
        case VK_RMENU: return 230;
        case VK_RWIN: return 231;
    }
    return 0;
}

WORD HidToVk(uint16_t hid) {
    if (hid >= 4 && hid <= 29) return (WORD)('A' + (hid - 4));
    if (hid >= 30 && hid <= 38) return (WORD)('1' + (hid - 30));
    if (hid == 39) return '0';
    
    switch (hid) {
        case 40: return VK_RETURN;
        case 41: return VK_ESCAPE;
        case 42: return VK_BACK;
        case 43: return VK_TAB;
        case 44: return VK_SPACE;
        case 45: return VK_OEM_MINUS;
        case 46: return VK_OEM_PLUS;
        case 47: return VK_OEM_4;
        case 48: return VK_OEM_6;
        case 49: return VK_OEM_5;
        case 51: return VK_OEM_1;
        case 52: return VK_OEM_7;
        case 53: return VK_OEM_3;
        case 54: return VK_OEM_COMMA;
        case 55: return VK_OEM_PERIOD;
        case 56: return VK_OEM_2;
        case 57: return VK_CAPITAL;
        case 58: return VK_F1;
        case 59: return VK_F2;
        case 60: return VK_F3;
        case 61: return VK_F4;
        case 62: return VK_F5;
        case 63: return VK_F6;
        case 64: return VK_F7;
        case 65: return VK_F8;
        case 66: return VK_F9;
        case 67: return VK_F10;
        case 68: return VK_F11;
        case 69: return VK_F12;
        case 70: return VK_PRINT;
        case 71: return VK_SCROLL;
        case 72: return VK_PAUSE;
        case 73: return VK_INSERT;
        case 74: return VK_HOME;
        case 75: return VK_PRIOR;
        case 76: return VK_DELETE;
        case 77: return VK_END;
        case 78: return VK_NEXT;
        case 79: return VK_RIGHT;
        case 80: return VK_LEFT;
        case 81: return VK_DOWN;
        case 82: return VK_UP;
        case 83: return VK_NUMLOCK;
        case 84: return VK_DIVIDE;
        case 85: return VK_MULTIPLY;
        case 86: return VK_SUBTRACT;
        case 87: return VK_ADD;
        case 88: return VK_RETURN; // Numpad Enter
        case 89: return VK_NUMPAD1;
        case 90: return VK_NUMPAD2;
        case 91: return VK_NUMPAD3;
        case 92: return VK_NUMPAD4;
        case 93: return VK_NUMPAD5;
        case 94: return VK_NUMPAD6;
        case 95: return VK_NUMPAD7;
        case 96: return VK_NUMPAD8;
        case 97: return VK_NUMPAD9;
        case 98: return VK_NUMPAD0;
        case 99: return VK_DECIMAL;
        case 224: return VK_LCONTROL;
        case 225: return VK_LSHIFT;
        case 226: return VK_LMENU;
        case 227: return VK_LWIN;
        case 228: return VK_RCONTROL;
        case 229: return VK_RSHIFT;
        case 230: return VK_RMENU;
        case 231: return VK_RWIN;
    }
    return 0;
}
