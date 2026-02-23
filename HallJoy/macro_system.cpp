// macro_system.cpp
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

#include "macro_system.h"
#include "win_util.h"

// Static member definitions
std::vector<Macro> MacroSystem::s_macros;
int MacroSystem::s_recordingMacroId = -1;
int MacroSystem::s_playingMacroId = -1;
uint32_t MacroSystem::s_recordingStartTime = 0;
uint32_t MacroSystem::s_playbackStartTime = 0;
size_t MacroSystem::s_currentActionIndex = 0;
bool MacroSystem::s_blockKeysDuringPlayback = true;
float MacroSystem::s_playbackSpeed = 1.0f;

// ============================================================================
// Macro Management
// ============================================================================

int MacroSystem::CreateMacro(const std::wstring& name)
{
    Macro newMacro(name);
    s_macros.push_back(newMacro);
    return (int)s_macros.size() - 1;
}

bool MacroSystem::DeleteMacro(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    // Stop recording/playing if this macro is active
    if (s_recordingMacroId == macroId)
        StopRecording();
    if (s_playingMacroId == macroId)
        StopPlayback();
    
    s_macros.erase(s_macros.begin() + macroId);
    
    // Update active IDs
    if (s_recordingMacroId > macroId)
        s_recordingMacroId--;
    if (s_playingMacroId > macroId)
        s_playingMacroId--;
    
    return true;
}

Macro* MacroSystem::GetMacro(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return nullptr;
    return &s_macros[macroId];
}

std::vector<int> MacroSystem::GetAllMacroIds()
{
    std::vector<int> ids;
    for (int i = 0; i < (int)s_macros.size(); ++i)
        ids.push_back(i);
    return ids;
}

int MacroSystem::GetMacroCount()
{
    return (int)s_macros.size();
}

// ============================================================================
// Recording
// ============================================================================

bool MacroSystem::StartRecording(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    if (s_recordingMacroId != -1)
        StopRecording();
    
    s_recordingMacroId = macroId;
    s_recordingStartTime = GetTickCount();
    
    Macro* macro = GetMacro(macroId);
    if (macro)
    {
        macro->actions.clear();
        macro->totalDuration = 0;
    }
    
    return true;
}

bool MacroSystem::StopRecording()
{
    if (s_recordingMacroId == -1)
        return false;
    
    Macro* macro = GetMacro(s_recordingMacroId);
    if (macro && !macro->actions.empty())
    {
        // Calculate total duration
        uint32_t lastTime = macro->actions.back().timestamp;
        macro->totalDuration = lastTime;
    }
    
    s_recordingMacroId = -1;
    return true;
}

bool MacroSystem::IsRecording()
{
    return s_recordingMacroId != -1;
}

int MacroSystem::GetRecordingMacroId()
{
    return s_recordingMacroId;
}

uint32_t MacroSystem::GetRecordingDuration()
{
    if (s_recordingMacroId == -1)
        return 0;
    return GetTickCount() - s_recordingStartTime;
}

// ============================================================================
// Playback
// ============================================================================

bool MacroSystem::StartPlayback(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_macros.size())
        return false;
    
    if (s_playingMacroId != -1)
        StopPlayback();
    
    Macro* macro = GetMacro(macroId);
    if (!macro || macro->actions.empty())
        return false;
    
    s_playingMacroId = macroId;
    s_playbackStartTime = GetTickCount();
    s_currentActionIndex = 0;
    
    return true;
}

bool MacroSystem::StopPlayback()
{
    s_playingMacroId = -1;
    s_currentActionIndex = 0;
    return true;
}

bool MacroSystem::IsPlaying()
{
    return s_playingMacroId != -1;
}

int MacroSystem::GetPlayingMacroId()
{
    return s_playingMacroId;
}

void MacroSystem::SetPlaybackSpeed(float speed)
{
    s_playbackSpeed = std::clamp(speed, 0.1f, 5.0f);
}

float MacroSystem::GetPlaybackSpeed()
{
    return s_playbackSpeed;
}

// ============================================================================
// Real-time Processing
// ============================================================================

void MacroSystem::Tick()
{
    // Process recording input
    if (s_recordingMacroId != -1)
    {
        // Input processing will be handled by the hook in app.cpp
        // This tick can be used for UI updates
    }
    
    // Process playback
    if (s_playingMacroId != -1)
    {
        ExecuteNextAction();
    }
}

void MacroSystem::ProcessKeyboardInput(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (s_recordingMacroId == -1)
        return;
    
    Macro* macro = GetMacro(s_recordingMacroId);
    if (!macro)
        return;
    
    uint32_t currentTime = GetTickCount() - s_recordingStartTime;
    
    // Convert to HID using the same function as the rest of the app
    KBDLLHOOKSTRUCT* kbd = (KBDLLHOOKSTRUCT*)lParam;
    const bool ext = (kbd->flags & LLKHF_EXTENDED) != 0;
    uint16_t hid = HidFromKeyboardScanCode(kbd->scanCode, ext, kbd->vkCode);
    
    if (hid == 0) return; // Skip invalid keys
    
    MacroAction action;
    action.timestamp = currentTime;
    
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
    {
        action.type = MacroActionType::KeyPress;
        action.hid = hid;
        macro->actions.push_back(action);
    }
    else if (msg == WM_KEYUP || msg == WM_SYSKEYUP)
    {
        action.type = MacroActionType::KeyRelease;
        action.hid = hid;
        macro->actions.push_back(action);
    }
}

void MacroSystem::ProcessMouseInput(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (s_recordingMacroId == -1)
        return;
    
    // CORRECTIF: Ignorer les mouvements de souris (cause des bugs et trop de données)
    if (msg == WM_MOUSEMOVE)
        return;
    
    Macro* macro = GetMacro(s_recordingMacroId);
    if (!macro)
        return;
    
    uint32_t currentTime = GetTickCount() - s_recordingStartTime;
    
    MacroAction action;
    action.timestamp = currentTime;
    
    // Capturer la position actuelle de la souris pour tous les événements
    POINT cursorPos;
    GetCursorPos(&cursorPos);
    
    switch (msg)
    {
    case WM_LBUTTONDOWN:
        action.type = MacroActionType::MouseLeftDown;
        action.mousePos = cursorPos;
        break;
    case WM_LBUTTONUP:
        action.type = MacroActionType::MouseLeftUp;
        action.mousePos = cursorPos;
        break;
    case WM_RBUTTONDOWN:
        action.type = MacroActionType::MouseRightDown;
        action.mousePos = cursorPos;
        break;
    case WM_RBUTTONUP:
        action.type = MacroActionType::MouseRightUp;
        action.mousePos = cursorPos;
        break;
    case WM_MBUTTONDOWN:
        action.type = MacroActionType::MouseMiddleDown;
        action.mousePos = cursorPos;
        break;
    case WM_MBUTTONUP:
        action.type = MacroActionType::MouseMiddleUp;
        action.mousePos = cursorPos;
        break;
    case WM_MOUSEWHEEL:
        action.type = (GET_WHEEL_DELTA_WPARAM(wParam) > 0) ? 
                      MacroActionType::MouseWheelUp : MacroActionType::MouseWheelDown;
        action.mousePos = cursorPos;
        break;
    default:
        return;
    }
    
    macro->actions.push_back(action);
}

void MacroSystem::ExecuteNextAction()
{
    if (s_playingMacroId == -1)
        return;
    
    Macro* macro = GetMacro(s_playingMacroId);
    if (!macro || s_currentActionIndex >= macro->actions.size())
    {
        // Check if we should loop
        if (macro && macro->isLooping && !macro->actions.empty())
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
    uint32_t adjustedTime = (uint32_t)(currentTime * s_playbackSpeed);
    
    const MacroAction& action = macro->actions[s_currentActionIndex];
    
    if (adjustedTime >= action.timestamp)
    {
        // Execute this action
        switch (action.type)
        {
        case MacroActionType::KeyPress:
        case MacroActionType::KeyRelease:
            SendKeyInput(action.hid, action.type == MacroActionType::KeyPress);
            break;
            
        case MacroActionType::MouseMove:
        case MacroActionType::MouseLeftDown:
        case MacroActionType::MouseLeftUp:
        case MacroActionType::MouseRightDown:
        case MacroActionType::MouseRightUp:
        case MacroActionType::MouseMiddleDown:
        case MacroActionType::MouseMiddleUp:
        case MacroActionType::MouseWheelUp:
        case MacroActionType::MouseWheelDown:
            SendMouseInput(action);
            break;
            
        case MacroActionType::Delay:
            // Delay is handled by timing check above
            break;
            
        default:
            break;
        }
        
        s_currentActionIndex++;
    }
}

void MacroSystem::SendKeyInput(uint16_t hid, bool down)
{
    // Convert HID back to VK code (simplified - need proper reverse mapping)
    // For now, we'll use the HID as VK code (this works for many standard keys)
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    
    // Try to find a reasonable VK code from HID
    // This is a simplified approach - in reality you'd need a proper HID->VK mapping table
    WORD vkCode = (WORD)hid;
    if (vkCode > 255) vkCode = 0; // Invalid VK code
    
    input.ki.wVk = vkCode;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    input.ki.time = 0;
    // Mark synthetic input so hooks ignore it
    input.ki.dwExtraInfo = (ULONG_PTR)0x484A4D43ULL;
    SendInput(1, &input, sizeof(INPUT));
}

void MacroSystem::SendMouseInput(const MacroAction& action)
{
    INPUT input = {};
    input.type = INPUT_MOUSE;
    
    switch (action.type)
    {
    case MacroActionType::MouseMove:
        // Skip mouse move macros to avoid interfering with user camera/mouse
        {
            char dbg[128];
            _snprintf_s(dbg, sizeof(dbg), _TRUNCATE, "HBM: SKIP_MOUSE_MOVE to=%d,%d\n", action.mousePos.x, action.mousePos.y);
            OutputDebugStringA(dbg);
        }
        return;
        
    case MacroActionType::MouseLeftDown:
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        break;
        
    case MacroActionType::MouseLeftUp:
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        break;
        
    case MacroActionType::MouseRightDown:
        input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
        break;
        
    case MacroActionType::MouseRightUp:
        input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        break;
        
    case MacroActionType::MouseMiddleDown:
        input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
        break;
        
    case MacroActionType::MouseMiddleUp:
        input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
        break;
        
    case MacroActionType::MouseWheelUp:
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        input.mi.mouseData = WHEEL_DELTA;
        break;
        
    case MacroActionType::MouseWheelDown:
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        input.mi.mouseData = -WHEEL_DELTA;
        break;
        
    default:
        return;
    }
    input.mi.dwExtraInfo = (ULONG_PTR)0x484A4D43ULL;
    SendInput(1, &input, sizeof(INPUT));
}

// ============================================================================
// Settings
// ============================================================================

void MacroSystem::SetBlockKeysDuringPlayback(bool block)
{
    s_blockKeysDuringPlayback = block;
}

bool MacroSystem::GetBlockKeysDuringPlayback()
{
    return s_blockKeysDuringPlayback;
}

void MacroSystem::SetLoopMacro(int macroId, bool loop)
{
    Macro* macro = GetMacro(macroId);
    if (macro)
        macro->isLooping = loop;
}

bool MacroSystem::GetLoopMacro(int macroId)
{
    Macro* macro = GetMacro(macroId);
    return macro ? macro->isLooping : false;
}

// ============================================================================
// Storage
// ============================================================================

bool MacroSystem::SaveToFile(const wchar_t* filePath)
{
    // Simplified implementation - in reality you'd use proper serialization
    std::wofstream file(filePath);
    if (!file.is_open())
        return false;
    
    file << L"[Macros]" << std::endl;
    file << L"Count=" << s_macros.size() << std::endl;
    
    for (size_t i = 0; i < s_macros.size(); ++i)
    {
        const Macro& macro = s_macros[i];
        file << L"[Macro" << i << L"]" << std::endl;
        file << L"Name=" << macro.name << std::endl;
        file << L"Loop=" << (macro.isLooping ? L"1" : L"0") << std::endl;
        file << L"Speed=" << macro.playbackSpeed << std::endl;
        file << L"BlockKeys=" << (macro.blockKeysDuringPlayback ? L"1" : L"0") << std::endl;
        file << L"ActionCount=" << macro.actions.size() << std::endl;
        
        for (size_t j = 0; j < macro.actions.size(); ++j)
        {
            const MacroAction& action = macro.actions[j];
            file << L"Action" << j << L"=" << (int)action.type << L"," << action.hid << L","
                 << action.mousePos.x << L"," << action.mousePos.y << L"," 
                 << action.delayMs << L"," << action.timestamp << std::endl;
        }
    }
    
    return true;
}

bool MacroSystem::LoadFromFile(const wchar_t* filePath)
{
    // Simplified implementation - in reality you'd use proper parsing
    std::wifstream file(filePath);
    if (!file.is_open())
        return false;
    
    s_macros.clear();
    
    std::wstring line;
    std::wstring section;
    
    while (std::getline(file, line))
    {
        // Remove leading/trailing whitespace
        line.erase(0, line.find_first_not_of(L" \t\r\n"));
        line.erase(line.find_last_not_of(L" \t\r\n") + 1);
        
        if (line.empty() || line[0] == L';' || line[0] == L'#')
            continue;
        
        if (line[0] == L'[' && line.back() == L']')
        {
            section = line.substr(1, line.length() - 2);
            continue;
        }
        
        size_t eqPos = line.find(L'=');
        if (eqPos == std::wstring::npos)
            continue;
        
        std::wstring key = line.substr(0, eqPos);
        std::wstring value = line.substr(eqPos + 1);
        
        // Parse based on section and key
        // This is a simplified parser - real implementation would be more robust
    }
    
    return true;
}

// ============================================================================
// Initialization
// ============================================================================

void MacroSystem::Initialize()
{
    // Clear all state
    s_macros.clear();
    s_recordingMacroId = -1;
    s_playingMacroId = -1;
    s_recordingStartTime = 0;
    s_playbackStartTime = 0;
    s_currentActionIndex = 0;
    s_blockKeysDuringPlayback = true;
    s_playbackSpeed = 1.0f;
    
    // Try to load existing macros
    // For now, we'll create a default macro for testing
    #ifdef _DEBUG
    CreateMacro(L"Macro Test");
    #endif
}

void MacroSystem::Shutdown()
{
    // Stop any recording or playback
    StopRecording();
    StopPlayback();
    
    // Save macros before shutdown
    // SaveToFile(L"macros.ini");
    
    // Clear all data
    s_macros.clear();
}
