#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <windows.h>

// HallJoy_Bind_Macro
// Provides recording and playback with DIRECT BINDING to mouse/keyboard events
// "Si clic gauche + clic droit → alors appuyer sur P"
// "Si clic droit → alors appuyer sur P"

// Enhanced action types for binding system
enum class HallJoyBindMacroActionType : uint8_t
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
    
    // New composite actions
    MouseLeftPressed = 13,    // Mouse left button is currently pressed
    MouseRightPressed = 14,   // Mouse right button is currently pressed
    KeyPressed = 15,          // Key is currently pressed
    KeyReleased = 16          // Key is currently released
};

// Trigger types for direct binding
enum class HallJoyBindTriggerType : uint8_t
{
    None = 0,
    MouseLeftDown = 1,
    MouseLeftUp = 2,
    MouseRightDown = 3,
    MouseRightUp = 4,
    MouseMiddleDown = 5,
    MouseMiddleUp = 6,
    KeyPress = 7,
    KeyRelease = 8,
    MouseLeftPressed = 9,     // Mouse left is held down
    MouseRightPressed = 10,   // Mouse right is held down
    MouseMiddlePressed = 11,   // Mouse middle is held down
    KeyPressed = 12,          // Key is held down
    KeyReleased = 13,         // Key is released
    Composite = 14            // Multiple triggers combined
};

struct HallJoyBindMacroAction
{
    HallJoyBindMacroActionType type = HallJoyBindMacroActionType::None;
    uint16_t hid = 0;           // HID code for keyboard actions
    POINT mousePos = {0, 0};    // Mouse position for mouse actions
    uint32_t delayMs = 0;       // Delay in milliseconds
    uint32_t timestamp = 0;     // Timestamp from recording start
};

struct HallJoyBindMacroTrigger
{
    HallJoyBindTriggerType type = HallJoyBindTriggerType::None;
    uint16_t hid = 0;           // HID code for key triggers
    uint32_t cooldownMs = 0;    // Cooldown between triggers
    bool isEnabled = true;      // Whether this trigger is active
    uint32_t lastTriggerTime = 0; // Last time this trigger was activated
};

struct HallJoyBindMacroCondition
{
    HallJoyBindTriggerType triggerType = HallJoyBindTriggerType::None;
    uint16_t hid = 0;
    bool isNegated = false;     // NOT condition
    bool requireAll = true;     // AND logic (true) or OR logic (false)
};

struct HallJoyBindMacro
{
    std::wstring name;
    std::vector<HallJoyBindMacroAction> actions;
    std::vector<HallJoyBindMacroTrigger> triggers;
    std::vector<HallJoyBindMacroCondition> conditions;
    
    bool isLooping = false;
    float playbackSpeed = 1.0f;
    bool blockKeysDuringPlayback = true;
    uint32_t totalDuration = 0;
    bool isDirectBinding = true;  // Always true for this system
    
    // Composite trigger support
    bool isComposite = false;     // Multiple triggers combined
    bool requireAllTriggers = true; // AND logic for composite triggers
    
    HallJoyBindMacro() = default;
    HallJoyBindMacro(const std::wstring& macroName) : name(macroName) {}
};

class HallJoy_Bind_Macro
{
public:
    // Macro management
    static int CreateMacro(const std::wstring& name);
    static bool DeleteMacro(int macroId);
    static HallJoyBindMacro* GetMacro(int macroId);
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
    
    // Direct Binding Management
    static bool AddTrigger(int macroId, HallJoyBindTriggerType triggerType, uint16_t hid = 0, uint32_t cooldownMs = 100);
    static bool RemoveTrigger(int macroId, int triggerIndex);
    static bool SetCompositeTrigger(int macroId, bool isComposite, bool requireAll = true);
    static bool EnableDirectBinding(bool enable);
    static bool IsDirectBindingEnabled();
    
    // Composite Triggers - "Si clic gauche + clic droit → alors..."
    static bool CreateCompositeTrigger(int macroId, const std::vector<HallJoyBindTriggerType>& triggers);
    static bool CheckCompositeTrigger(int macroId);
    
    // Conditions - "Conditions multiples comme l'utilisateur le décide"
    static bool AddCondition(int macroId, const HallJoyBindMacroCondition& condition);
    static bool RemoveCondition(int macroId, int conditionIndex);
    static bool SetConditionLogic(int macroId, bool requireAll);
    static bool EvaluateConditions(int macroId);
    
    // Real-time updates (called from main loop)
    static void Tick();
    
    // Input processing (called from hook)
    static void ProcessKeyboardInput(UINT msg, WPARAM wParam, LPARAM lParam);
    static void ProcessMouseInput(UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Trigger checking
    static bool CheckTrigger(int macroId);
    static bool CheckMouseTrigger(HallJoyBindTriggerType triggerType);
    static bool CheckKeyTrigger(HallJoyBindTriggerType triggerType, uint16_t hid);
    
    // Settings
    static void SetBlockKeysDuringPlayback(bool block);
    static bool GetBlockKeysDuringPlayback();
    static void SetLoopMacro(int macroId, bool loop);
    static bool GetLoopMacro(int macroId);
    static void SetGlobalCooldown(uint32_t cooldownMs);
    static uint32_t GetGlobalCooldown();
    
    // Storage
    static bool SaveToFile(const wchar_t* filePath);
    static bool LoadFromFile(const wchar_t* filePath);
    
    // Statistics
    static uint32_t GetMacroExecutionCount(int macroId);
    static uint32_t GetMacroLastExecutionTime(int macroId);
    static void ResetMacroStatistics(int macroId);
    
    // Initialization
    static void Initialize();
    static void Shutdown();
    
private:
    static void ProcessRecordingInput(UINT msg, WPARAM wParam, LPARAM lParam);
    static void ExecuteNextAction();
    static void ExecuteAction(const HallJoyBindMacroAction& action);
    static void SendKeyInput(uint16_t hid, bool down);
    static void SendMouseInput(const HallJoyBindMacroAction& action);
    static void CheckAllTriggers();
    static bool EvaluateCondition(const HallJoyBindMacroCondition& condition);
    
    static std::vector<HallJoyBindMacro> s_macros;
    static int s_recordingMacroId;
    static int s_playingMacroId;
    static uint32_t s_recordingStartTime;
    static uint32_t s_playbackStartTime;
    static size_t s_currentActionIndex;
    static bool s_blockKeysDuringPlayback;
    static float s_playbackSpeed;
    static bool s_directBindingEnabled;
    static uint32_t s_globalCooldown;
    
    // Mouse state tracking for triggers
    static bool s_mouseLeftPressed;
    static bool s_mouseRightPressed;
    static bool s_mouseMiddlePressed;
    
    // Key state tracking
    static std::vector<bool> s_keyStates;
    
    // Statistics
    static std::vector<uint32_t> s_executionCounts;
    static std::vector<uint32_t> s_lastExecutionTimes;
};
