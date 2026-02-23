#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <windows.h>

// Macro System for HallJoy
// Provides recording and playback of keyboard/mouse sequences

enum class MacroActionType : uint8_t
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
    Delay = 12
};

struct MacroAction
{
    MacroActionType type = MacroActionType::None;
    uint16_t hid = 0;           // HID code for keyboard actions
    POINT mousePos = {0, 0};    // Mouse position for mouse actions
    uint32_t delayMs = 0;       // Delay in milliseconds
    uint32_t timestamp = 0;     // Timestamp from recording start
};

struct Macro
{
    std::wstring name;
    std::vector<MacroAction> actions;
    bool isLooping = false;
    float playbackSpeed = 1.0f;
    bool blockKeysDuringPlayback = true;
    uint32_t totalDuration = 0;  // Total duration in milliseconds
    
    Macro() = default;
    Macro(const std::wstring& macroName) : name(macroName) {}
};

class MacroSystem
{
public:
    // Macro management
    static int CreateMacro(const std::wstring& name);
    static bool DeleteMacro(int macroId);
    static Macro* GetMacro(int macroId);
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
    
private:
    static void ProcessRecordingInput(UINT msg, WPARAM wParam, LPARAM lParam);
    static void ExecuteNextAction();
    static void SendKeyInput(uint16_t hid, bool down);
    static void SendMouseInput(const MacroAction& action);
    
    static std::vector<Macro> s_macros;
    static int s_recordingMacroId;
    static int s_playingMacroId;
    static uint32_t s_recordingStartTime;
    static uint32_t s_playbackStartTime;
    static size_t s_currentActionIndex;
    static bool s_blockKeysDuringPlayback;
    static float s_playbackSpeed;
};
