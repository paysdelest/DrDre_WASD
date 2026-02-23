#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <windows.h>

// Advanced Macro System with Direct Binding for HallJoy
// Provides conditional macros, direct binding, and composite actions

// Extended action types for advanced macros
enum class BindMacroActionType : uint8_t
{
    None = 0,
    KeyPress = 1,
    KeyRelease = 2,
    MouseMove = 3,
    MouseLeftDown = 4,
    MouseLeftUp = 5,
    MouseRightDown = 6,
    MouseRightUp = 7,
    MouseMiddleDown = 8,
    MouseMiddleUp = 9,
    MouseWheelUp = 10,
    MouseWheelDown = 11,
    Delay = 12,
    
    // New advanced action types
    ConditionalStart = 13,    // Start of conditional block
    ConditionalEnd = 14,      // End of conditional block
    MouseLeftPressed = 15,    // Check if left mouse is pressed
    MouseRightPressed = 16,   // Check if right mouse is pressed
    MouseButtonPressed = 17,  // Check if specific mouse button is pressed
    KeyIsPressed = 18,        // Check if specific key is pressed
    LogicalAND = 19,          // Logical AND operator
    LogicalOR = 20,           // Logical OR operator
    LogicalNOT = 21           // Logical NOT operator
};

// Trigger types for direct binding
enum class BindTriggerType : uint8_t
{
    None = 0,
    MouseLeftDown = 1,
    MouseLeftUp = 2,
    MouseRightDown = 3,
    MouseRightUp = 4,
    MouseLeftPressed = 5,    // While held down
    MouseRightPressed = 6,   // While held down
    KeyPress = 7,
    KeyRelease = 8,
    KeyPressed = 9,          // While held down
    Composite = 10           // Multiple conditions
};

struct BindMacroAction
{
    BindMacroActionType type = BindMacroActionType::None;
    uint16_t hid = 0;           // HID code for keyboard actions
    POINT mousePos = {0, 0};    // Mouse position for mouse actions
    uint32_t delayMs = 0;       // Delay in milliseconds
    uint32_t timestamp = 0;     // Timestamp from recording start
    
    // New fields for advanced features
    uint16_t conditionValue = 0;    // Value for condition checks
    bool conditionResult = false;   // Result of condition evaluation
    std::vector<BindMacroAction> nestedActions; // For conditional blocks
};

struct BindMacroTrigger
{
    BindTriggerType type = BindTriggerType::None;
    uint16_t hid = 0;           // HID code for keyboard triggers
    bool isActive = false;      // Whether trigger is currently active
    uint32_t lastTriggerTime = 0; // Last time this trigger was activated
    uint32_t cooldownMs = 100;  // Cooldown between triggers
};

struct BindMacro
{
    std::wstring name;
    std::vector<BindMacroAction> actions;
    std::vector<BindMacroTrigger> triggers;
    
    // Standard macro properties
    bool isLooping = false;
    float playbackSpeed = 1.0f;
    bool blockKeysDuringPlayback = true;
    uint32_t totalDuration = 0;
    
    // Advanced properties
    bool isDirectBinding = false;     // Whether this macro uses direct binding
    bool allowMultipleTriggers = true; // Can be triggered multiple times rapidly
    bool requiresAllConditions = true; // AND vs OR for multiple triggers
    
    BindMacro() = default;
    BindMacro(const std::wstring& macroName) : name(macroName) {}
};

class BindMacroSystem
{
public:
    // Macro management
    static int CreateMacro(const std::wstring& name);
    static bool DeleteMacro(int macroId);
    static BindMacro* GetMacro(int macroId);
    static std::vector<int> GetAllMacroIds();
    static int GetMacroCount();
    
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
    
    // Direct binding management
    static bool AddTrigger(int macroId, BindTriggerType triggerType, uint16_t hid = 0);
    static bool RemoveTrigger(int macroId, BindTriggerType triggerType, uint16_t hid = 0);
    static bool SetTriggerCooldown(int macroId, BindTriggerType triggerType, uint32_t cooldownMs);
    static void EnableDirectBinding(bool enable);
    static bool IsDirectBindingEnabled();
    
    // Conditional macro creation
    static bool CreateConditionalMacro(int macroId, const std::wstring& condition);
    static bool AddConditionalAction(int macroId, const BindMacroAction& action);
    static bool CreateCompositeTrigger(int macroId, const std::vector<BindTriggerType>& triggers);
    
    // Real-time updates (called from main loop)
    static void Tick();
    
    // Input processing (called from hook)
    static void ProcessKeyboardInput(UINT msg, WPARAM wParam, LPARAM lParam);
    static void ProcessMouseInput(UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Settings
    static void SetBlockKeysDuringPlayback(bool block);
    static bool GetBlockKeysDuringPlayback();
    static void SetLoopMacro(int macroId, bool loop);
    static bool GetLoopMacro(int macroId);
    
    // Storage
    static bool SaveToFile(const wchar_t* filePath);
    static bool LoadFromFile(const wchar_t* filePath);
    
    // Initialization
    static void Initialize();
    static void Shutdown();
    
    // Advanced features
    static bool EvaluateCondition(const BindMacroAction& condition);
    static bool CheckTriggers(int macroId);
    static void ExecuteConditionalMacro(int macroId);
    
private:
    // Core functionality
    static void ProcessRecordingInput(UINT msg, WPARAM wParam, LPARAM lParam);
    static void ExecuteNextAction();
    static void SendKeyInput(uint16_t hid, bool down);
    static void SendMouseInput(const BindMacroAction& action);
    
    // Direct binding processing
    static void ProcessDirectBindings(UINT msg, WPARAM wParam, LPARAM lParam);
    static bool ShouldTriggerMacro(int macroId, UINT msg, WPARAM wParam, LPARAM lParam);
    static void TriggerMacro(int macroId);
    
    // Conditional processing
    static bool EvaluateLogicalExpression(const std::vector<BindMacroAction>& actions);
    static bool CheckMouseCondition(BindMacroActionType condition);
    static bool CheckKeyCondition(BindMacroActionType condition, uint16_t hid);
    
    // State management
    static std::vector<BindMacro> s_macros;
    static int s_recordingMacroId;
    static int s_playingMacroId;
    static uint32_t s_recordingStartTime;
    static uint32_t s_playbackStartTime;
    static size_t s_currentActionIndex;
    static bool s_blockKeysDuringPlayback;
    static float s_playbackSpeed;
    static bool s_directBindingEnabled;
    
    // Mouse state tracking for direct binding
    static bool s_mouseLeftPressed;
    static bool s_mouseRightPressed;
    static bool s_mouseMiddlePressed;
    static uint32_t s_mouseLeftPressTime;
    static uint32_t s_mouseRightPressTime;
    static uint32_t s_mouseMiddlePressTime;
    
    // Key state tracking for direct binding
    static std::vector<bool> s_keyStates;
    static std::vector<uint32_t> s_keyPressTimes;
};
