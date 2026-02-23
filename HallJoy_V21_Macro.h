#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

// HallJoy V.2.1 Macro System - Advanced Macro System WITHOUT Direct Binding
// Features: Composite macros, multiple conditions, variables, loops, intervals
// Very intuitive and fast access interface

// ============================================================================
// Advanced Action Types
// ============================================================================

enum class HallJoyV21MacroActionType
{
    // Basic input actions
    KeyPress,
    KeyRelease,
    MouseMove,
    MouseLeftDown,
    MouseLeftUp,
    MouseRightDown,
    MouseRightUp,
    MouseMiddleDown,
    MouseMiddleUp,
    MouseWheelUp,
    MouseWheelDown,
    Delay,
    
    // Advanced actions
    VariableSet,
    VariableAdd,
    VariableSubtract,
    VariableMultiply,
    VariableDivide,
    ConditionalJump,
    LoopStart,
    LoopEnd,
    RandomDelay,
    TextType,
    SoundPlay,
    Notification,
    
    // Composite actions
    CompositeAction
};

// ============================================================================
// Advanced Condition Types
// ============================================================================

enum class HallJoyV21ConditionType
{
    // Variable conditions
    VariableEquals,
    VariableNotEquals,
    VariableGreater,
    VariableLess,
    VariableGreaterEquals,
    VariableLessEquals,
    
    // Input conditions
    KeyPressed,
    KeyNotPressed,
    MouseButtonPressed,
    MouseButtonNotPressed,
    
    // Time conditions
    TimeElapsed,
    RandomChance,
    
    // System conditions
    MacroCount,
    WindowActive,
    ProcessRunning
};

// ============================================================================
// Variable Types
// ============================================================================

enum class HallJoyV21VariableType
{
    Integer,
    Float,
    String,
    Boolean
};

// ============================================================================
// Advanced Action Structure
// ============================================================================

struct HallJoyV21MacroAction
{
    HallJoyV21MacroActionType type;
    uint32_t timestamp;
    
    // Input data
    uint16_t hid;
    POINT mousePos;
    uint32_t delayMs;
    
    // Variable data
    std::wstring variableName;
    int intValue;
    float floatValue;
    std::wstring stringValue;
    bool boolValue;
    
    // Jump data
    uint32_t jumpTarget;
    
    // Loop data
    uint32_t loopCount;
    
    // Composite data
    std::vector<HallJoyV21MacroAction> compositeActions;
    
    // Sound data
    std::wstring soundFile;
    
    // Notification data
    std::wstring message;
    
    // Random delay data
    uint32_t minDelay;
    uint32_t maxDelay;
};

// ============================================================================
// Advanced Condition Structure
// ============================================================================

struct HallJoyV21MacroCondition
{
    HallJoyV21ConditionType type;
    std::wstring variableName;
    int intValue;
    float floatValue;
    std::wstring stringValue;
    bool boolValue;
    uint16_t hid;
    std::wstring button;
    uint32_t timeValue;
    float probability;
    uint32_t count;
    std::wstring windowTitle;
    std::wstring processName;
    bool isNegated;
    bool requireAll; // For multiple conditions (AND logic)
};

// ============================================================================
// Variable Structure
// ============================================================================

struct HallJoyV21MacroVariable
{
    std::wstring name;
    HallJoyV21VariableType type;
    int intValue;
    float floatValue;
    std::wstring stringValue;
    bool boolValue;
};

// ============================================================================
// Macro Structure
// ============================================================================

struct HallJoyV21Macro
{
    std::wstring name;
    std::wstring description;
    std::vector<HallJoyV21MacroAction> actions;
    std::vector<HallJoyV21MacroCondition> conditions;
    std::vector<HallJoyV21MacroVariable> variables;
    
    // Quick access features
    std::wstring category;
    std::wstring hotkey;
    bool isFavorite;
    int priority;
    
    // Statistics
    uint32_t executionCount;
    uint32_t lastExecutionTime;
    uint32_t totalDuration;
    
    // Settings
    bool loopMacro;
    float playbackSpeed;
    bool blockKeysDuringPlayback;
    
    // V.2.1 specific - NO direct binding
    bool isDirectBinding = false;
    
    HallJoyV21Macro(const std::wstring& macroName) : name(macroName), isFavorite(false), priority(0), 
        executionCount(0), lastExecutionTime(0), totalDuration(0), loopMacro(false), 
        playbackSpeed(1.0f, blockKeysDuringPlayback(true) {}
};

// ============================================================================
// Main Class - HallJoy V.2.1 Macro System
// ============================================================================

class HallJoy_V21_Macro
{
public:
    // Macro Management
    static int CreateMacro(const std::wstring& name, const std::wstring& description = L"");
    static bool DeleteMacro(int macroId);
    static HallJoyV21Macro* GetMacro(int macroId);
    static std::vector<int> GetAllMacroIds();
    static int GetMacroCount();
    
    // Quick Access Features
    static bool SetMacroCategory(int macroId, const std::wstring& category);
    static bool SetMacroHotkey(int macroId, const std::wstring& hotkey);
    static bool ToggleFavorite(int macroId);
    static std::vector<int> GetMacrosByCategory(const std::wstring& category);
    static std::vector<int> GetFavoriteMacros();
    static std::vector<int> SearchMacros(const std::wstring& searchTerm);
    
    // Recording
    static bool StartRecording(int macroId);
    static bool StopRecording();
    static bool IsRecording();
    static int GetRecordingMacroId();
    static uint32_t GetRecordingDuration();
    
    // Playback
    static bool StartPlayback(int macroId);
    static bool StopPlayback();
    static bool IsPlaying();
    static int GetPlayingMacroId();
    static void SetPlaybackSpeed(float speed);
    static float GetPlaybackSpeed();
    
    // Advanced Features - Variables
    static bool CreateVariable(int macroId, const std::wstring& name, HallJoyV21VariableType type);
    static bool SetVariable(int macroId, const std::wstring& name, int value);
    static bool SetVariable(int macroId, const std::wstring& name, float value);
    static bool SetVariable(int macroId, const std::wstring& name, const std::wstring& value);
    static bool SetVariable(int macroId, const std::wstring& name, bool value);
    static HallJoyV21MacroVariable* GetVariable(int macroId, const std::wstring& name);
    
    // Advanced Features - Conditions
    static bool AddCondition(int macroId, const HallJoyV21MacroCondition& condition);
    static bool RemoveCondition(int macroId, int conditionIndex);
    static bool EvaluateConditions(int macroId);
    
    // Advanced Features - Actions
    static bool AddAction(int macroId, const HallJoyV21MacroAction& action);
    static bool RemoveAction(int macroId, int actionIndex);
    static bool InsertAction(int macroId, int index, const HallJoyV21MacroAction& action);
    static bool ClearActions(int macroId);
    
    // Advanced Features - Composite Actions
    static bool AddCompositeAction(int macroId, const std::vector<HallJoyV21MacroAction>& actions);
    static bool CreateLoopBlock(int macroId, size_t startIndex, size_t endIndex, uint32_t iterations);
    static bool CreateConditionalJump(int macroId, size_t actionIndex, uint32_t targetIndex, const HallJoyV21MacroCondition& condition);
    
    // Quick Templates for Common Actions
    static int CreateQuickMacro(const std::wstring& name, const std::wstring& templateType);
    static bool AddKeyPressSequence(int macroId, const std::wstring& keySequence);
    static bool AddMouseClickSequence(int macroId, const std::vector<POINT>& clickPositions);
    static bool AddTextMacro(int macroId, const std::wstring& text);
    
    // Real-time Processing
    static void Tick();
    static void ProcessKeyboardInput(UINT msg, WPARAM wParam, LPARAM lParam);
    static void ProcessMouseInput(UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Settings
    static void SetBlockKeysDuringPlayback(bool block);
    static bool GetBlockKeysDuringPlayback();
    static void SetLoopMacro(int macroId, bool loop);
    static bool GetLoopMacro(int macroId);
    
    // Statistics
    static uint32_t GetMacroExecutionCount(int macroId);
    static uint32_t GetMacroLastExecutionTime(int macroId);
    static uint32_t GetMacroActionCount(int macroId);
    static uint32_t GetMacroExecutionTime(int macroId);
    
    // System
    static void Initialize();
    static void Shutdown();
    
private:
    // Static data
    static std::vector<HallJoyV21Macro> s_macros;
    static int s_recordingMacroId;
    static int s_playingMacroId;
    static uint32_t s_recordingStartTime;
    static uint32_t s_playbackStartTime;
    static size_t s_currentActionIndex;
    static bool s_blockKeysDuringPlayback;
    static float s_playbackSpeed;
    
    // V.2.1 Runtime state
    static std::vector<std::vector<HallJoyV21MacroVariable>> s_runtimeVariables;
    static std::vector<uint32_t> s_loopCounters;
    static std::vector<size_t> s_loopStartIndices;
    static std::vector<uint32_t> s_executionTimes;
    static std::vector<uint32_t> s_actionCounts;
    
    // Input state for conditions
    static std::vector<bool> s_keyStates;
    static bool s_mouseLeftPressed;
    static bool s_mouseRightPressed;
    static bool s_mouseMiddlePressed;
    
    // Private methods
    static void ExecuteNextAction();
    static void ExecuteAction(const HallJoyV21MacroAction& action);
    static bool EvaluateCondition(const HallJoyV21MacroCondition& condition);
    static void ProcessRecordingInput(UINT msg, WPARAM wParam, LPARAM lParam);
    static void SendKeyInput(uint16_t hid, bool down);
    static void SendMouseInput(const HallJoyV21MacroAction& action);
    static void PlaySound(const std::wstring& soundFile);
    static void ShowNotification(const std::wstring& message);
    static void TypeText(const std::wstring& text);
    static bool IsKeyPressed(uint16_t hid);
    static bool IsMouseButtonPressed(const std::wstring& button);
    static uint32_t GetCurrentTime();
    static float GetRandomFloat();
};
