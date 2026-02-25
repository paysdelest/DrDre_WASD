#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <vector>
#include <string>

// ============================================================
// FREE COMBO SYSTEM - DrDre_WASD v2.0
// Free-trigger combo system (keyboard + mouse)
// Optional modifier + main trigger key
// ============================================================

// Reuse action types from the existing combo system
#include "mouse_combo_system.h"

// Main trigger key type
enum class FreeTriggerKeyType
{
    None = 0,
    Keyboard,       // Keyboard key (VK code)
    MouseLeft,      // Left click
    MouseRight,     // Right click
    MouseMiddle,    // Middle click
    MouseX1,        // Mouse X1 button
    MouseX2,        // Mouse X2 button
    WheelUp,        // Wheel up
    WheelDown,      // Wheel down
    MouseDoubleLeft,   // Double left click
    MouseDoubleRight,  // Double right click
};

// Supported modifiers
enum class FreeTriggerModifier
{
    None = 0,
    Ctrl,
    Shift,
    Alt,
    Win,
};

// Free trigger
struct FreeTrigger
{
    FreeTriggerModifier modifier = FreeTriggerModifier::None;
    FreeTriggerKeyType  keyType  = FreeTriggerKeyType::None;
    WORD                vkCode   = 0;   // For Keyboard: Windows VK code
    FreeTriggerKeyType  holdKeyType = FreeTriggerKeyType::None; // Optional held button/key
    WORD                holdVkCode  = 0; // For holdKeyType == Keyboard
    
    bool IsValid() const { return keyType != FreeTriggerKeyType::None; }
    std::wstring ToString() const;      // Ex: "Ctrl + F" or "Shift + Left click"
};

// Free combo
struct FreeCombo
{
    std::wstring            name;
    FreeTrigger             trigger;
    std::vector<ComboAction> actions;
    bool                    enabled         = true;
    bool                    repeatWhileHeld = false;
    uint32_t                repeatDelayMs   = 400;
    DWORD                   lastExecTime    = 0;
    bool                    isExample       = false; // Marked as first-run example
};

// Free combo management system
namespace FreeComboSystem
{
    void Initialize();
    void Shutdown();

    // CRUD
    int  CreateCombo(const std::wstring& name);
    bool DeleteCombo(int id);
    FreeCombo* GetCombo(int id);
    std::vector<int> GetAllIds();
    int  GetCount();

    // Actions
    bool AddAction(int id, const ComboAction& action);
    bool RemoveAction(int id, int actionIndex);
    bool MoveActionUp(int id, int actionIndex);
    bool MoveActionDown(int id, int actionIndex);
    bool ClearActions(int id);

    // Trigger
    bool SetTrigger(int id, const FreeTrigger& trigger);

    // Options
    bool SetEnabled(int id, bool enabled);
    bool SetRepeat(int id, bool repeat, uint32_t delayMs);

    // Trigger capture
    // Call StartCapture(), wait until IsCaptureComplete() is true,
    // then call GetCapturedTrigger()
    void StartCapture();
    void StopCapture();
    bool IsCapturing();
    bool IsCaptureComplete();
    FreeTrigger GetCapturedTrigger();
    bool IsCaptureWaitingSecondInput();
    FreeTrigger GetCaptureFirstInput();

    // Events (called by existing hooks)
    void ProcessMouseEvent(UINT msg, WPARAM wParam, LPARAM lParam);
    bool ProcessKeyboardEvent(UINT msg, WPARAM wParam, LPARAM lParam); // true => event consumed

    // Tick for repeats
    void Tick();

    // Save / Load
    bool SaveToFile(const wchar_t* path);
    bool LoadFromFile(const wchar_t* path);

    // Create first-run example combos
    void CreateExampleCombos();
    bool HasExampleCombos();
}

// Naming helpers
std::wstring FreeTriggerModifierToString(FreeTriggerModifier mod);
std::wstring FreeTriggerKeyTypeToString(FreeTriggerKeyType type, WORD vk);
WORD VkFromChar(wchar_t c);
