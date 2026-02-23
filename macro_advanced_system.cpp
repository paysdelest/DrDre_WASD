// macro_advanced_system.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <sstream>
#include <random>

#include "macro_advanced_system.h"
#include "macro_system.h"
#include "win_util.h"

// Static member definitions
std::vector<CompositeMacro> AdvancedMacroSystem::s_compositeMacros;
int AdvancedMacroSystem::s_executingMacroId = -1;
uint32_t AdvancedMacroSystem::s_executionStartTime = 0;
uint32_t AdvancedMacroSystem::s_nextCommandTime = 0;
bool AdvancedMacroSystem::s_waitingForDelay = false;

// ============================================================================
// Composite Macro Management
// ============================================================================

int AdvancedMacroSystem::CreateCompositeMacro(const std::wstring& name)
{
    CompositeMacro newMacro(name);
    s_compositeMacros.push_back(newMacro);
    return (int)s_compositeMacros.size() - 1;
}

bool AdvancedMacroSystem::DeleteCompositeMacro(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_compositeMacros.size())
        return false;
    
    if (s_executingMacroId == macroId)
        StopExecution();
    
    s_compositeMacros.erase(s_compositeMacros.begin() + macroId);
    
    if (s_executingMacroId > macroId)
        s_executingMacroId--;
    
    return true;
}

CompositeMacro* AdvancedMacroSystem::GetCompositeMacro(int macroId)
{
    if (macroId < 0 || macroId >= (int)s_compositeMacros.size())
        return nullptr;
    return &s_compositeMacros[macroId];
}

std::vector<int> AdvancedMacroSystem::GetAllCompositeMacroIds()
{
    std::vector<int> ids;
    for (int i = 0; i < (int)s_compositeMacros.size(); ++i)
        ids.push_back(i);
    return ids;
}

int AdvancedMacroSystem::GetCompositeMacroCount()
{
    return (int)s_compositeMacros.size();
}

// ============================================================================
// Script Editing
// ============================================================================

bool AdvancedMacroSystem::AddCommand(int macroId, const ScriptCommand& command)
{
    CompositeMacro* macro = GetCompositeMacro(macroId);
    if (!macro)
        return false;
    
    macro->commands.push_back(command);
    return true;
}

bool AdvancedMacroSystem::InsertCommand(int macroId, size_t index, const ScriptCommand& command)
{
    CompositeMacro* macro = GetCompositeMacro(macroId);
    if (!macro || index > macro->commands.size())
        return false;
    
    macro->commands.insert(macro->commands.begin() + index, command);
    return true;
}

bool AdvancedMacroSystem::RemoveCommand(int macroId, size_t index)
{
    CompositeMacro* macro = GetCompositeMacro(macroId);
    if (!macro || index >= macro->commands.size())
        return false;
    
    macro->commands.erase(macro->commands.begin() + index);
    return true;
}

bool AdvancedMacroSystem::UpdateCommand(int macroId, size_t index, const ScriptCommand& command)
{
    CompositeMacro* macro = GetCompositeMacro(macroId);
    if (!macro || index >= macro->commands.size())
        return false;
    
    macro->commands[index] = command;
    return true;
}

bool AdvancedMacroSystem::ClearCommands(int macroId)
{
    CompositeMacro* macro = GetCompositeMacro(macroId);
    if (!macro)
        return false;
    
    macro->commands.clear();
    return true;
}

// ============================================================================
// Script Parsing (AutoHotkey-like syntax)
// ============================================================================

std::wstring AdvancedMacroSystem::Trim(const std::wstring& str)
{
    size_t start = str.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos)
        return L"";
    
    size_t end = str.find_last_not_of(L" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::vector<std::wstring> AdvancedMacroSystem::Split(const std::wstring& str, wchar_t delimiter)
{
    std::vector<std::wstring> tokens;
    std::wstring token;
    std::wistringstream tokenStream(str);
    
    while (std::getline(tokenStream, token, delimiter))
    {
        tokens.push_back(Trim(token));
    }
    
    return tokens;
}

ScriptCommand AdvancedMacroSystem::ParseLine(const std::wstring& line)
{
    ScriptCommand cmd;
    std::wstring trimmed = Trim(line);
    
    if (trimmed.empty() || trimmed[0] == L';')
    {
        cmd.type = ScriptCommandType::Comment;
        cmd.param1 = trimmed;
        return cmd;
    }
    
    // Convert to lowercase for command matching
    std::wstring lower = trimmed;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    
    // Parse different command types
    if (lower.find(L"playmacro") == 0)
    {
        cmd.type = ScriptCommandType::PlayMacro;
        auto parts = Split(trimmed, L',');
        if (parts.size() > 1)
            cmd.param1 = parts[1];
    }
    else if (lower.find(L"delay") == 0 || lower.find(L"sleep") == 0)
    {
        cmd.type = ScriptCommandType::Delay;
        auto parts = Split(trimmed, L',');
        if (parts.size() > 1)
            cmd.intParam1 = _wtoi(parts[1].c_str());
    }
    else if (lower.find(L"send") == 0)
    {
        cmd.type = ScriptCommandType::SendText;
        size_t pos = trimmed.find(L',');
        if (pos != std::wstring::npos)
            cmd.param1 = Trim(trimmed.substr(pos + 1));
    }
    else if (lower.find(L"click") == 0)
    {
        cmd.type = ScriptCommandType::MouseClick;
        auto parts = Split(trimmed, L',');
        if (parts.size() > 1)
            cmd.intParam1 = _wtoi(parts[1].c_str());  // X
        if (parts.size() > 2)
            cmd.intParam2 = _wtoi(parts[2].c_str());  // Y
        if (parts.size() > 3)
            cmd.param1 = parts[3];  // Button (Left/Right/Middle)
    }
    else if (lower.find(L"mousemove") == 0)
    {
        cmd.type = ScriptCommandType::MouseMove;
        auto parts = Split(trimmed, L',');
        if (parts.size() > 1)
            cmd.intParam1 = _wtoi(parts[1].c_str());  // X
        if (parts.size() > 2)
            cmd.intParam2 = _wtoi(parts[2].c_str());  // Y
    }
    else if (lower.find(L"keypress") == 0)
    {
        cmd.type = ScriptCommandType::KeyPress;
        auto parts = Split(trimmed, L',');
        if (parts.size() > 1)
            cmd.param1 = parts[1];
    }
    else if (lower.find(L"keyrelease") == 0)
    {
        cmd.type = ScriptCommandType::KeyRelease;
        auto parts = Split(trimmed, L',');
        if (parts.size() > 1)
            cmd.param1 = parts[1];
    }
    else if (lower.find(L"keytap") == 0)
    {
        cmd.type = ScriptCommandType::KeyTap;
        auto parts = Split(trimmed, L',');
        if (parts.size() > 1)
            cmd.param1 = parts[1];
    }
    else if (lower.find(L"setvar") == 0)
    {
        cmd.type = ScriptCommandType::SetVariable;
        auto parts = Split(trimmed, L',');
        if (parts.size() > 1)
            cmd.param1 = parts[1];  // Variable name
        if (parts.size() > 2)
            cmd.param2 = parts[2];  // Variable value
    }
    else if (lower.find(L"if") == 0)
    {
        cmd.type = ScriptCommandType::IfCondition;
        size_t pos = trimmed.find(L',');
        if (pos != std::wstring::npos)
            cmd.param1 = Trim(trimmed.substr(pos + 1));  // Condition
    }
    else if (lower.find(L"loop") == 0)
    {
        cmd.type = ScriptCommandType::Loop;
        auto parts = Split(trimmed, L',');
        if (parts.size() > 1)
            cmd.intParam1 = _wtoi(parts[1].c_str());  // Loop count
    }
    else if (lower.find(L"break") == 0)
    {
        cmd.type = ScriptCommandType::LoopBreak;
    }
    else if (lower.find(L"random") == 0)
    {
        cmd.type = ScriptCommandType::Random;
        auto parts = Split(trimmed, L',');
        if (parts.size() > 1)
            cmd.param1 = parts[1];  // Variable name
        if (parts.size() > 2)
            cmd.intParam1 = _wtoi(parts[2].c_str());  // Min
        if (parts.size() > 3)
            cmd.intParam2 = _wtoi(parts[3].c_str());  // Max
    }
    else if (trimmed.find(L':') != std::wstring::npos && trimmed.back() == L':')
    {
        cmd.type = ScriptCommandType::Label;
        cmd.param1 = trimmed.substr(0, trimmed.length() - 1);
    }
    else if (lower.find(L"goto") == 0)
    {
        cmd.type = ScriptCommandType::Goto;
        auto parts = Split(trimmed, L',');
        if (parts.size() > 1)
            cmd.param1 = parts[1];
    }
    
    return cmd;
}

bool AdvancedMacroSystem::ParseScript(int macroId, const std::wstring& scriptText)
{
    CompositeMacro* macro = GetCompositeMacro(macroId);
    if (!macro)
        return false;
    
    macro->commands.clear();
    macro->labels.clear();
    
    std::wistringstream stream(scriptText);
    std::wstring line;
    size_t lineIndex = 0;
    
    while (std::getline(stream, line))
    {
        ScriptCommand cmd = ParseLine(line);
        
        // Register labels
        if (cmd.type == ScriptCommandType::Label)
        {
            macro->labels[cmd.param1] = lineIndex;
        }
        
        macro->commands.push_back(cmd);
        lineIndex++;
    }
    
    return true;
}

std::wstring AdvancedMacroSystem::GenerateScript(int macroId)
{
    CompositeMacro* macro = GetCompositeMacro(macroId);
    if (!macro)
        return L"";
    
    std::wostringstream script;
    
    for (const auto& cmd : macro->commands)
    {
        switch (cmd.type)
        {
        case ScriptCommandType::PlayMacro:
            script << L"PlayMacro, " << cmd.param1 << L"\n";
            break;
        case ScriptCommandType::Delay:
            script << L"Delay, " << cmd.intParam1 << L"\n";
            break;
        case ScriptCommandType::SendText:
            script << L"Send, " << cmd.param1 << L"\n";
            break;
        case ScriptCommandType::MouseClick:
            script << L"Click, " << cmd.intParam1 << L", " << cmd.intParam2;
            if (!cmd.param1.empty())
                script << L", " << cmd.param1;
            script << L"\n";
            break;
        case ScriptCommandType::MouseMove:
            script << L"MouseMove, " << cmd.intParam1 << L", " << cmd.intParam2 << L"\n";
            break;
        case ScriptCommandType::KeyPress:
            script << L"KeyPress, " << cmd.param1 << L"\n";
            break;
        case ScriptCommandType::KeyRelease:
            script << L"KeyRelease, " << cmd.param1 << L"\n";
            break;
        case ScriptCommandType::KeyTap:
            script << L"KeyTap, " << cmd.param1 << L"\n";
            break;
        case ScriptCommandType::SetVariable:
            script << L"SetVar, " << cmd.param1 << L", " << cmd.param2 << L"\n";
            break;
        case ScriptCommandType::IfCondition:
            script << L"If, " << cmd.param1 << L"\n";
            break;
        case ScriptCommandType::Loop:
            script << L"Loop, " << cmd.intParam1 << L"\n";
            break;
        case ScriptCommandType::LoopBreak:
            script << L"Break\n";
            break;
        case ScriptCommandType::Random:
            script << L"Random, " << cmd.param1 << L", " << cmd.intParam1 << L", " << cmd.intParam2 << L"\n";
            break;
        case ScriptCommandType::Label:
            script << cmd.param1 << L":\n";
            break;
        case ScriptCommandType::Goto:
            script << L"Goto, " << cmd.param1 << L"\n";
            break;
        case ScriptCommandType::Comment:
            script << cmd.param1 << L"\n";
            break;
        default:
            break;
        }
    }
    
    return script.str();
}

// ============================================================================
// Execution
// ============================================================================

bool AdvancedMacroSystem::StartExecution(int macroId)
{
    CompositeMacro* macro = GetCompositeMacro(macroId);
    if (!macro || !macro->isEnabled)
        return false;
    
    if (s_executingMacroId != -1)
        StopExecution();
    
    s_executingMacroId = macroId;
    s_executionStartTime = GetTickCount();
    s_nextCommandTime = 0;
    s_waitingForDelay = false;
    
    macro->currentCommandIndex = 0;
    macro->loopCounter = 0;
    
    return true;
}

bool AdvancedMacroSystem::StopExecution()
{
    s_executingMacroId = -1;
    s_waitingForDelay = false;
    return true;
}

bool AdvancedMacroSystem::IsExecuting()
{
    return s_executingMacroId != -1;
}

int AdvancedMacroSystem::GetExecutingMacroId()
{
    return s_executingMacroId;
}

void AdvancedMacroSystem::Tick()
{
    if (s_executingMacroId == -1)
        return;
    
    CompositeMacro* macro = GetCompositeMacro(s_executingMacroId);
    if (!macro)
    {
        StopExecution();
        return;
    }
    
    // Check if we're waiting for a delay
    if (s_waitingForDelay)
    {
        uint32_t currentTime = GetTickCount();
        if (currentTime < s_nextCommandTime)
            return;  // Still waiting
        
        s_waitingForDelay = false;
    }
    
    // Execute next command
    if (macro->currentCommandIndex >= macro->commands.size())
    {
        StopExecution();
        return;
    }
    
    const ScriptCommand& cmd = macro->commands[macro->currentCommandIndex];
    ExecuteCommand(macro, cmd);
    
    macro->currentCommandIndex++;
}

// ============================================================================
// Command Execution
// ============================================================================

void AdvancedMacroSystem::ExecuteCommand(CompositeMacro* macro, const ScriptCommand& command)
{
    switch (command.type)
    {
    case ScriptCommandType::PlayMacro:
        ExecutePlayMacro(command);
        break;
    case ScriptCommandType::Delay:
        ExecuteDelay(command);
        break;
    case ScriptCommandType::KeyPress:
        ExecuteKeyPress(command);
        break;
    case ScriptCommandType::KeyRelease:
        ExecuteKeyRelease(command);
        break;
    case ScriptCommandType::KeyTap:
        ExecuteKeyTap(command);
        break;
    case ScriptCommandType::MouseClick:
        ExecuteMouseClick(command);
        break;
    case ScriptCommandType::MouseMove:
        ExecuteMouseMove(command);
        break;
    case ScriptCommandType::SendText:
        ExecuteSendText(command);
        break;
    case ScriptCommandType::SetVariable:
        if (macro)
            macro->variables[command.param1] = command.param2;
        break;
    case ScriptCommandType::Random:
        if (macro)
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(command.intParam1, command.intParam2);
            int randomValue = dis(gen);
            macro->variables[command.param1] = std::to_wstring(randomValue);
        }
        break;
    case ScriptCommandType::Goto:
        if (macro && macro->labels.find(command.param1) != macro->labels.end())
        {
            macro->currentCommandIndex = macro->labels[command.param1];
        }
        break;
    case ScriptCommandType::IfCondition:
        if (macro && !EvaluateCondition(macro, command.param1))
        {
            // Skip next command if condition is false
            macro->currentCommandIndex++;
        }
        break;
    case ScriptCommandType::Loop:
        if (macro)
        {
            if (macro->loopCounter < command.intParam1 && macro->loopCounter < macro->maxLoopCount)
            {
                macro->loopCounter++;
                // Loop back to start (simplified - should track loop start)
                macro->currentCommandIndex = 0;
            }
            else
            {
                macro->loopCounter = 0;
            }
        }
        break;
    case ScriptCommandType::LoopBreak:
        if (macro)
        {
            macro->loopCounter = 0;
            // Skip to end of loop (simplified)
        }
        break;
    default:
        break;
    }
}

void AdvancedMacroSystem::ExecutePlayMacro(const ScriptCommand& command)
{
    // Find macro by name
    int macroId = _wtoi(command.param1.c_str());
    MacroSystem::StartPlayback(macroId);
}

void AdvancedMacroSystem::ExecuteDelay(const ScriptCommand& command)
{
    s_waitingForDelay = true;
    s_nextCommandTime = GetTickCount() + command.intParam1;
}

void AdvancedMacroSystem::ExecuteKeyPress(const ScriptCommand& command)
{
    // Convert key name to VK code (simplified)
    WORD vk = VkKeyScanW(command.param1[0]);
    
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = 0;
    // Mark synthetic input so hooks ignore it
    input.ki.dwExtraInfo = (ULONG_PTR)0x484A4D43ULL;
    SendInput(1, &input, sizeof(INPUT));
}

void AdvancedMacroSystem::ExecuteKeyRelease(const ScriptCommand& command)
{
    WORD vk = VkKeyScanW(command.param1[0]);
    
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    input.ki.dwExtraInfo = (ULONG_PTR)0x484A4D43ULL;
    SendInput(1, &input, sizeof(INPUT));
}

void AdvancedMacroSystem::ExecuteKeyTap(const ScriptCommand& command)
{
    ExecuteKeyPress(command);
    Sleep(50);
    ExecuteKeyRelease(command);
}

void AdvancedMacroSystem::ExecuteMouseClick(const ScriptCommand& command)
{
    // Move to position if specified
    if (command.intParam1 != 0 || command.intParam2 != 0)
    {
        // Skip programmatic cursor moves from macros (prevents camera/axis interference)
        char dbg[128];
        _snprintf_s(dbg, sizeof(dbg), _TRUNCATE, "HBMA: SKIP_MOUSE_MOVE to=%d,%d\n", command.intParam1, command.intParam2);
        OutputDebugStringA(dbg);
    }
    
    // Determine button
    DWORD downFlag = MOUSEEVENTF_LEFTDOWN;
    DWORD upFlag = MOUSEEVENTF_LEFTUP;
    
    if (command.param1 == L"Right")
    {
        downFlag = MOUSEEVENTF_RIGHTDOWN;
        upFlag = MOUSEEVENTF_RIGHTUP;
    }
    else if (command.param1 == L"Middle")
    {
        downFlag = MOUSEEVENTF_MIDDLEDOWN;
        upFlag = MOUSEEVENTF_MIDDLEUP;
    }
    
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = downFlag;
    inputs[0].mi.dwExtraInfo = (ULONG_PTR)0x484A4D43ULL;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = upFlag;
    inputs[1].mi.dwExtraInfo = (ULONG_PTR)0x484A4D43ULL;
    SendInput(2, inputs, sizeof(INPUT));
}

void AdvancedMacroSystem::ExecuteMouseMove(const ScriptCommand& command)
{
    // Skip mouse move macros to avoid interfering with user camera/mouse
    char dbg[128];
    _snprintf_s(dbg, sizeof(dbg), _TRUNCATE, "HBMA: SKIP_MOUSE_MOVE to=%d,%d\n", command.intParam1, command.intParam2);
    OutputDebugStringA(dbg);
}

void AdvancedMacroSystem::ExecuteSendText(const ScriptCommand& command)
{
    for (wchar_t ch : command.param1)
    {
        WORD vk = VkKeyScanW(ch);
        
        INPUT inputs[2] = {};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = vk;
        inputs[0].ki.dwFlags = 0;
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = vk;
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
        inputs[0].ki.dwExtraInfo = (ULONG_PTR)0x484A4D43ULL;
        inputs[1].ki.dwExtraInfo = (ULONG_PTR)0x484A4D43ULL;
        SendInput(2, inputs, sizeof(INPUT));
        Sleep(10);
    }
}

bool AdvancedMacroSystem::EvaluateCondition(CompositeMacro* macro, const std::wstring& condition)
{
    // Simplified condition evaluation
    // Format: "variable == value" or "variable != value"
    
    size_t eqPos = condition.find(L"==");
    size_t neqPos = condition.find(L"!=");
    
    if (eqPos != std::wstring::npos)
    {
        std::wstring varName = Trim(condition.substr(0, eqPos));
        std::wstring value = Trim(condition.substr(eqPos + 2));
        
        if (macro->variables.find(varName) != macro->variables.end())
        {
            return macro->variables[varName] == value;
        }
        return false;
    }
    else if (neqPos != std::wstring::npos)
    {
        std::wstring varName = Trim(condition.substr(0, neqPos));
        std::wstring value = Trim(condition.substr(neqPos + 2));
        
        if (macro->variables.find(varName) != macro->variables.end())
        {
            return macro->variables[varName] != value;
        }
        return true;
    }
    
    return false;
}

// ============================================================================
// Variable Management
// ============================================================================

bool AdvancedMacroSystem::SetVariable(int macroId, const std::wstring& name, const std::wstring& value)
{
    CompositeMacro* macro = GetCompositeMacro(macroId);
    if (!macro)
        return false;
    
    macro->variables[name] = value;
    return true;
}

std::wstring AdvancedMacroSystem::GetVariable(int macroId, const std::wstring& name)
{
    CompositeMacro* macro = GetCompositeMacro(macroId);
    if (!macro || macro->variables.find(name) == macro->variables.end())
        return L"";
    
    return macro->variables[name];
}

bool AdvancedMacroSystem::HasVariable(int macroId, const std::wstring& name)
{
    CompositeMacro* macro = GetCompositeMacro(macroId);
    if (!macro)
        return false;
    
    return macro->variables.find(name) != macro->variables.end();
}

// ============================================================================
// Storage
// ============================================================================

bool AdvancedMacroSystem::SaveToFile(const wchar_t* filePath)
{
    std::wofstream file(filePath);
    if (!file.is_open())
        return false;
    
    file << L"[CompositeMacros]" << std::endl;
    file << L"Count=" << s_compositeMacros.size() << std::endl;
    
    for (size_t i = 0; i < s_compositeMacros.size(); ++i)
    {
        const CompositeMacro& macro = s_compositeMacros[i];
        file << L"[CompositeMacro" << i << L"]" << std::endl;
        file << L"Name=" << macro.name << std::endl;
        file << L"Description=" << macro.description << std::endl;
        file << L"Enabled=" << (macro.isEnabled ? L"1" : L"0") << std::endl;
        file << L"Script=" << std::endl;
        file << GenerateScript((int)i) << std::endl;
        file << L"[EndScript]" << std::endl;
    }
    
    return true;
}

bool AdvancedMacroSystem::LoadFromFile(const wchar_t* filePath)
{
    std::wifstream file(filePath);
    if (!file.is_open())
        return false;
    
    s_compositeMacros.clear();
    
    // Simplified loading - real implementation would be more robust
    std::wstring line;
    while (std::getline(file, line))
    {
        // Parse file content
    }
    
    return true;
}

// ============================================================================
// Initialization
// ============================================================================

void AdvancedMacroSystem::Initialize()
{
    s_compositeMacros.clear();
    s_executingMacroId = -1;
    s_executionStartTime = 0;
    s_nextCommandTime = 0;
    s_waitingForDelay = false;
}

void AdvancedMacroSystem::Shutdown()
{
    StopExecution();
    s_compositeMacros.clear();
}
