#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>

// Advanced Macro UI System for HallJoy
// Provides UI for composite macros and script editing

// Control IDs for advanced macro UI
constexpr int MACRO_ADV_ID_NEW = 9001;
constexpr int MACRO_ADV_ID_DELETE = 9002;
constexpr int MACRO_ADV_ID_RENAME = 9003;
constexpr int MACRO_ADV_ID_EXECUTE = 9004;
constexpr int MACRO_ADV_ID_STOP = 9005;
constexpr int MACRO_ADV_ID_MACRO_LIST = 9006;
constexpr int MACRO_ADV_ID_SCRIPT_EDITOR = 9007;
constexpr int MACRO_ADV_ID_PARSE_SCRIPT = 9008;
constexpr int MACRO_ADV_ID_SAVE_SCRIPT = 9009;
constexpr int MACRO_ADV_ID_LOAD_SCRIPT = 9010;
constexpr int MACRO_ADV_ID_HELP = 9011;
constexpr int MACRO_ADV_ID_EXAMPLE = 9012;
constexpr int MACRO_ADV_ID_VALIDATE = 9013;

// Timer IDs
constexpr UINT_PTR MACRO_ADV_UI_TIMER_ID = 9014;
constexpr UINT MACRO_ADV_UI_REFRESH_MS = 100;

// Window procedure for advanced macro subpage
LRESULT CALLBACK MacroAdvancedSubpageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// UI creation helpers
HWND MacroAdvancedSubpage_Create(HWND hParent, HINSTANCE hInst);
void MacroAdvancedSubpage_RefreshUI(HWND hWnd);
void MacroAdvancedSubpage_UpdateExecutionStatus(HWND hWnd);
void MacroAdvancedSubpage_LoadScript(HWND hWnd);
void MacroAdvancedSubpage_SaveScript(HWND hWnd);
void MacroAdvancedSubpage_ShowHelp(HWND hWnd);
void MacroAdvancedSubpage_InsertExample(HWND hWnd);

// UI state management
struct MacroAdvancedUIState
{
    HWND hMacroList = nullptr;
    HWND hScriptEditor = nullptr;
    HWND hExecuteButton = nullptr;
    HWND hStatusText = nullptr;
    HWND hHelpText = nullptr;
    
    bool isExecuting = false;
    int selectedMacroId = -1;
    uint32_t lastUpdateTime = 0;
};

