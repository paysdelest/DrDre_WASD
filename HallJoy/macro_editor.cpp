// macro_editor.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include "macro_editor.h"
#include "macro_system.h"
#include "ui_theme.h"

// ============================================================================
// Emergency Stop System
// ============================================================================

bool MacroEmergencyStop::s_triggered = false;
HWND MacroEmergencyStop::s_mainWindow = nullptr;

void MacroEmergencyStop::Initialize(HWND hWnd)
{
    s_mainWindow = hWnd;
    s_triggered = false;
    
    // Register Ctrl+Alt+Backspace hotkey
    RegisterHotKey(hWnd, HOTKEY_ID_EMERGENCY_STOP, 
                   MOD_CONTROL | MOD_ALT, VK_BACK);
}

void MacroEmergencyStop::Shutdown()
{
    if (s_mainWindow)
    {
        UnregisterHotKey(s_mainWindow, HOTKEY_ID_EMERGENCY_STOP);
        s_mainWindow = nullptr;
    }
}

bool MacroEmergencyStop::WasTriggered()
{
    return s_triggered;
}

void MacroEmergencyStop::ResetTrigger()
{
    s_triggered = false;
}

void MacroEmergencyStop::ProcessHotkey(WPARAM wParam)
{
    if (wParam == HOTKEY_ID_EMERGENCY_STOP)
    {
        s_triggered = true;
        
        // Stop any recording or playback immediately
        if (MacroSystem::IsRecording())
        {
            MacroSystem::StopRecording();
            MessageBoxW(s_mainWindow, 
                       L"Recording cancelled by emergency stop (Ctrl+Alt+Backspace)",
                       L"Emergency Stop", MB_OK | MB_ICONWARNING);
        }
        
        if (MacroSystem::IsPlaying())
        {
            MacroSystem::StopPlayback();
            MessageBoxW(s_mainWindow, 
                       L"Playback cancelled by emergency stop (Ctrl+Alt+Backspace)",
                       L"Emergency Stop", MB_OK | MB_ICONWARNING);
        }
    }
}

// ============================================================================
// Macro Editor Functions
// ============================================================================

bool MacroEditor::EditActionTiming(int macroId, size_t actionIndex, uint32_t newTimestamp)
{
    Macro* macro = MacroSystem::GetMacro(macroId);
    if (!macro || actionIndex >= macro->actions.size())
        return false;
    
    macro->actions[actionIndex].timestamp = newTimestamp;
    
    // Recalculate total duration
    if (!macro->actions.empty())
    {
        uint32_t maxTime = 0;
        for (const auto& action : macro->actions)
        {
            if (action.timestamp > maxTime)
                maxTime = action.timestamp;
        }
        macro->totalDuration = maxTime;
    }
    
    return true;
}

bool MacroEditor::InsertDelay(int macroId, size_t afterActionIndex, uint32_t delayMs)
{
    Macro* macro = MacroSystem::GetMacro(macroId);
    if (!macro || afterActionIndex >= macro->actions.size())
        return false;
    
    // Shift all actions after this point by delayMs
    uint32_t insertTime = macro->actions[afterActionIndex].timestamp;
    
    for (size_t i = afterActionIndex + 1; i < macro->actions.size(); ++i)
    {
        macro->actions[i].timestamp += delayMs;
    }
    
    // Insert a delay action
    MacroAction delayAction;
    delayAction.type = MacroActionType::Delay;
    delayAction.timestamp = insertTime + delayMs;
    delayAction.delayMs = delayMs;
    
    macro->actions.insert(macro->actions.begin() + afterActionIndex + 1, delayAction);
    
    // Update total duration
    if (!macro->actions.empty())
        macro->totalDuration = macro->actions.back().timestamp;
    
    return true;
}

bool MacroEditor::DeleteAction(int macroId, size_t actionIndex)
{
    Macro* macro = MacroSystem::GetMacro(macroId);
    if (!macro || actionIndex >= macro->actions.size())
        return false;
    
    macro->actions.erase(macro->actions.begin() + actionIndex);
    
    // Update total duration
    if (!macro->actions.empty())
        macro->totalDuration = macro->actions.back().timestamp;
    else
        macro->totalDuration = 0;
    
    return true;
}

bool MacroEditor::ScaleAllTimings(int macroId, float factor)
{
    Macro* macro = MacroSystem::GetMacro(macroId);
    if (!macro || factor <= 0.0f)
        return false;
    
    for (auto& action : macro->actions)
    {
        action.timestamp = (uint32_t)(action.timestamp * factor);
        if (action.type == MacroActionType::Delay)
            action.delayMs = (uint32_t)(action.delayMs * factor);
    }
    
    macro->totalDuration = (uint32_t)(macro->totalDuration * factor);
    
    return true;
}

bool MacroEditor::AdjustTimingBetween(int macroId, size_t startIndex, size_t endIndex, int deltaMs)
{
    Macro* macro = MacroSystem::GetMacro(macroId);
    if (!macro || startIndex >= macro->actions.size() || endIndex >= macro->actions.size())
        return false;
    
    if (startIndex > endIndex)
        std::swap(startIndex, endIndex);
    
    // Adjust timing for actions in range
    for (size_t i = startIndex + 1; i <= endIndex; ++i)
    {
        int newTime = (int)macro->actions[i].timestamp + deltaMs;
        if (newTime < 0)
            newTime = 0;
        macro->actions[i].timestamp = (uint32_t)newTime;
    }
    
    // Shift all actions after endIndex
    for (size_t i = endIndex + 1; i < macro->actions.size(); ++i)
    {
        int newTime = (int)macro->actions[i].timestamp + deltaMs;
        if (newTime < 0)
            newTime = 0;
        macro->actions[i].timestamp = (uint32_t)newTime;
    }
    
    // Update total duration
    if (!macro->actions.empty())
        macro->totalDuration = macro->actions.back().timestamp;
    
    return true;
}

// ============================================================================
// Editor Window
// ============================================================================

struct EditorState
{
    int macroId = -1;
    HWND hActionList = nullptr;
    HWND hTimingEdit = nullptr;
    int selectedActionIndex = -1;
};

void MacroEditor::RefreshActionList(HWND hWnd, int macroId)
{
    EditorState* state = (EditorState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state || !state->hActionList)
        return;
    
    Macro* macro = MacroSystem::GetMacro(macroId);
    if (!macro)
        return;
    
    // Clear list
    SendMessageW(state->hActionList, LB_RESETCONTENT, 0, 0);
    
    // Add actions
    for (size_t i = 0; i < macro->actions.size(); ++i)
    {
        const MacroAction& action = macro->actions[i];
        
        wchar_t line[256];
        float seconds = action.timestamp / 1000.0f;
        
        switch (action.type)
        {
        case MacroActionType::KeyPress:
            swprintf_s(line, L"[%03zu] %06.3fs - Touche PRESSEE (HID: 0x%04X)", 
                      i + 1, seconds, action.hid);
            break;
        case MacroActionType::KeyRelease:
            swprintf_s(line, L"[%03zu] %06.3fs - Touche RELACHEE (HID: 0x%04X)", 
                      i + 1, seconds, action.hid);
            break;
        case MacroActionType::MouseLeftDown:
            swprintf_s(line, L"[%03zu] %06.3fs - Clic GAUCHE DOWN a (%d, %d)", 
                      i + 1, seconds, action.mousePos.x, action.mousePos.y);
            break;
        case MacroActionType::MouseLeftUp:
            swprintf_s(line, L"[%03zu] %06.3fs - Clic GAUCHE UP a (%d, %d)", 
                      i + 1, seconds, action.mousePos.x, action.mousePos.y);
            break;
        case MacroActionType::MouseRightDown:
            swprintf_s(line, L"[%03zu] %06.3fs - Clic DROIT DOWN a (%d, %d)", 
                      i + 1, seconds, action.mousePos.x, action.mousePos.y);
            break;
        case MacroActionType::MouseRightUp:
            swprintf_s(line, L"[%03zu] %06.3fs - Clic DROIT UP a (%d, %d)", 
                      i + 1, seconds, action.mousePos.x, action.mousePos.y);
            break;
        case MacroActionType::Delay:
            swprintf_s(line, L"[%03zu] %06.3fs - DELAI de %u ms", 
                      i + 1, seconds, action.delayMs);
            break;
        default:
            swprintf_s(line, L"[%03zu] %06.3fs - Action inconnue", i + 1, seconds);
            break;
        }
        
        SendMessageW(state->hActionList, LB_ADDSTRING, 0, (LPARAM)line);
    }
}

LRESULT CALLBACK MacroEditor::EditorWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    EditorState* state = (EditorState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    
    switch (msg)
    {
    case WM_CREATE:
        {
            state = new EditorState();
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)state);
            
            // Create controls
            int x = 10, y = 10;
            int width = 600, height = 25;
            
            // Action list
            CreateWindowW(L"STATIC", L"Actions de la macro:",
                         WS_CHILD | WS_VISIBLE,
                         x, y, width, height, hWnd, nullptr, nullptr, nullptr);
            y += height + 5;
            
            state->hActionList = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
                x, y, width, 300, hWnd, 
                (HMENU)(uintptr_t)MACRO_EDITOR_ID_ACTION_LIST, nullptr, nullptr);
            y += 305;
            
            // Timing edit
            CreateWindowW(L"STATIC", L"Nouveau timing (ms):",
                         WS_CHILD | WS_VISIBLE,
                         x, y, 150, height, hWnd, nullptr, nullptr, nullptr);
            
            state->hTimingEdit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"0",
                WS_CHILD | WS_VISIBLE | ES_NUMBER,
                x + 155, y, 100, height, hWnd, 
                (HMENU)(uintptr_t)MACRO_EDITOR_ID_EDIT_TIMING, nullptr, nullptr);
            y += height + 10;
            
            // Buttons
            CreateWindowW(L"BUTTON", L"Edit timing",
                         WS_CHILD | WS_VISIBLE,
                         x, y, 120, 30, hWnd, 
                         (HMENU)(uintptr_t)MACRO_EDITOR_ID_EDIT_TIMING, nullptr, nullptr);
            
            CreateWindowW(L"BUTTON", L"Insert delay",
                         WS_CHILD | WS_VISIBLE,
                         x + 130, y, 120, 30, hWnd, 
                         (HMENU)(uintptr_t)MACRO_EDITOR_ID_INSERT_DELAY, nullptr, nullptr);
            
            CreateWindowW(L"BUTTON", L"Delete",
                         WS_CHILD | WS_VISIBLE,
                         x + 260, y, 120, 30, hWnd, 
                         (HMENU)(uintptr_t)MACRO_EDITOR_ID_DELETE_ACTION, nullptr, nullptr);
            
            CreateWindowW(L"BUTTON", L"Close",
                         WS_CHILD | WS_VISIBLE,
                         x + 390, y, 120, 30, hWnd, 
                         (HMENU)(uintptr_t)MACRO_EDITOR_ID_CANCEL, nullptr, nullptr);
        }
        return 0;
        
    case WM_COMMAND:
        {
            int id = LOWORD(wParam);
            
            switch (id)
            {
            case MACRO_EDITOR_ID_ACTION_LIST:
                if (HIWORD(wParam) == LBN_SELCHANGE)
                {
                    state->selectedActionIndex = (int)SendMessageW(state->hActionList, LB_GETCURSEL, 0, 0);
                    
                    if (state->selectedActionIndex >= 0)
                    {
                        Macro* macro = MacroSystem::GetMacro(state->macroId);
                        if (macro && state->selectedActionIndex < (int)macro->actions.size())
                        {
                            uint32_t timestamp = macro->actions[state->selectedActionIndex].timestamp;
                            wchar_t buf[32];
                            swprintf_s(buf, L"%u", timestamp);
                            SetWindowTextW(state->hTimingEdit, buf);
                        }
                    }
                }
                break;
                
            case MACRO_EDITOR_ID_EDIT_TIMING:
                if (state->selectedActionIndex >= 0)
                {
                    wchar_t buf[32];
                    GetWindowTextW(state->hTimingEdit, buf, 32);
                    uint32_t newTime = (uint32_t)_wtoi(buf);
                    
                    if (EditActionTiming(state->macroId, state->selectedActionIndex, newTime))
                    {
                        RefreshActionList(hWnd, state->macroId);
                        MessageBoxW(hWnd, L"Timing modifie avec succes", L"Succes", MB_OK);
                    }
                }
                break;
                
            case MACRO_EDITOR_ID_INSERT_DELAY:
                if (state->selectedActionIndex >= 0)
                {
                    wchar_t buf[32];
                    GetWindowTextW(state->hTimingEdit, buf, 32);
                    uint32_t delayMs = (uint32_t)_wtoi(buf);
                    
                    if (InsertDelay(state->macroId, state->selectedActionIndex, delayMs))
                    {
                        RefreshActionList(hWnd, state->macroId);
                        MessageBoxW(hWnd, L"Delai insere avec succes", L"Succes", MB_OK);
                    }
                }
                break;
                
            case MACRO_EDITOR_ID_DELETE_ACTION:
                if (state->selectedActionIndex >= 0)
                {
                    if (DeleteAction(state->macroId, state->selectedActionIndex))
                    {
                        RefreshActionList(hWnd, state->macroId);
                        state->selectedActionIndex = -1;
                        MessageBoxW(hWnd, L"Action deleted successfully", L"Success", MB_OK);
                    }
                }
                break;
                
            case MACRO_EDITOR_ID_CANCEL:
                DestroyWindow(hWnd);
                break;
            }
        }
        return 0;
        
    case WM_DESTROY:
        if (state)
        {
            delete state;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;
    }
    
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool MacroEditor::OpenEditor(HWND hParent, int macroId)
{
    Macro* macro = MacroSystem::GetMacro(macroId);
    if (!macro)
        return false;
    
    // Register window class
    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = EditorWndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = L"MacroEditorWindow";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        
        RegisterClassW(&wc);
        classRegistered = true;
    }
    
    // Create window
    std::wstring title = L"Editeur de Macro - " + macro->name;
    
    HWND hWnd = CreateWindowExW(
        0, L"MacroEditorWindow", title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 500,
        hParent, nullptr, GetModuleHandle(nullptr), nullptr);
    
    if (!hWnd)
        return false;
    
    EditorState* state = (EditorState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (state)
    {
        state->macroId = macroId;
        RefreshActionList(hWnd, macroId);
    }
    
    return true;
}
