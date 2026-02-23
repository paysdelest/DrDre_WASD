#include "macro_bind_system.h"
#include <fstream>
#include <algorithm>
#include <iostream>

// Static member initialization
std::vector<BindMacro> BindMacroSystem::s_macros;
int BindMacroSystem::s_recordingMacroId = -1;
int BindMacroSystem::s_playingMacroId = -1;
uint32_t BindMacroSystem::s_recordingStartTime = 0;
uint32_t BindMacroSystem::s_playbackStartTime = 0;
size_t BindMacroSystem::s_currentActionIndex = 0;
bool BindMacroSystem::s_blockKeysDuringPlayback = true;
float BindMacroSystem::s_playbackSpeed = 1.0f;
bool BindMacroSystem::s_directBindingEnabled = false;

// Mouse state tracking
bool BindMacroSystem::s_mouseLeftPressed = false;
bool BindMacroSystem::s_mouseRightPressed = false;
bool BindMacroSystem::s_mouseMiddlePressed = false;
uint32_t BindMacroSystem::s_mouseLeftPressTime = 0;
uint32_t BindMacroSystem::s_mouseRightPressTime = 0;
uint32_t BindMacroSystem::s_mouseMiddlePressTime = 0;

// Key state tracking
std::vector<bool> BindMacroSystem::s_keyStates(256, false);
std::vector<uint32_t> BindMacroSystem::s_keyPressTimes(256, 0);

// ============================================================================
// Macro Management
// ============================================================================

int BindMacroSystem::CreateMacro(const std::wstring& name)
{
    BindMacro macro(name);
    s_macros.push_back(macro);
    return (int)s_macros.size() - 1;
}

bool BindMacroSystem::DeleteMacro(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    // Stop recording/playing if this macro is active
    if (s_recordingMacroId == macroId)
        StopRecording();
    if (s_playingMacroId == macroId)
        StopPlayback();
    
    s_macros.erase(s_macros.begin() + macroId);
    return true;
}

BindMacro* BindMacroSystem::GetMacro(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return nullptr;
    
    return &s_macros[macroId];
}

std::vector<int> BindMacroSystem::GetAllMacroIds()
{
    std::vector<int> ids;
    for (int i = 0; i < (int)s_macros.size(); i++)
    {
        ids.push_back(i);
    }
    return ids;
}

int BindMacroSystem::GetMacroCount()
{
    return (int)s_macros.size();
}

// ============================================================================
// Recording
// ============================================================================

bool BindMacroSystem::StartRecording(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    if (s_recordingMacroId != -1)
        StopRecording();
    
    s_recordingMacroId = macroId;
    s_recordingStartTime = GetTickCount();
    
    BindMacro* macro = GetMacro(macroId);
    if (macro)
    {
        macro->actions.clear();
        macro->totalDuration = 0;
    }
    
    return true;
}

bool BindMacroSystem::StopRecording()
{
    if (s_recordingMacroId == -1)
        return false;
    
    BindMacro* macro = GetMacro(s_recordingMacroId);
    if (macro && !macro->actions.empty())
    {
        uint32_t totalTime = GetTickCount() - s_recordingStartTime;
        macro->totalDuration = totalTime;
    }
    
    s_recordingMacroId = -1;
    return true;
}

bool BindMacroSystem::IsRecording()
{
    return s_recordingMacroId != -1;
}

int BindMacroSystem::GetRecordingMacroId()
{
    return s_recordingMacroId;
}

uint32_t BindMacroSystem::GetRecordingDuration()
{
    if (s_recordingMacroId == -1)
        return 0;
    
    return GetTickCount() - s_recordingStartTime;
}

// ============================================================================
// Playback
// ============================================================================

bool BindMacroSystem::StartPlayback(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    if (s_playingMacroId != -1)
        StopPlayback();
    
    s_playingMacroId = macroId;
    s_playbackStartTime = GetTickCount();
    s_currentActionIndex = 0;
    
    return true;
}

bool BindMacroSystem::StopPlayback()
{
    s_playingMacroId = -1;
    s_currentActionIndex = 0;
    return true;
}

bool BindMacroSystem::IsPlaying()
{
    return s_playingMacroId != -1;
}

int BindMacroSystem::GetPlayingMacroId()
{
    return s_playingMacroId;
}

void BindMacroSystem::SetPlaybackSpeed(float speed)
{
    s_playbackSpeed = std::max(0.1f, std::min(5.0f, speed));
}

float BindMacroSystem::GetPlaybackSpeed()
{
    return s_playbackSpeed;
}

// ============================================================================
// Direct Binding Management
// ============================================================================

bool BindMacroSystem::AddTrigger(int macroId, BindTriggerType triggerType, uint16_t hid)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    BindMacro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    BindMacroTrigger trigger;
    trigger.type = triggerType;
    trigger.hid = hid;
    trigger.isActive = false;
    trigger.lastTriggerTime = 0;
    
    macro->triggers.push_back(trigger);
    macro->isDirectBinding = true;
    
    return true;
}

bool BindMacroSystem::RemoveTrigger(int macroId, BindTriggerType triggerType, uint16_t hid)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    BindMacro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    auto it = std::remove_if(macro->triggers.begin(), macro->triggers.end(),
        [triggerType, hid](const BindMacroTrigger& trigger) {
            return trigger.type == triggerType && trigger.hid == hid;
        });
    
    if (it != macro->triggers.end())
    {
        macro->triggers.erase(it, macro->triggers.end());
        if (macro->triggers.empty())
            macro->isDirectBinding = false;
        return true;
    }
    
    return false;
}

bool BindMacroSystem::SetTriggerCooldown(int macroId, BindTriggerType triggerType, uint32_t cooldownMs)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    BindMacro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    for (auto& trigger : macro->triggers)
    {
        if (trigger.type == triggerType)
        {
            trigger.cooldownMs = cooldownMs;
            return true;
        }
    }
    
    return false;
}

void BindMacroSystem::EnableDirectBinding(bool enable)
{
    s_directBindingEnabled = enable;
}

bool BindMacroSystem::IsDirectBindingEnabled()
{
    return s_directBindingEnabled;
}

// ============================================================================
// Conditional Macro Creation
// ============================================================================

bool BindMacroSystem::CreateConditionalMacro(int macroId, const std::wstring& condition)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    BindMacro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    // Parse condition string and create conditional actions
    // This is a simplified implementation - in reality you'd want a proper parser
    
    BindMacroAction conditionalStart;
    conditionalStart.type = BindMacroActionType::ConditionalStart;
    conditionalStart.timestamp = 0;
    
    BindMacroAction conditionalEnd;
    conditionalEnd.type = BindMacroActionType::ConditionalEnd;
    conditionalEnd.timestamp = 0;
    
    macro->actions.push_back(conditionalStart);
    // Add condition-specific actions here based on parsing
    macro->actions.push_back(conditionalEnd);
    
    return true;
}

bool BindMacroSystem::AddConditionalAction(int macroId, const BindMacroAction& action)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    BindMacro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    // Find the last ConditionalEnd and insert before it
    for (int i = (int)macro->actions.size() - 1; i >= 0; i--)
    {
        if (macro->actions[i].type == BindMacroActionType::ConditionalEnd)
        {
            macro->actions.insert(macro->actions.begin() + i, action);
            return true;
        }
    }
    
    // If no ConditionalEnd found, just append
    macro->actions.push_back(action);
    return true;
}

bool BindMacroSystem::CreateCompositeTrigger(int macroId, const std::vector<BindTriggerType>& triggers)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    BindMacro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    macro->triggers.clear();
    
    for (auto triggerType : triggers)
    {
        BindMacroTrigger trigger;
        trigger.type = triggerType;
        trigger.hid = 0;
        trigger.isActive = false;
        trigger.lastTriggerTime = 0;
        macro->triggers.push_back(trigger);
    }
    
    macro->isDirectBinding = true;
    return true;
}

// ============================================================================
// Real-time Processing
// ============================================================================

void BindMacroSystem::Tick()
{
    // Process direct bindings if enabled
    if (s_directBindingEnabled)
    {
        for (int i = 0; i < (int)s_macros.size(); i++)
        {
            if (s_macros[i].isDirectBinding)
            {
                CheckTriggers(i);
            }
        }
    }
    
    // Process playback
    if (s_playingMacroId != -1)
    {
        ExecuteNextAction();
    }
}

void BindMacroSystem::ProcessKeyboardInput(UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Update key state tracking
    if (wParam < 256)
    {
        bool isDown = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
        s_keyStates[wParam] = isDown;
        
        if (isDown)
            s_keyPressTimes[wParam] = GetTickCount();
    }
    
    // Process direct bindings
    if (s_directBindingEnabled)
    {
        ProcessDirectBindings(msg, wParam, lParam);
    }
    
    // Process recording
    if (s_recordingMacroId != -1)
    {
        ProcessRecordingInput(msg, wParam, lParam);
    }
}

void BindMacroSystem::ProcessMouseInput(UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Update mouse state tracking
    uint32_t currentTime = GetTickCount();
    
    switch (msg)
    {
    case WM_LBUTTONDOWN:
        s_mouseLeftPressed = true;
        s_mouseLeftPressTime = currentTime;
        break;
    case WM_LBUTTONUP:
        s_mouseLeftPressed = false;
        break;
    case WM_RBUTTONDOWN:
        s_mouseRightPressed = true;
        s_mouseRightPressTime = currentTime;
        break;
    case WM_RBUTTONUP:
        s_mouseRightPressed = false;
        break;
    case WM_MBUTTONDOWN:
        s_mouseMiddlePressed = true;
        s_mouseMiddlePressTime = currentTime;
        break;
    case WM_MBUTTONUP:
        s_mouseMiddlePressed = false;
        break;
    }
    
    // Process direct bindings
    if (s_directBindingEnabled)
    {
        ProcessDirectBindings(msg, wParam, lParam);
    }
    
    // Process recording
    if (s_recordingMacroId != -1)
    {
        ProcessRecordingInput(msg, wParam, lParam);
    }
}

// ============================================================================
// Direct Binding Processing
// ============================================================================

void BindMacroSystem::ProcessDirectBindings(UINT msg, WPARAM wParam, LPARAM lParam)
{
    for (int i = 0; i < (int)s_macros.size(); i++)
    {
        if (s_macros[i].isDirectBinding && ShouldTriggerMacro(i, msg, wParam, lParam))
        {
            TriggerMacro(i);
        }
    }
}

bool BindMacroSystem::ShouldTriggerMacro(int macroId, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    BindMacro* macro = GetMacro(macroId);
    if (!macro || macro->triggers.empty())
        return false;
    
    uint32_t currentTime = GetTickCount();
    
    // Check if any trigger should activate this macro
    for (const auto& trigger : macro->triggers)
    {
        if (currentTime - trigger.lastTriggerTime < trigger.cooldownMs)
            continue; // Still in cooldown
        
        bool shouldTrigger = false;
        
        switch (trigger.type)
        {
        case BindTriggerType::MouseLeftDown:
            shouldTrigger = (msg == WM_LBUTTONDOWN);
            break;
        case BindTriggerType::MouseLeftUp:
            shouldTrigger = (msg == WM_LBUTTONUP);
            break;
        case BindTriggerType::MouseRightDown:
            shouldTrigger = (msg == WM_RBUTTONDOWN);
            break;
        case BindTriggerType::MouseRightUp:
            shouldTrigger = (msg == WM_RBUTTONUP);
            break;
        case BindTriggerType::MouseLeftPressed:
            shouldTrigger = s_mouseLeftPressed;
            break;
        case BindTriggerType::MouseRightPressed:
            shouldTrigger = s_mouseRightPressed;
            break;
        case BindTriggerType::KeyPress:
            shouldTrigger = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && wParam == trigger.hid;
            break;
        case BindTriggerType::KeyRelease:
            shouldTrigger = (msg == WM_KEYUP || msg == WM_SYSKEYUP) && wParam == trigger.hid;
            break;
        case BindTriggerType::KeyPressed:
            shouldTrigger = (wParam < 256) && s_keyStates[wParam];
            break;
        }
        
        if (shouldTrigger)
        {
            // For composite triggers, check if all conditions are met
            if (macro->triggers.size() > 1)
            {
                if (macro->requiresAllConditions)
                {
                    // AND logic - all triggers must be active
                    bool allActive = true;
                    for (const auto& otherTrigger : macro->triggers)
                    {
                        if (!CheckTriggerActive(otherTrigger))
                        {
                            allActive = false;
                            break;
                        }
                    }
                    return allActive;
                }
                else
                {
                    // OR logic - any trigger can activate
                    return true;
                }
            }
            
            return true;
        }
    }
    
    return false;
}

bool BindMacroSystem::CheckTriggerActive(const BindMacroTrigger& trigger)
{
    switch (trigger.type)
    {
    case BindTriggerType::MouseLeftPressed:
        return s_mouseLeftPressed;
    case BindTriggerType::MouseRightPressed:
        return s_mouseRightPressed;
    case BindTriggerType::KeyPressed:
        return (trigger.hid < 256) && s_keyStates[trigger.hid];
    default:
        return false;
    }
}

void BindMacroSystem::TriggerMacro(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return;
    
    BindMacro* macro = GetMacro(macroId);
    if (!macro)
        return;
    
    // Update trigger times
    uint32_t currentTime = GetTickCount();
    for (auto& trigger : macro->triggers)
    {
        trigger.lastTriggerTime = currentTime;
    }
    
    // Start playback of this macro
    StartPlayback(macroId);
}

// ============================================================================
// Conditional Processing
// ============================================================================

bool BindMacroSystem::EvaluateCondition(const BindMacroAction& condition)
{
    switch (condition.type)
    {
    case BindMacroActionType::MouseLeftPressed:
        return s_mouseLeftPressed;
    case BindMacroActionType::MouseRightPressed:
        return s_mouseRightPressed;
    case BindMacroActionType::MouseButtonPressed:
        return CheckMouseCondition(condition.type);
    case BindMacroActionType::KeyIsPressed:
        return CheckKeyCondition(condition.type, condition.hid);
    default:
        return false;
    }
}

bool BindMacroSystem::CheckMouseCondition(BindMacroActionType condition)
{
    switch (condition)
    {
    case BindMacroActionType::MouseLeftPressed:
        return s_mouseLeftPressed;
    case BindMacroActionType::MouseRightPressed:
        return s_mouseRightPressed;
    default:
        return false;
    }
}

bool BindMacroSystem::CheckKeyCondition(BindMacroActionType condition, uint16_t hid)
{
    if (hid >= 256)
        return false;
    
    return s_keyStates[hid];
}

// ============================================================================
// Recording Implementation
// ============================================================================

void BindMacroSystem::ProcessRecordingInput(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (s_recordingMacroId == -1)
        return;
    
    BindMacro* macro = GetMacro(s_recordingMacroId);
    if (!macro)
        return;
    
    uint32_t currentTime = GetTickCount() - s_recordingStartTime;
    
    BindMacroAction action;
    action.timestamp = currentTime;
    
    // Process keyboard input
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
    {
        action.type = BindMacroActionType::KeyPress;
        action.hid = (uint16_t)wParam;
        macro->actions.push_back(action);
    }
    else if (msg == WM_KEYUP || msg == WM_SYSKEYUP)
    {
        action.type = BindMacroActionType::KeyRelease;
        action.hid = (uint16_t)wParam;
        macro->actions.push_back(action);
    }
    
    // Process mouse input
    switch (msg)
    {
    case WM_LBUTTONDOWN:
        action.type = BindMacroActionType::MouseLeftDown;
        macro->actions.push_back(action);
        break;
    case WM_LBUTTONUP:
        action.type = BindMacroActionType::MouseLeftUp;
        macro->actions.push_back(action);
        break;
    case WM_RBUTTONDOWN:
        action.type = BindMacroActionType::MouseRightDown;
        macro->actions.push_back(action);
        break;
    case WM_RBUTTONUP:
        action.type = BindMacroActionType::MouseRightUp;
        macro->actions.push_back(action);
        break;
    case WM_MBUTTONDOWN:
        action.type = BindMacroActionType::MouseMiddleDown;
        macro->actions.push_back(action);
        break;
    case WM_MBUTTONUP:
        action.type = BindMacroActionType::MouseMiddleUp;
        macro->actions.push_back(action);
        break;
    case WM_MOUSEWHEEL:
        action.type = (GET_WHEEL_DELTA_WPARAM(wParam) > 0) ? 
                      BindMacroActionType::MouseWheelUp : BindMacroActionType::MouseWheelDown;
        macro->actions.push_back(action);
        break;
    case WM_MOUSEMOVE:
        action.type = BindMacroActionType::MouseMove;
        action.mousePos.x = GET_X_LPARAM(lParam);
        action.mousePos.y = GET_Y_LPARAM(lParam);
        macro->actions.push_back(action);
        break;
    }
}

// ============================================================================
// Playback Implementation
// ============================================================================

void BindMacroSystem::ExecuteNextAction()
{
    if (s_playingMacroId == -1)
        return;
    
    BindMacro* macro = GetMacro(s_playingMacroId);
    if (!macro || s_currentActionIndex >= macro->actions.size())
    {
        // Macro finished
        if (macro->isLooping)
        {
            s_currentActionIndex = 0;
            s_playbackStartTime = GetTickCount();
        }
        else
        {
            StopPlayback();
        }
        return;
    }
    
    uint32_t currentTime = GetTickCount() - s_playbackStartTime;
    BindMacroAction& action = macro->actions[s_currentActionIndex];
    
    // Check if it's time to execute this action
    uint32_t actionTime = (uint32_t)(action.timestamp / s_playbackSpeed);
    if (currentTime >= actionTime)
    {
        // Handle conditional blocks
        if (action.type == BindMacroActionType::ConditionalStart)
        {
            // Evaluate condition and skip to ConditionalEnd if false
            if (!EvaluateCondition(action))
            {
                // Skip to matching ConditionalEnd
                int depth = 1;
                for (size_t i = s_currentActionIndex + 1; i < macro->actions.size(); i++)
                {
                    if (macro->actions[i].type == BindMacroActionType::ConditionalStart)
                        depth++;
                    else if (macro->actions[i].type == BindMacroActionType::ConditionalEnd)
                    {
                        depth--;
                        if (depth == 0)
                        {
                            s_currentActionIndex = i + 1;
                            return;
                        }
                    }
                }
            }
        }
        else if (action.type == BindMacroActionType::ConditionalEnd)
        {
            // Just skip conditional end
            s_currentActionIndex++;
            return;
        }
        else
        {
            // Execute regular action
            ExecuteAction(action);
        }
        
        s_currentActionIndex++;
    }
}

void BindMacroSystem::ExecuteAction(const BindMacroAction& action)
{
    switch (action.type)
    {
    case BindMacroActionType::KeyPress:
        SendKeyInput(action.hid, true);
        break;
    case BindMacroActionType::KeyRelease:
        SendKeyInput(action.hid, false);
        break;
    case BindMacroActionType::MouseMove:
        SendMouseInput(action);
        break;
    case BindMacroActionType::MouseLeftDown:
        SendMouseInput(action);
        break;
    case BindMacroActionType::MouseLeftUp:
        SendMouseInput(action);
        break;
    case BindMacroActionType::MouseRightDown:
        SendMouseInput(action);
        break;
    case BindMacroActionType::MouseRightUp:
        SendMouseInput(action);
        break;
    case BindMacroActionType::MouseMiddleDown:
        SendMouseInput(action);
        break;
    case BindMacroActionType::MouseMiddleUp:
        SendMouseInput(action);
        break;
    case BindMacroActionType::MouseWheelUp:
        SendMouseInput(action);
        break;
    case BindMacroActionType::MouseWheelDown:
        SendMouseInput(action);
        break;
    case BindMacroActionType::Delay:
        Sleep(action.delayMs);
        break;
    }
}

// ============================================================================
// Input Sending
// ============================================================================

void BindMacroSystem::SendKeyInput(uint16_t hid, bool down)
{
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    
    WORD vkCode = (WORD)hid;
    if (vkCode > 255) vkCode = 0;
    
    input.ki.wVk = vkCode;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    input.ki.time = 0;
    // Mark as synthetic so hooks ignore it
    input.ki.dwExtraInfo = (ULONG_PTR)0x484A4D43ULL;
    SendInput(1, &input, sizeof(INPUT));
}

void BindMacroSystem::SendMouseInput(const BindMacroAction& action)
{
    INPUT input = {};
    input.type = INPUT_MOUSE;
    
    switch (action.type)
    {
    case BindMacroActionType::MouseMove:
        // Skip macro mouse move to avoid interfering with user camera/mouse
        {
            char dbg[128];
            _snprintf_s(dbg, sizeof(dbg), _TRUNCATE, "HBM: SKIP_MOUSE_MOVE to=%d,%d\n", action.mousePos.x, action.mousePos.y);
            OutputDebugStringA(dbg);
        }
        return;
    case BindMacroActionType::MouseLeftDown:
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        break;
    case BindMacroActionType::MouseLeftUp:
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        break;
    case BindMacroActionType::MouseRightDown:
        input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
        break;
    case BindMacroActionType::MouseRightUp:
        input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        break;
    case BindMacroActionType::MouseMiddleDown:
        input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
        break;
    case BindMacroActionType::MouseMiddleUp:
        input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
        break;
    case BindMacroActionType::MouseWheelUp:
        input.mi.mouseData = WHEEL_DELTA;
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        break;
    case BindMacroActionType::MouseWheelDown:
        input.mi.mouseData = -WHEEL_DELTA;
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        break;
    }
    input.mi.dwExtraInfo = (ULONG_PTR)0x484A4D43ULL;
    SendInput(1, &input, sizeof(INPUT));
}

// ============================================================================
// Settings
// ============================================================================

void BindMacroSystem::SetBlockKeysDuringPlayback(bool block)
{
    s_blockKeysDuringPlayback = block;
}

bool BindMacroSystem::GetBlockKeysDuringPlayback()
{
    return s_blockKeysDuringPlayback;
}

void BindMacroSystem::SetLoopMacro(int macroId, bool loop)
{
    BindMacro* macro = GetMacro(macroId);
    if (macro)
        macro->isLooping = loop;
}

bool BindMacroSystem::GetLoopMacro(int macroId)
{
    BindMacro* macro = GetMacro(macroId);
    return macro ? macro->isLooping : false;
}

// ============================================================================
// Storage
// ============================================================================

bool BindMacroSystem::SaveToFile(const wchar_t* filePath)
{
    // Simplified implementation - in reality you'd want proper serialization
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open())
        return false;
    
    // Write macro count
    uint32_t count = (uint32_t)s_macros.size();
    file.write((const char*)&count, sizeof(count));
    
    // Write each macro
    for (const auto& macro : s_macros)
    {
        // Write macro name length and name
        uint32_t nameLen = (uint32_t)macro.name.length();
        file.write((const char*)&nameLen, sizeof(nameLen));
        file.write((const char*)macro.name.c_str(), nameLen * sizeof(wchar_t));
        
        // Write macro properties
        file.write((const char*)&macro.isLooping, sizeof(macro.isLooping));
        file.write((const char*)&macro.playbackSpeed, sizeof(macro.playbackSpeed));
        file.write((const char*)&macro.blockKeysDuringPlayback, sizeof(macro.blockKeysDuringPlayback));
        file.write((const char*)&macro.totalDuration, sizeof(macro.totalDuration));
        file.write((const char*)&macro.isDirectBinding, sizeof(macro.isDirectBinding));
        
        // Write actions count and actions
        uint32_t actionCount = (uint32_t)macro.actions.size();
        file.write((const char*)&actionCount, sizeof(actionCount));
        for (const auto& action : macro.actions)
        {
            file.write((const char*)&action, sizeof(action));
        }
        
        // Write triggers count and triggers
        uint32_t triggerCount = (uint32_t)macro.triggers.size();
        file.write((const char*)&triggerCount, sizeof(triggerCount));
        for (const auto& trigger : macro.triggers)
        {
            file.write((const char*)&trigger, sizeof(trigger));
        }
    }
    
    file.close();
    return true;
}

bool BindMacroSystem::LoadFromFile(const wchar_t* filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
        return false;
    
    // Clear existing macros
    s_macros.clear();
    
    // Read macro count
    uint32_t count;
    file.read((char*)&count, sizeof(count));
    
    // Read each macro
    for (uint32_t i = 0; i < count; i++)
    {
        BindMacro macro;
        
        // Read name
        uint32_t nameLen;
        file.read((char*)&nameLen, sizeof(nameLen));
        macro.name.resize(nameLen);
        file.read((char*)macro.name.c_str(), nameLen * sizeof(wchar_t));
        
        // Read macro properties
        file.read((char*)&macro.isLooping, sizeof(macro.isLooping));
        file.read((char*)&macro.playbackSpeed, sizeof(macro.playbackSpeed));
        file.read((char*)&macro.blockKeysDuringPlayback, sizeof(macro.blockKeysDuringPlayback));
        file.read((char*)&macro.totalDuration, sizeof(macro.totalDuration));
        file.read((char*)&macro.isDirectBinding, sizeof(macro.isDirectBinding));
        
        // Read actions
        uint32_t actionCount;
        file.read((char*)&actionCount, sizeof(actionCount));
        macro.actions.resize(actionCount);
        for (uint32_t j = 0; j < actionCount; j++)
        {
            file.read((char*)&macro.actions[j], sizeof(BindMacroAction));
        }
        
        // Read triggers
        uint32_t triggerCount;
        file.read((char*)&triggerCount, sizeof(triggerCount));
        macro.triggers.resize(triggerCount);
        for (uint32_t j = 0; j < triggerCount; j++)
        {
            file.read((char*)&macro.triggers[j], sizeof(BindMacroTrigger));
        }
        
        s_macros.push_back(macro);
    }
    
    file.close();
    return true;
}

// ============================================================================
// Initialization
// ============================================================================

void BindMacroSystem::Initialize()
{
    s_macros.clear();
    s_recordingMacroId = -1;
    s_playingMacroId = -1;
    s_currentActionIndex = 0;
    s_blockKeysDuringPlayback = true;
    s_playbackSpeed = 1.0f;
    s_directBindingEnabled = false;
    
    // Reset mouse state
    s_mouseLeftPressed = false;
    s_mouseRightPressed = false;
    s_mouseMiddlePressed = false;
    s_mouseLeftPressTime = 0;
    s_mouseRightPressTime = 0;
    s_mouseMiddlePressTime = 0;
    
    // Reset key state
    std::fill(s_keyStates.begin(), s_keyStates.end(), false);
    std::fill(s_keyPressTimes.begin(), s_keyPressTimes.end(), 0);
}

void BindMacroSystem::Shutdown()
{
    StopRecording();
    StopPlayback();
    s_macros.clear();
}
