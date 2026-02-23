#include "HallJoy_V21_Macro.h"
#include "win_util.h"
#include <string>
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <sstream>

// Static member definitions
std::vector<HallJoyV21Macro> HallJoy_V21_Macro::s_macros;
int HallJoy_V21_Macro::s_recordingMacroId = -1;
int HallJoy_V21_Macro::s_playingMacroId = -1;
uint32_t HallJoy_V21_Macro::s_recordingStartTime = 0;
uint32_t HallJoy_V21_Macro::s_playbackStartTime = 0;
size_t HallJoy_V21_Macro::s_currentActionIndex = 0;
bool HallJoy_V21_Macro::s_blockKeysDuringPlayback = true;
float HallJoy_V21_Macro::s_playbackSpeed = 1.0f;

// V.2.1 Runtime state
std::vector<std::vector<HallJoyV21MacroVariable>> HallJoy_V21_Macro::s_runtimeVariables;
std::vector<uint32_t> HallJoy_V21_Macro::s_loopCounters;
std::vector<size_t> HallJoy_V21_Macro::s_loopStartIndices;
std::vector<uint32_t> HallJoy_V21_Macro::s_executionTimes;
std::vector<uint32_t> HallJoy_V21_Macro::s_actionCounts;

// Input state for conditions
std::vector<bool> HallJoy_V21_Macro::s_keyStates(256, false);
bool HallJoy_V21_Macro::s_mouseLeftPressed = false;
bool HallJoy_V21_Macro::s_mouseRightPressed = false;
bool HallJoy_V21_Macro::s_mouseMiddlePressed = false;

// ============================================================================
// Macro Management
// ============================================================================

int HallJoy_V21_Macro::CreateMacro(const std::wstring& name, const std::wstring& description)
{
    HallJoyV21Macro newMacro(name);
    newMacro.description = description;
    newMacro.isDirectBinding = false;  // Always false for V.2.1 system
    s_macros.push_back(newMacro);
    
    // Initialize runtime variables
    s_runtimeVariables.emplace_back();
    s_loopCounters.push_back(0);
    s_loopStartIndices.push_back(0);
    s_executionTimes.push_back(0);
    s_actionCounts.push_back(0);
    
    return (int)s_macros.size() - 1;
}

bool HallJoy_V21_Macro::DeleteMacro(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    // Stop recording/playing if this macro is active
    if (s_recordingMacroId == macroId)
        StopRecording();
    if (s_playingMacroId == macroId)
        StopPlayback();
    
    // Remove macro
    s_macros.erase(s_macros.begin() + macroId);
    
    // Remove runtime data
    if (macroId < (int)s_runtimeVariables.size())
        s_runtimeVariables.erase(s_runtimeVariables.begin() + macroId);
    if (macroId < (int)s_loopCounters.size())
        s_loopCounters.erase(s_loopCounters.begin() + macroId);
    if (macroId < (int)s_loopStartIndices.size())
        s_loopStartIndices.erase(s_loopStartIndices.begin() + macroId);
    if (macroId < (int)s_executionTimes.size())
        s_executionTimes.erase(s_executionTimes.begin() + macroId);
    if (macroId < (int)s_actionCounts.size())
        s_actionCounts.erase(s_actionCounts.begin() + macroId);
    
    // Update active IDs
    if (s_recordingMacroId > macroId)
        s_recordingMacroId--;
    if (s_playingMacroId > macroId)
        s_playingMacroId--;
    
    return true;
}

HallJoyV21Macro* HallJoy_V21_Macro::GetMacro(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return nullptr;
    return &s_macros[macroId];
}

std::vector<int> HallJoy_V21_Macro::GetAllMacroIds()
{
    std::vector<int> ids;
    for (int i = 0; i < (int)s_macros.size(); ++i)
        ids.push_back(i);
    return ids;
}

int HallJoy_V21_Macro::GetMacroCount()
{
    return (int)s_macros.size();
}

// ============================================================================
// Quick Access Features
// ============================================================================

bool HallJoy_V21_Macro::SetMacroCategory(int macroId, const std::wstring& category)
{
    HallJoyV21Macro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    macro->category = category;
    return true;
}

bool HallJoy_V21_Macro::SetMacroHotkey(int macroId, const std::wstring& hotkey)
{
    HallJoyV21Macro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    macro->hotkey = hotkey;
    return true;
}

bool HallJoy_V21_Macro::ToggleFavorite(int macroId)
{
    HallJoyV21Macro* macro = GetMacro(macroId);
    if (!macro)
        return false;
    
    macro->isFavorite = !macro->isFavorite;
    return true;
}

std::vector<int> HallJoy_V21_Macro::GetMacrosByCategory(const std::wstring& category)
{
    std::vector<int> result;
    for (int i = 0; i < (int)s_macros.size(); ++i)
    {
        if (s_macros[i].category == category)
            result.push_back(i);
    }
    return result;
}

std::vector<int> HallJoy_V21_Macro::GetFavoriteMacros()
{
    std::vector<int> result;
    for (int i = 0; i < (int)s_macros.size(); ++i)
    {
        if (s_macros[i].isFavorite)
            result.push_back(i);
    }
    return result;
}

std::vector<int> HallJoy_V21_Macro::SearchMacros(const std::wstring& searchTerm)
{
    std::vector<int> result;
    std::wstring lowerSearch = searchTerm;
    std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), std::towlower);
    
    for (int i = 0; i < (int)s_macros.size(); ++i)
    {
        std::wstring lowerName = s_macros[i].name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), std::towlower);
        
        std::wstring lowerDesc = s_macros[i].description;
        std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(), std::towlower);
        
        if (lowerName.find(lowerSearch) != std::wstring::npos ||
            lowerDesc.find(lowerSearch) != std::wstring::npos)
        {
            result.push_back(i);
        }
    }
    return result;
}

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
