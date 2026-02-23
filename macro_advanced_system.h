#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <windows.h>
#include "macro_system.h"

// Advanced Macro System for HallJoy
// Provides composite macros, scripting, and conditional logic
// Inspired by AutoHotkey functionality

// ============================================================================
// Script Command Types
// ============================================================================

enum class ScriptCommandType : uint8_t
{
    None = 0,
    PlayMacro = 1,              // Play an existing macro
    Delay = 2,                  // Wait for specified time
    KeyPress = 3,               // Press a key
    KeyRelease = 4,             // Release a key
    KeyTap = 5,                 // Press and release a key
    MouseClick = 6,             // Click mouse button
    MouseMove = 7,              // Move mouse to position
    SetVariable = 8,            // Set a variable value
    IfCondition = 9,            // Conditional execution
    Loop = 10,                  // Loop execution
    LoopBreak = 11,             // Break from loop
    WaitForKey = 12,            // Wait for key press
    WaitForMouse = 13,          // Wait for mouse click
    Comment = 14,               // Comment line (ignored)
    Label = 15,                 // Label for goto
    Goto = 16,                  // Jump to label
    Random = 17,                // Generate random number
    SendText = 18,              // Type text string
    PixelSearch = 19,           // Search for pixel color
    ImageSearch = 20,           // Search for image on screen
};

// ============================================================================
// Script Command Structure
// ============================================================================

struct ScriptCommand
{
    ScriptCommandType type = ScriptCommandType::None;
    
    // Command parameters
    std::wstring param1;        // General purpose parameter
    std::wstring param2;        // General purpose parameter
    std::wstring param3;        // General purpose parameter
    int intParam1 = 0;          // Integer parameter
    int intParam2 = 0;          // Integer parameter
    int intParam3 = 0;          // Integer parameter
    
    // For conditional/loop commands
    std::vector<ScriptCommand> subCommands;  // Nested commands
    
    ScriptCommand() = default;
    ScriptCommand(ScriptCommandType t) : type(t) {}
};

// ============================================================================
// Composite Macro Structure
// ============================================================================

struct CompositeMacro
{
    std::wstring name;
    std::wstring description;
    std::vector<ScriptCommand> commands;
    bool isEnabled = true;
    
    // Execution state
    size_t currentCommandIndex = 0;
    std::map<std::wstring, std::wstring> variables;
    std::map<std::wstring, size_t> labels;
    int loopCounter = 0;
    int maxLoopCount = 1000;  // Safety limit
    
    CompositeMacro() = default;
    CompositeMacro(const std::wstring& macroName) : name(macroName) {}
};

// ============================================================================
// Advanced Macro System Class
// ============================================================================

class AdvancedMacroSystem
{
public:
    // Composite macro management
    static int CreateCompositeMacro(const std::wstring& name);
    static bool DeleteCompositeMacro(int macroId);
    static CompositeMacro* GetCompositeMacro(int macroId);
    static std::vector<int> GetAllCompositeMacroIds();
    static int GetCompositeMacroCount();
    
    // Script editing
    static bool AddCommand(int macroId, const ScriptCommand& command);
    static bool InsertCommand(int macroId, size_t index, const ScriptCommand& command);
    static bool RemoveCommand(int macroId, size_t index);
    static bool UpdateCommand(int macroId, size_t index, const ScriptCommand& command);
    static bool ClearCommands(int macroId);
    
    // Script parsing (AutoHotkey-like syntax)
    static bool ParseScript(int macroId, const std::wstring& scriptText);
    static std::wstring GenerateScript(int macroId);
    
    // Execution
    static bool StartExecution(int macroId);
    static bool StopExecution();
    static bool IsExecuting();
    static int GetExecutingMacroId();
    static void Tick();  // Called from main loop
    
    // Variable management
    static bool SetVariable(int macroId, const std::wstring& name, const std::wstring& value);
    static std::wstring GetVariable(int macroId, const std::wstring& name);
    static bool HasVariable(int macroId, const std::wstring& name);
    
    // Storage
    static bool SaveToFile(const wchar_t* filePath);
    static bool LoadFromFile(const wchar_t* filePath);
    
    // Initialization
    static void Initialize();
    static void Shutdown();
    
private:
    // Execution helpers
    static void ExecuteCommand(CompositeMacro* macro, const ScriptCommand& command);
    static void ExecutePlayMacro(const ScriptCommand& command);
    static void ExecuteDelay(const ScriptCommand& command);
    static void ExecuteKeyPress(const ScriptCommand& command);
    static void ExecuteKeyRelease(const ScriptCommand& command);
    static void ExecuteKeyTap(const ScriptCommand& command);
    static void ExecuteMouseClick(const ScriptCommand& command);
    static void ExecuteMouseMove(const ScriptCommand& command);
    static void ExecuteSendText(const ScriptCommand& command);
    static bool EvaluateCondition(CompositeMacro* macro, const std::wstring& condition);
    
    // Parsing helpers
    static ScriptCommand ParseLine(const std::wstring& line);
    static std::wstring Trim(const std::wstring& str);
    static std::vector<std::wstring> Split(const std::wstring& str, wchar_t delimiter);
    
    // State
    static std::vector<CompositeMacro> s_compositeMacros;
    static int s_executingMacroId;
    static uint32_t s_executionStartTime;
    static uint32_t s_nextCommandTime;
    static bool s_waitingForDelay;
};

