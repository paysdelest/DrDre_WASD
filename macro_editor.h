#pragma once
#include "macro_system.h"
#include <windows.h>

// Macro Editor
// Provides UI for editing macro actions and timing

constexpr int MACRO_EDITOR_ID_ACTION_LIST = 9001;
constexpr int MACRO_EDITOR_ID_EDIT_TIMING = 9002;
constexpr int MACRO_EDITOR_ID_DELETE_ACTION = 9003;
constexpr int MACRO_EDITOR_ID_INSERT_DELAY = 9004;
constexpr int MACRO_EDITOR_ID_SCALE_TIMING = 9005;
constexpr int MACRO_EDITOR_ID_SAVE = 9006;
constexpr int MACRO_EDITOR_ID_CANCEL = 9007;

class MacroEditor
{
public:
    // Open editor window for a macro
    static bool OpenEditor(HWND hParent, int macroId);
    
    // Edit timing of a specific action
    static bool EditActionTiming(int macroId, size_t actionIndex, uint32_t newTimestamp);
    
    // Insert a delay between actions
    static bool InsertDelay(int macroId, size_t afterActionIndex, uint32_t delayMs);
    
    // Delete an action
    static bool DeleteAction(int macroId, size_t actionIndex);
    
    // Scale all timings by a factor (e.g., 0.5 = half speed, 2.0 = double speed)
    static bool ScaleAllTimings(int macroId, float factor);
    
    // Adjust timing between two actions
    static bool AdjustTimingBetween(int macroId, size_t startIndex, size_t endIndex, int deltaMs);
    
private:
    static LRESULT CALLBACK EditorWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static void RefreshActionList(HWND hWnd, int macroId);
};

// Emergency Stop System
class MacroEmergencyStop
{
public:
    // Initialize emergency stop hotkey (Ctrl+Alt+Backspace)
    static void Initialize(HWND hWnd);
    
    // Shutdown emergency stop
    static void Shutdown();
    
    // Check if emergency stop was triggered
    static bool WasTriggered();
    
    // Reset trigger flag
    static void ResetTrigger();
    
    // Process hotkey message
    static void ProcessHotkey(WPARAM wParam);
    
private:
    static constexpr int HOTKEY_ID_EMERGENCY_STOP = 1001;
    static bool s_triggered;
    static HWND s_mainWindow;
};
