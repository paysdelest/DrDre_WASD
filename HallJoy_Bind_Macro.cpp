// halljoy_bind_macro.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <sstream>

#include "halljoy_bind_macro.h"
#include "win_util.h"
#include "mouse_combo_system.h"
#include "keyboard_ui.h"
#include "backend.h"

#include <atomic>

// Static member definitions
std::vector<HallJoyBindMacro> HallJoy_Bind_Macro::s_macros;
int HallJoy_Bind_Macro::s_recordingMacroId = -1;
int HallJoy_Bind_Macro::s_playingMacroId = -1;
uint32_t HallJoy_Bind_Macro::s_recordingStartTime = 0;
uint32_t HallJoy_Bind_Macro::s_playbackStartTime = 0;
size_t HallJoy_Bind_Macro::s_currentActionIndex = 0;
bool HallJoy_Bind_Macro::s_blockKeysDuringPlayback = true;
float HallJoy_Bind_Macro::s_playbackSpeed = 1.0f;
bool HallJoy_Bind_Macro::s_directBindingEnabled = true;
uint32_t HallJoy_Bind_Macro::s_globalCooldown = 100;

// Mouse state tracking for triggers
bool HallJoy_Bind_Macro::s_mouseLeftPressed = false;
bool HallJoy_Bind_Macro::s_mouseRightPressed = false;
bool HallJoy_Bind_Macro::s_mouseMiddlePressed = false;

// Key state tracking
std::vector<bool> HallJoy_Bind_Macro::s_keyStates(256, false);

// Ignore synthetic key messages for a short window after we call SendInput
std::atomic<unsigned long long> s_ignoreKeyEventsUntilMs{ 0 };

// Marker for our synthetic SendInput events (dwExtraInfo)
static const ULONG_PTR kSyntheticExtraInfo = 0x484A4D43ULL; // 'HJMC' arbitrary magic

// Statistics
std::vector<uint32_t> HallJoy_Bind_Macro::s_executionCounts;
std::vector<uint32_t> HallJoy_Bind_Macro::s_lastExecutionTimes;

// ============================================================================
// Macro Management
// ============================================================================

int HallJoy_Bind_Macro::CreateMacro(const std::wstring& name)
{
    HallJoyBindMacro newMacro(name);
    newMacro.isDirectBinding = true;  // Always true for this system
    s_macros.push_back(newMacro);
    
    // Initialize statistics
    s_executionCounts.push_back(0);
    s_lastExecutionTimes.push_back(0);
    
    return (int)s_macros.size() - 1;
}

bool HallJoy_Bind_Macro::DeleteMacro(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    // Stop recording/playing if this macro is active
    if (s_recordingMacroId == macroId)
        StopRecording();
    if (s_playingMacroId == macroId)
        StopPlayback();
    
    s_macros.erase(s_macros.begin() + macroId);
    
    // Update statistics
    if (macroId < (int)s_executionCounts.size())
        s_executionCounts.erase(s_executionCounts.begin() + macroId);
    if (macroId < (int)s_lastExecutionTimes.size())
        s_lastExecutionTimes.erase(s_lastExecutionTimes.begin() + macroId);
    
    // Update active IDs
    if (s_recordingMacroId > macroId)
        s_recordingMacroId--;
    if (s_playingMacroId > macroId)
        s_playingMacroId--;
    
    return true;
}

HallJoyBindMacro* HallJoy_Bind_Macro::GetMacro(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return nullptr;
    return &s_macros[macroId];
}

std::vector<int> HallJoy_Bind_Macro::GetAllMacroIds()
{
    std::vector<int> ids;
    for (int i = 0; i < (int)s_macros.size(); ++i)
        ids.push_back(i);
    return ids;
}

int HallJoy_Bind_Macro::GetMacroCount()
{
    return (int)s_macros.size();
}

// ============================================================================
// Recording
// ============================================================================

bool HallJoy_Bind_Macro::StartRecording(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    if (s_recordingMacroId != -1)
        StopRecording();
    
    s_recordingMacroId = macroId;
    s_recordingStartTime = GetTickCount();
    
    HallJoyBindMacro* macro = GetMacro(macroId);
    if (macro)
    {
        macro->actions.clear();
        macro->totalDuration = 0;
    }
    
    return true;
}

bool HallJoy_Bind_Macro::StopRecording()
{
    if (s_recordingMacroId == -1)
        return false;
    
    HallJoyBindMacro* macro = GetMacro(s_recordingMacroId);
    if (macro && !macro->actions.empty())
    {
        uint32_t totalTime = GetTickCount() - s_recordingStartTime;
        macro->totalDuration = totalTime;
    }
    
    s_recordingMacroId = -1;
    return true;
}

bool HallJoy_Bind_Macro::IsRecording()
{
    return s_recordingMacroId != -1;
}

int HallJoy_Bind_Macro::GetRecordingMacroId()
{
    return s_recordingMacroId;
}

uint32_t HallJoy_Bind_Macro::GetRecordingDuration()
{
    if (s_recordingMacroId == -1)
        return 0;
    
    return GetTickCount() - s_recordingStartTime;
}

// ============================================================================
// Playback
// ============================================================================

bool HallJoy_Bind_Macro::StartPlayback(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    HallJoyBindMacro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    if (s_playingMacroId != -1)
        StopPlayback();
    
    // Check conditions before starting
    if (!macro->conditions.empty() && !EvaluateConditions(macroId))
        return false;
    
    s_playingMacroId = macroId;
    s_playbackStartTime = GetTickCount();
    s_currentActionIndex = 0;
    
    // Update statistics
    if (macroId < (int)s_executionCounts.size())
    {
        s_executionCounts[macroId]++;
        s_lastExecutionTimes[macroId] = GetTickCount();
    }
    
    return true;
}

bool HallJoy_Bind_Macro::StopPlayback()
{
    s_playingMacroId = -1;
    s_currentActionIndex = 0;
    return true;
}

bool HallJoy_Bind_Macro::IsPlaying()
{
    return s_playingMacroId != -1;
}

int HallJoy_Bind_Macro::GetPlayingMacroId()
{
    return s_playingMacroId;
}

void HallJoy_Bind_Macro::SetPlaybackSpeed(float speed)
{
    s_playbackSpeed = std::max(0.1f, std::min(5.0f, speed));
}

float HallJoy_Bind_Macro::GetPlaybackSpeed()
{
    return s_playbackSpeed;
}

// ============================================================================
// Direct Binding Management
// ============================================================================

bool HallJoy_Bind_Macro::AddTrigger(int macroId, HallJoyBindTriggerType triggerType, uint16_t hid, uint32_t cooldownMs)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    HallJoyBindMacro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    HallJoyBindMacroTrigger trigger;
    trigger.type = triggerType;
    trigger.hid = hid;
    trigger.cooldownMs = cooldownMs;
    trigger.isEnabled = true;
    trigger.lastTriggerTime = 0;
    
    macro->triggers.push_back(trigger);
    return true;
}

bool HallJoy_Bind_Macro::RemoveTrigger(int macroId, int triggerIndex)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    HallJoyBindMacro* macro = GetMacro(macroId);
    if (!macro || triggerIndex < 0 || triggerIndex >= (int)macro->triggers.size())
        return false;
    
    macro->triggers.erase(macro->triggers.begin() + triggerIndex);
    return true;
}

bool HallJoy_Bind_Macro::SetCompositeTrigger(int macroId, bool isComposite, bool requireAll)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    HallJoyBindMacro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    macro->isComposite = isComposite;
    macro->requireAllTriggers = requireAll;
    return true;
}

bool HallJoy_Bind_Macro::EnableDirectBinding(bool enable)
{
    s_directBindingEnabled = enable;
    return true;
}

bool HallJoy_Bind_Macro::IsDirectBindingEnabled()
{
    return s_directBindingEnabled;
}

// ============================================================================
// Composite Triggers - "Si clic gauche + clic droit → alors..."
// ============================================================================

bool HallJoy_Bind_Macro::CreateCompositeTrigger(int macroId, const std::vector<HallJoyBindTriggerType>& triggers)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    HallJoyBindMacro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    // Clear existing triggers
    macro->triggers.clear();
    
    // Add all composite triggers
    for (auto triggerType : triggers)
    {
        HallJoyBindMacroTrigger trigger;
        trigger.type = triggerType;
        trigger.cooldownMs = s_globalCooldown;
        trigger.isEnabled = true;
        trigger.lastTriggerTime = 0;
        macro->triggers.push_back(trigger);
    }
    
    // Mark as composite
    macro->isComposite = true;
    return true;
}

bool HallJoy_Bind_Macro::CheckCompositeTrigger(int macroId)
{
    HallJoyBindMacro* macro = GetMacro(macroId);
    if (!macro || !macro->isComposite || macro->triggers.empty())
        return false;
    
    uint32_t currentTime = GetTickCount();
    
    // Check if any trigger is on cooldown
    for (const auto& trigger : macro->triggers)
    {
        if (!trigger.isEnabled)
            continue;
            
        if (currentTime - trigger.lastTriggerTime < trigger.cooldownMs)
            return false;
    }
    
    // Check all triggers
    bool allTriggersActive = true;
    bool anyTriggerActive = false;
    
    for (const auto& trigger : macro->triggers)
    {
        if (!trigger.isEnabled)
            continue;
            
        bool triggerActive = false;
        
        switch (trigger.type)
        {
        case HallJoyBindTriggerType::MouseLeftDown:
            triggerActive = CheckMouseTrigger(HallJoyBindTriggerType::MouseLeftPressed);
            break;
        case HallJoyBindTriggerType::MouseRightDown:
            triggerActive = CheckMouseTrigger(HallJoyBindTriggerType::MouseRightPressed);
            break;
        case HallJoyBindTriggerType::KeyPress:
            triggerActive = CheckKeyTrigger(HallJoyBindTriggerType::KeyPressed, trigger.hid);
            break;
        default:
            triggerActive = CheckTrigger(macroId);
            break;
        }
        
        if (triggerActive)
            anyTriggerActive = true;
        else
            allTriggersActive = false;
    }
    
    // Return based on logic
    bool shouldTrigger = macro->requireAllTriggers ? allTriggersActive : anyTriggerActive;
    
    if (shouldTrigger)
    {
        // Update last trigger time for all triggers
        for (auto& trigger : macro->triggers)
        {
            const_cast<uint32_t&>(trigger.lastTriggerTime) = currentTime;
        }
    }
    
    return shouldTrigger;
}

// ============================================================================
// Conditions - "Conditions multiples comme l'utilisateur le décide"
// ============================================================================

bool HallJoy_Bind_Macro::AddCondition(int macroId, const HallJoyBindMacroCondition& condition)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    HallJoyBindMacro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    macro->conditions.push_back(condition);
    return true;
}

bool HallJoy_Bind_Macro::RemoveCondition(int macroId, int conditionIndex)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    HallJoyBindMacro* macro = GetMacro(macroId);
    if (!macro || conditionIndex < 0 || conditionIndex >= (int)macro->conditions.size())
        return false;
    
    macro->conditions.erase(macro->conditions.begin() + conditionIndex);
    return true;
}

bool HallJoy_Bind_Macro::SetConditionLogic(int macroId, bool requireAll)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    HallJoyBindMacro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    // Update all conditions to use the same logic
    for (auto& condition : macro->conditions)
    {
        condition.requireAll = requireAll;
    }
    
    return true;
}

bool HallJoy_Bind_Macro::EvaluateConditions(int macroId)
{
    HallJoyBindMacro* macro = GetMacro(macroId);
    if (!macro || macro->conditions.empty())
        return true;
    
    bool result = true;
    
    for (const auto& condition : macro->conditions)
    {
        bool conditionResult = EvaluateCondition(condition);
        
        if (condition.isNegated)
            conditionResult = !conditionResult;
        
        if (condition.requireAll)
        {
            // AND logic - all conditions must be true
            result = result && conditionResult;
            if (!result) break; // Early exit
        }
        else
        {
            // OR logic - any condition can be true
            if (conditionResult)
            {
                result = true;
                break; // Early exit on success
            }
            result = false;
        }
    }
    
    return result;
}

// ============================================================================
// Real-time Processing
// ============================================================================

void HallJoy_Bind_Macro::Tick()
{
    // Update key and mouse states for trigger checking
    // This would typically be called from a hook or main loop
    
    // Process playback
    if (s_playingMacroId != -1)
    {
        ExecuteNextAction();
    }
    
    // Check all triggers for direct binding
    if (s_directBindingEnabled && s_playingMacroId == -1)
    {
        CheckAllTriggers();
    }
}

void HallJoy_Bind_Macro::ProcessKeyboardInput(UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Prevent re-trigger loops: ignore keyboard messages that arrive immediately
    // after we injected synthetic input via SendInput.
    unsigned long long now = GetTickCount64();
    unsigned long long ignoreUntil = s_ignoreKeyEventsUntilMs.load(std::memory_order_acquire);
    if (now < ignoreUntil)
    {
        // Debug: skipped synthetic keyboard event
        OutputDebugStringA("HBM: Skipping keyboard msg (synthetic window)\n");
        return; // skip processing synthetic events
    }

    // Update key state tracking
    if (wParam < 256)
    {
        bool isDown = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
        s_keyStates[wParam] = isDown;
        // Debug: log physical key HID/state
        char debugMsg[128];
        _snprintf_s(debugMsg, sizeof(debugMsg), _TRUNCATE, "HBM: PHYS_KEY HID=0x%02X %s\n", (unsigned int)wParam, isDown ? "DOWN" : "UP");
        OutputDebugStringA(debugMsg);
    }
    
    // Process recording
    if (s_recordingMacroId != -1)
    {
        ProcessRecordingInput(msg, wParam, lParam);
    }
}

void HallJoy_Bind_Macro::ProcessMouseInput(UINT msg, WPARAM wParam, LPARAM lParam)
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
// Trigger Checking
// ============================================================================

void HallJoy_Bind_Macro::CheckAllTriggers()
{
    uint32_t currentTime = GetTickCount();
    
    for (int macroId = 0; macroId < (int)s_macros.size(); macroId++)
    {
        HallJoyBindMacro* macro = GetMacro(macroId);
        if (!macro || !macro->isDirectBinding || macro->triggers.empty())
            continue;
        
        // Check if macro is already playing
        if (s_playingMacroId == macroId)
            continue;
        
        // Check global cooldown
        if (macroId < (int)s_lastExecutionTimes.size())
        {
            if (currentTime - s_lastExecutionTimes[macroId] < s_globalCooldown)
                continue;
        }
        
        // Check triggers
        if (macro->isComposite)
        {
            if (CheckCompositeTrigger(macroId))
            {
                // Check conditions before triggering
                if (EvaluateConditions(macroId))
                {
                    StartPlayback(macroId);
                }
            }
        }
        else
        {
            // Simple trigger checking
            for (auto& trigger : macro->triggers)
            {
                if (!trigger.isEnabled)
                    continue;
                
                if (currentTime - trigger.lastTriggerTime < trigger.cooldownMs)
                    continue;
                
                bool triggered = false;
                
                switch (trigger.type)
                {
                case HallJoyBindTriggerType::MouseLeftDown:
                    triggered = CheckMouseTrigger(HallJoyBindTriggerType::MouseLeftPressed);
                    break;
                case HallJoyBindTriggerType::MouseRightDown:
                    triggered = CheckMouseTrigger(HallJoyBindTriggerType::MouseRightPressed);
                    break;
                case HallJoyBindTriggerType::KeyPress:
                    triggered = CheckKeyTrigger(HallJoyBindTriggerType::KeyPressed, trigger.hid);
                    break;
                default:
                    triggered = CheckTrigger(macroId);
                    break;
                }
                
                if (triggered)
                {
                    trigger.lastTriggerTime = currentTime;
                    
                    // Check conditions before triggering
                    if (EvaluateConditions(macroId))
                    {
                        StartPlayback(macroId);
                        break;
                    }
                }
            }
        }
    }
}

bool HallJoy_Bind_Macro::CheckTrigger(int macroId)
{
    HallJoyBindMacro* macro = GetMacro(macroId);
    if (!macro || macro->triggers.empty())
        return false;
    
    uint32_t currentTime = GetTickCount();
    
    for (const auto& trigger : macro->triggers)
    {
        if (!trigger.isEnabled)
            continue;
        
        if (currentTime - trigger.lastTriggerTime < trigger.cooldownMs)
            continue;
        
        switch (trigger.type)
        {
        case HallJoyBindTriggerType::MouseLeftDown:
            if (CheckMouseTrigger(HallJoyBindTriggerType::MouseLeftPressed))
                return true;
            break;
        case HallJoyBindTriggerType::MouseRightDown:
            if (CheckMouseTrigger(HallJoyBindTriggerType::MouseRightPressed))
                return true;
            break;
        case HallJoyBindTriggerType::KeyPress:
            if (CheckKeyTrigger(HallJoyBindTriggerType::KeyPressed, trigger.hid))
                return true;
            break;
        default:
            break;
        }
    }
    
    return false;
}

bool HallJoy_Bind_Macro::CheckMouseTrigger(HallJoyBindTriggerType triggerType)
{
    switch (triggerType)
    {
    case HallJoyBindTriggerType::MouseLeftPressed:
        return s_mouseLeftPressed;
    case HallJoyBindTriggerType::MouseRightPressed:
        return s_mouseRightPressed;
    case HallJoyBindTriggerType::MouseMiddlePressed:
        return s_mouseMiddlePressed;
    default:
        return false;
    }
}

bool HallJoy_Bind_Macro::CheckKeyTrigger(HallJoyBindTriggerType triggerType, uint16_t hid)
{
    if (hid >= 256)
        return false;
    
    switch (triggerType)
    {
    case HallJoyBindTriggerType::KeyPressed:
        return s_keyStates[hid];
    case HallJoyBindTriggerType::KeyReleased:
        return !s_keyStates[hid];
    default:
        return false;
    }
}

// ============================================================================
// Settings
// ============================================================================

void HallJoy_Bind_Macro::SetBlockKeysDuringPlayback(bool block)
{
    s_blockKeysDuringPlayback = block;
}

bool HallJoy_Bind_Macro::GetBlockKeysDuringPlayback()
{
    return s_blockKeysDuringPlayback;
}

void HallJoy_Bind_Macro::SetLoopMacro(int macroId, bool loop)
{
    HallJoyBindMacro* macro = GetMacro(macroId);
    if (macro)
        macro->isLooping = loop;
}

bool HallJoy_Bind_Macro::GetLoopMacro(int macroId)
{
    HallJoyBindMacro* macro = GetMacro(macroId);
    return macro ? macro->isLooping : false;
}

void HallJoy_Bind_Macro::SetGlobalCooldown(uint32_t cooldownMs)
{
    s_globalCooldown = cooldownMs;
}

uint32_t HallJoy_Bind_Macro::GetGlobalCooldown()
{
    return s_globalCooldown;
}

// ============================================================================
// Statistics
// ============================================================================

uint32_t HallJoy_Bind_Macro::GetMacroExecutionCount(int macroId)
{
    if (macroId >= 0 && macroId < (int)s_executionCounts.size())
        return s_executionCounts[macroId];
    return 0;
}

uint32_t HallJoy_Bind_Macro::GetMacroLastExecutionTime(int macroId)
{
    if (macroId >= 0 && macroId < (int)s_lastExecutionTimes.size())
        return s_lastExecutionTimes[macroId];
    return 0;
}

void HallJoy_Bind_Macro::ResetMacroStatistics(int macroId)
{
    if (macroId >= 0 && macroId < (int)s_executionCounts.size())
    {
        s_executionCounts[macroId] = 0;
        s_lastExecutionTimes[macroId] = 0;
    }
}

// ============================================================================
// Storage
// ============================================================================

bool HallJoy_Bind_Macro::SaveToFile(const wchar_t* filePath)
{
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open())
        return false;
    
    uint32_t count = (uint32_t)s_macros.size();
    file.write((const char*)&count, sizeof(count));
    
    for (const auto& macro : s_macros)
    {
        uint32_t nameLen = (uint32_t)macro.name.length();
        file.write((const char*)&nameLen, sizeof(nameLen));
        file.write((const char*)macro.name.c_str(), nameLen * sizeof(wchar_t));
        
        file.write((const char*)&macro.isLooping, sizeof(macro.isLooping));
        file.write((const char*)&macro.playbackSpeed, sizeof(macro.playbackSpeed));
        file.write((const char*)&macro.blockKeysDuringPlayback, sizeof(macro.blockKeysDuringPlayback));
        file.write((const char*)&macro.totalDuration, sizeof(macro.totalDuration));
        file.write((const char*)&macro.isDirectBinding, sizeof(macro.isDirectBinding));
        file.write((const char*)&macro.isComposite, sizeof(macro.isComposite));
        file.write((const char*)&macro.requireAllTriggers, sizeof(macro.requireAllTriggers));
        
        uint32_t actionCount = (uint32_t)macro.actions.size();
        file.write((const char*)&actionCount, sizeof(actionCount));
        for (const auto& action : macro.actions)
        {
            file.write((const char*)&action, sizeof(action));
        }
        
        uint32_t triggerCount = (uint32_t)macro.triggers.size();
        file.write((const char*)&triggerCount, sizeof(triggerCount));
        for (const auto& trigger : macro.triggers)
        {
            file.write((const char*)&trigger, sizeof(trigger));
        }
        
        uint32_t conditionCount = (uint32_t)macro.conditions.size();
        file.write((const char*)&conditionCount, sizeof(conditionCount));
        for (const auto& condition : macro.conditions)
        {
            file.write((const char*)&condition, sizeof(condition));
        }
    }
    
    file.close();
    return true;
}

bool HallJoy_Bind_Macro::LoadFromFile(const wchar_t* filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
        return false;
    
    s_macros.clear();
    s_executionCounts.clear();
    s_lastExecutionTimes.clear();
    
    uint32_t count;
    file.read((char*)&count, sizeof(count));
    
    for (uint32_t i = 0; i < count; i++)
    {
        HallJoyBindMacro macro;
        
        uint32_t nameLen;
        file.read((char*)&nameLen, sizeof(nameLen));
        macro.name.resize(nameLen);
        file.read((char*)macro.name.c_str(), nameLen * sizeof(wchar_t));
        
        file.read((char*)&macro.isLooping, sizeof(macro.isLooping));
        file.read((char*)&macro.playbackSpeed, sizeof(macro.playbackSpeed));
        file.read((char*)&macro.blockKeysDuringPlayback, sizeof(macro.blockKeysDuringPlayback));
        file.read((char*)&macro.totalDuration, sizeof(macro.totalDuration));
        file.read((char*)&macro.isDirectBinding, sizeof(macro.isDirectBinding));
        file.read((char*)&macro.isComposite, sizeof(macro.isComposite));
        file.read((char*)&macro.requireAllTriggers, sizeof(macro.requireAllTriggers));
        
        uint32_t actionCount;
        file.read((char*)&actionCount, sizeof(actionCount));
        macro.actions.resize(actionCount);
        for (uint32_t j = 0; j < actionCount; j++)
        {
            file.read((char*)&macro.actions[j], sizeof(HallJoyBindMacroAction));
        }
        
        uint32_t triggerCount;
        file.read((char*)&triggerCount, sizeof(triggerCount));
        macro.triggers.resize(triggerCount);
        for (uint32_t j = 0; j < triggerCount; j++)
        {
            file.read((char*)&macro.triggers[j], sizeof(HallJoyBindMacroTrigger));
        }
        
        uint32_t conditionCount;
        file.read((char*)&conditionCount, sizeof(conditionCount));
        macro.conditions.resize(conditionCount);
        for (uint32_t j = 0; j < conditionCount; j++)
        {
            file.read((char*)&macro.conditions[j], sizeof(HallJoyBindMacroCondition));
        }
        
        s_macros.push_back(macro);
        s_executionCounts.push_back(0);
        s_lastExecutionTimes.push_back(0);
    }
    
    file.close();
    return true;
}

// ============================================================================
// Initialization
// ============================================================================

void HallJoy_Bind_Macro::Initialize()
{
    s_macros.clear();
    s_recordingMacroId = -1;
    s_playingMacroId = -1;
    s_currentActionIndex = 0;
    s_blockKeysDuringPlayback = true;
    s_playbackSpeed = 1.0f;
    s_directBindingEnabled = true;
    s_globalCooldown = 100;
    
    // Reset mouse state
    s_mouseLeftPressed = false;
    s_mouseRightPressed = false;
    s_mouseMiddlePressed = false;
    
    // Reset key state
    std::fill(s_keyStates.begin(), s_keyStates.end(), false);
    
    // Clear statistics
    s_executionCounts.clear();
    s_lastExecutionTimes.clear();
}

void HallJoy_Bind_Macro::Shutdown()
{
    StopRecording();
    StopPlayback();
    s_macros.clear();
    s_executionCounts.clear();
    s_lastExecutionTimes.clear();
}

// ============================================================================
// Private Helper Functions
// ============================================================================

void HallJoy_Bind_Macro::ProcessRecordingInput(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (s_recordingMacroId == -1)
        return;
    
    HallJoyBindMacro* macro = GetMacro(s_recordingMacroId);
    if (!macro)
        return;
    
    uint32_t currentTime = GetTickCount() - s_recordingStartTime;
    
    HallJoyBindMacroAction action;
    action.timestamp = currentTime;
    
    // Process keyboard input
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
    {
        action.type = HallJoyBindMacroActionType::KeyPress;
        action.hid = (uint16_t)wParam;
        macro->actions.push_back(action);
    }
    else if (msg == WM_KEYUP || msg == WM_SYSKEYUP)
    {
        action.type = HallJoyBindMacroActionType::KeyRelease;
        action.hid = (uint16_t)wParam;
        macro->actions.push_back(action);
    }
    
    // Process mouse input
    switch (msg)
    {
    case WM_LBUTTONDOWN:
        action.type = HallJoyBindMacroActionType::MouseLeftDown;
        macro->actions.push_back(action);
        break;
    case WM_LBUTTONUP:
        action.type = HallJoyBindMacroActionType::MouseLeftUp;
        macro->actions.push_back(action);
        break;
    case WM_RBUTTONDOWN:
        action.type = HallJoyBindMacroActionType::MouseRightDown;
        macro->actions.push_back(action);
        break;
    case WM_RBUTTONUP:
        action.type = HallJoyBindMacroActionType::MouseRightUp;
        macro->actions.push_back(action);
        break;
    case WM_MBUTTONDOWN:
        action.type = HallJoyBindMacroActionType::MouseMiddleDown;
        macro->actions.push_back(action);
        break;
    case WM_MBUTTONUP:
        action.type = HallJoyBindMacroActionType::MouseMiddleUp;
        macro->actions.push_back(action);
        break;
    case WM_MOUSEWHEEL:
        action.type = (GET_WHEEL_DELTA_WPARAM(wParam) > 0) ? 
                      HallJoyBindMacroActionType::MouseWheelUp : HallJoyBindMacroActionType::MouseWheelDown;
        macro->actions.push_back(action);
        break;
    case WM_MOUSEMOVE:
        action.type = HallJoyBindMacroActionType::MouseMove;
        action.mousePos.x = GET_X_LPARAM(lParam);
        action.mousePos.y = GET_Y_LPARAM(lParam);
        macro->actions.push_back(action);
        break;
    }
}

void HallJoy_Bind_Macro::ExecuteNextAction()
{
    if (s_playingMacroId == -1)
        return;
    
    HallJoyBindMacro* macro = GetMacro(s_playingMacroId);
    if (!macro || s_currentActionIndex >= macro->actions.size())
    {
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
    HallJoyBindMacroAction& action = macro->actions[s_currentActionIndex];
    
    uint32_t actionTime = (uint32_t)(action.timestamp / s_playbackSpeed);
    if (currentTime >= actionTime)
    {
        ExecuteAction(action);
        s_currentActionIndex++;
    }
}

void HallJoy_Bind_Macro::ExecuteAction(const HallJoyBindMacroAction& action)
{
    switch (action.type)
    {
    case HallJoyBindMacroActionType::KeyPress:
        SendKeyInput(action.hid, true);
        break;
    case HallJoyBindMacroActionType::KeyRelease:
        SendKeyInput(action.hid, false);
        break;
    case HallJoyBindMacroActionType::MouseMove:
        SendMouseInput(action);
        break;
    case HallJoyBindMacroActionType::MouseLeftDown:
        SendMouseInput(action);
        break;
    case HallJoyBindMacroActionType::MouseLeftUp:
        SendMouseInput(action);
        break;
    case HallJoyBindMacroActionType::MouseRightDown:
        SendMouseInput(action);
        break;
    case HallJoyBindMacroActionType::MouseRightUp:
        SendMouseInput(action);
        break;
    case HallJoyBindMacroActionType::MouseMiddleDown:
        SendMouseInput(action);
        break;
    case HallJoyBindMacroActionType::MouseMiddleUp:
        SendMouseInput(action);
        break;
    case HallJoyBindMacroActionType::MouseWheelUp:
        SendMouseInput(action);
        break;
    case HallJoyBindMacroActionType::MouseWheelDown:
        SendMouseInput(action);
        break;
    case HallJoyBindMacroActionType::Delay:
        Sleep(action.delayMs);
        break;
    }
}

void HallJoy_Bind_Macro::SendKeyInput(uint16_t hid, bool down)
{
    INPUT input = {};
    input.type = INPUT_KEYBOARD;

    // Determine actual HID to inject analog value into UI/backend.
    uint16_t actualHid = 0;
    // Try interpreting 'hid' as a VK and convert to HID
    WORD vk = (WORD)hid;
    uint16_t candidate = VkToHid(vk);
    // Debug: log mapping attempt
    {
        char dbg[192];
        _snprintf_s(dbg, sizeof(dbg), _TRUNCATE, "HBM: SendKeyInput request hid=0x%02X vk=0x%02X candidateHid=0x%02X\n", (unsigned int)hid, (unsigned int)vk, (unsigned int)candidate);
        OutputDebugStringA(dbg);
    }
    if (candidate != 0 && candidate < 256 && KeyboardUI_HasHid(candidate))
    {
        actualHid = candidate;
    }
    else if (hid < 256 && KeyboardUI_HasHid(hid))
    {
        actualHid = hid;
    }
    else if (candidate != 0)
    {
        actualHid = candidate;
    }
    else
    {
        actualHid = hid;
    }

    // Inject analog via macro cache only; backend tick will update UI snapshot.
    if (down)
    {
        Backend_SetMacroAnalog(actualHid, 1.0f);
        // Debug
        char dbg2[128];
        _snprintf_s(dbg2, sizeof(dbg2), _TRUNCATE, "HBM: MACRO_INJECT SET HID=0x%02X\n", (unsigned int)actualHid);
        OutputDebugStringA(dbg2);
    }
    else
    {
        Backend_ClearMacroAnalog(actualHid);
        // Debug
        char dbg3[128];
        _snprintf_s(dbg3, sizeof(dbg3), _TRUNCATE, "HBM: MACRO_INJECT CLEAR HID=0x%02X\n", (unsigned int)actualHid);
        OutputDebugStringA(dbg3);
    }

    // Also send mechanical VK event for compatibility
    WORD vkCode = (WORD)hid;
    if (vkCode > 255) vkCode = 0;

    input.ki.wVk = vkCode;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    input.ki.time = 0;
    input.ki.dwExtraInfo = kSyntheticExtraInfo;

    // Mark a short window where incoming keyboard messages should be ignored
    // to avoid our synthetic SendInput from retriggering macro logic.
    unsigned long long now = GetTickCount64();
    s_ignoreKeyEventsUntilMs.store(now + 100ULL, std::memory_order_release);

    // Debug: about to SendInput
    {
        char dbg4[192];
        _snprintf_s(dbg4, sizeof(dbg4), _TRUNCATE, "HBM: SendInput vk=0x%02X %s (actualHid=0x%02X)\n", (unsigned int)vkCode, (down ? "DOWN" : "UP"), (unsigned int)actualHid);
        OutputDebugStringA(dbg4);
    }

    SendInput(1, &input, sizeof(INPUT));
}

void HallJoy_Bind_Macro::SendMouseInput(const HallJoyBindMacroAction& action)
{
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwExtraInfo = kSyntheticExtraInfo;
    
    // Simple rate limiter: avoid flooding OS with rapid mouse events
    static std::atomic<uint32_t> s_lastMouseSendMs{0};
    uint32_t nowMs = (uint32_t)GetTickCount();
    uint32_t lastMs = s_lastMouseSendMs.load(std::memory_order_relaxed);
    if (nowMs < lastMs + 2) // at most ~500 events/sec
    {
        Sleep(1);
    }
    s_lastMouseSendMs.store((uint32_t)GetTickCount(), std::memory_order_relaxed);

    // Skip sending raw mouse movements recorded in macros — they interfere with in-game camera.
    if (action.type == HallJoyBindMacroActionType::MouseMove)
    {
        char dbg[192];
        _snprintf_s(dbg, sizeof(dbg), _TRUNCATE, "HBM: SKIP_MOUSE_MOVE x=%d y=%d\n", action.mousePos.x, action.mousePos.y);
        OutputDebugStringA(dbg);
        return;
    }
    switch (action.type)
    {
    case HallJoyBindMacroActionType::MouseMove:
        input.mi.dx = action.mousePos.x;
        input.mi.dy = action.mousePos.y;
        input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
        break;
    case HallJoyBindMacroActionType::MouseLeftDown:
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        break;
    case HallJoyBindMacroActionType::MouseLeftUp:
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        break;
    case HallJoyBindMacroActionType::MouseRightDown:
        input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
        break;
    case HallJoyBindMacroActionType::MouseRightUp:
        input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        break;
    case HallJoyBindMacroActionType::MouseMiddleDown:
        input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
        break;
    case HallJoyBindMacroActionType::MouseMiddleUp:
        input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
        break;
    case HallJoyBindMacroActionType::MouseWheelUp:
        input.mi.mouseData = WHEEL_DELTA;
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        break;
    case HallJoyBindMacroActionType::MouseWheelDown:
        input.mi.mouseData = -WHEEL_DELTA;
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        break;
    }
    
    SendInput(1, &input, sizeof(INPUT));
}

bool HallJoy_Bind_Macro::EvaluateCondition(const HallJoyBindMacroCondition& condition)
{
    switch (condition.triggerType)
    {
    case HallJoyBindTriggerType::MouseLeftPressed:
        return s_mouseLeftPressed;
    case HallJoyBindTriggerType::MouseRightPressed:
        return s_mouseRightPressed;
    case HallJoyBindTriggerType::KeyPressed:
        return (condition.hid < 256) && s_keyStates[condition.hid];
    case HallJoyBindTriggerType::KeyReleased:
        return (condition.hid < 256) && !s_keyStates[condition.hid];
    default:
        return false;
    }
}
