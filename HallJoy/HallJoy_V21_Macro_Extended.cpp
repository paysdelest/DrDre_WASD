#include "HallJoy_V21_Macro.h"
#include "win_util.h"
#include <string>
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <sstream>

// ============================================================================
// Recording
// ============================================================================

bool HallJoy_V21_Macro::StartRecording(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    if (s_recordingMacroId != -1)
        StopRecording();
    
    s_recordingMacroId = macroId;
    s_recordingStartTime = GetTickCount();
    
    HallJoyV21Macro* macro = GetMacro(s_recordingMacroId);
    if (macro)
    {
        macro->actions.clear();
        macro->totalDuration = 0;
    }
    
    return true;
}

bool HallJoy_V21_Macro::StopRecording()
{
    if (s_recordingMacroId == -1)
        return false;
    
    HallJoyV21Macro* macro = GetMacro(s_recordingMacroId);
    if (macro && !macro->actions.empty())
    {
        uint32_t totalTime = GetTickCount() - s_recordingStartTime;
        macro->totalDuration = totalTime;
    }
    
    s_recordingMacroId = -1;
    return true;
}

bool HallJoy_V21_Macro::IsRecording()
{
    return s_recordingMacroId != -1;
}

int HallJoy_V21_Macro::GetRecordingMacroId()
{
    return s_recordingMacroId;
}

uint32_t HallJoy_V21_Macro::GetRecordingDuration()
{
    if (s_recordingMacroId == -1)
        return 0;
    
    return GetTickCount() - s_recordingStartTime;
}

// ============================================================================
// Playback
// ============================================================================

bool HallJoy_V21_Macro::StartPlayback(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    HallJoyV21Macro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    if (s_playingMacroId != -1)
        StopPlayback();
    
    s_playingMacroId = macroId;
    s_currentActionIndex = 0;
    s_playbackStartTime = GetTickCount();
    
    // Reset loop counter
    if (macroId < (int)s_loopCounters.size())
        s_loopCounters[macroId] = 0;
    
    // Update statistics
    macro->executionCount++;
    macro->lastExecutionTime = GetTickCount();
    
    return true;
}

bool HallJoy_V21_Macro::StopPlayback()
{
    s_playingMacroId = -1;
    s_currentActionIndex = 0;
    return true;
}

bool HallJoy_V21_Macro::IsPlaying()
{
    return s_playingMacroId != -1;
}

int HallJoy_V21_Macro::GetPlayingMacroId()
{
    return s_playingMacroId;
}

void HallJoy_V21_Macro::SetPlaybackSpeed(float speed)
{
    s_playbackSpeed = std::max(0.1f, std::min(5.0f, speed));
}

float HallJoy_V21_Macro::GetPlaybackSpeed()
{
    return s_playbackSpeed;
}

// ============================================================================
// Quick Templates for Common Actions
// ============================================================================

int HallJoy_V21_Macro::CreateQuickMacro(const std::wstring& name, const std::wstring& templateType)
{
    int macroId = CreateMacro(name, L"Quick macro from template: " + templateType);
    HallJoyV21Macro* macro = GetMacro(macroId);
    if (!macro)
        return -1;
    
    if (templateType == L"ping")
    {
        // Template: Press P key
        HallJoyV21MacroAction action;
        action.type = HallJoyV21MacroActionType::KeyPress;
        action.hid = 0x50; // P key
        action.timestamp = 50;
        macro->actions.push_back(action);
        
        action.type = HallJoyV21MacroActionType::KeyRelease;
        action.timestamp = 100;
        macro->actions.push_back(action);
        
        macro->description = L"Quick ping macro - Press P key";
    }
    else if (templateType == L"hello")
    {
        // Template: Type "Hello"
        AddTextMacro(macroId, L"Hello");
        macro->description = L"Type 'Hello' text";
    }
    else if (templateType == L"double_click")
    {
        // Template: Double left click
        HallJoyV21MacroAction action;
        action.type = HallJoyV21MacroActionType::MouseLeftDown;
        action.timestamp = 50;
        macro->actions.push_back(action);
        
        action.type = HallJoyV21MacroActionType::MouseLeftUp;
        action.timestamp = 100;
        macro->actions.push_back(action);
        
        action.type = HallJoyV21MacroActionType::MouseLeftDown;
        action.timestamp = 200;
        macro->actions.push_back(action);
        
        action.type = HallJoyV21MacroActionType::MouseLeftUp;
        action.timestamp = 250;
        macro->actions.push_back(action);
        
        macro->description = L"Double left click";
    }
    
    return macroId;
}

bool HallJoy_V21_Macro::AddKeyPressSequence(int macroId, const std::wstring& keySequence)
{
    HallJoyV21Macro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    uint32_t timestamp = 50;
    for (wchar_t ch : keySequence)
    {
        // Simple mapping - you'd want a more sophisticated keyboard mapping
        uint16_t hid = (uint16_t)ch;
        
        HallJoyV21MacroAction action;
        action.type = HallJoyV21MacroActionType::KeyPress;
        action.hid = hid;
        action.timestamp = timestamp;
        macro->actions.push_back(action);
        
        action.type = HallJoyV21MacroActionType::KeyRelease;
        action.timestamp = timestamp + 50;
        macro->actions.push_back(action);
        
        timestamp += 100;
    }
    
    return true;
}

bool HallJoy_V21_Macro::AddMouseClickSequence(int macroId, const std::vector<POINT>& clickPositions)
{
    HallJoyV21Macro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    uint32_t timestamp = 50;
    for (const POINT& pos : clickPositions)
    {
        HallJoyV21MacroAction action;
        
        // Move to position
        action.type = HallJoyV21MacroActionType::MouseMove;
        action.mousePos = pos;
        action.timestamp = timestamp;
        macro->actions.push_back(action);
        
        // Click down
        action.type = HallJoyV21MacroActionType::MouseLeftDown;
        action.timestamp = timestamp + 50;
        macro->actions.push_back(action);
        
        // Click up
        action.type = HallJoyV21MacroActionType::MouseLeftUp;
        action.timestamp = timestamp + 100;
        macro->actions.push_back(action);
        
        timestamp += 200;
    }
    
    return true;
}

bool HallJoy_V21_Macro::AddTextMacro(int macroId, const std::wstring& text)
{
    HallJoyV21Macro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    HallJoyV21MacroAction action;
    action.type = HallJoyV21MacroActionType::TextType;
    action.stringValue = text;
    action.timestamp = 50;
    macro->actions.push_back(action);
    
    return true;
}

// ============================================================================
// Real-time Processing
// ============================================================================

void HallJoy_V21_Macro::Tick()
{
    // Process playback
    if (s_playingMacroId != -1)
    {
        ExecuteNextAction();
    }
}

void HallJoy_V21_Macro::ProcessKeyboardInput(UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Update key state tracking
    if (wParam < 256)
    {
        bool isDown = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
        s_keyStates[wParam] = isDown;
    }
    
    // Process recording
    if (s_recordingMacroId != -1)
    {
        ProcessRecordingInput(msg, wParam, lParam);
    }
}

void HallJoy_V21_Macro::ProcessMouseInput(UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Update mouse state tracking
    switch (msg)
    {
    case WM_LBUTTONDOWN:
        s_mouseLeftPressed = true;
        break;
    case WM_LBUTTONUP:
        s_mouseLeftPressed = false;
        break;
    case WM_RBUTTONDOWN:
        s_mouseRightPressed = true;
        break;
    case WM_RBUTTONUP:
        s_mouseRightPressed = false;
        break;
    case WM_MBUTTONDOWN:
        s_mouseMiddlePressed = true;
        break;
    case WM_MBUTTONUP:
        s_mouseMiddlePressed = false;
        break;
    }
    
    // Process recording
    if (s_recordingMacroId != -1)
    {
        ProcessRecordingInput(msg, wParam, lParam);
    }
}

// ============================================================================
// Initialization
// ============================================================================

void HallJoy_V21_Macro::Initialize()
{
    s_macros.clear();
    s_recordingMacroId = -1;
    s_playingMacroId = -1;
    s_currentActionIndex = 0;
    s_blockKeysDuringPlayback = true;
    s_playbackSpeed = 1.0f;
    
    // Reset mouse state
    s_mouseLeftPressed = false;
    s_mouseRightPressed = false;
    s_mouseMiddlePressed = false;
    
    // Reset key state
    std::fill(s_keyStates.begin(), s_keyStates.end(), false);
    
    // Clear runtime data
    s_runtimeVariables.clear();
    s_loopCounters.clear();
    s_loopStartIndices.clear();
    s_executionTimes.clear();
    s_actionCounts.clear();
}

void HallJoy_V21_Macro::Shutdown()
{
    StopRecording();
    StopPlayback();
    s_macros.clear();
    s_runtimeVariables.clear();
    s_loopCounters.clear();
    s_loopStartIndices.clear();
    s_executionTimes.clear();
    s_actionCounts.clear();
}
