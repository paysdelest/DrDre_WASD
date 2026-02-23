#pragma once
#include <windows.h>
#include <string>
#include <vector>

// HallJoy V.2.1 Macro UI - Very Intuitive and Fast Access
// Quick macro creation, management, and execution

// UI Control IDs
#define V21UI_MAIN_WINDOW 1000
#define V21UI_QUICK_CREATE_BTN 1001
#define V21UI_RECORD_BTN 1002
#define V21UI_PLAY_BTN 1003
#define V21UI_STOP_BTN 1004
#define V21UI_MACRO_LIST 1005
#define V21UI_SEARCH_BOX 1006
#define V21UI_CATEGORY_LIST 1007
#define V21UI_FAVORITE_LIST 1008
#define V21UI_TEMPLATE_LIST 1009
#define V21UI_NAME_EDIT 1010
#define V21UI_DESC_EDIT 1011
#define V21UI_CATEGORY_EDIT 1012
#define V21UI_HOTKEY_EDIT 1013
#define V21UI_STATUS_LABEL 1014
#define V21UI_QUICK_PING_BTN 1015
#define V21UI_QUICK_HELLO_BTN 1016
#define V21UI_QUICK_CLICK_BTN 1017
#define V21UI_QUICK_TEXT_BTN 1018

// UI State structure
struct HallJoyV21UIState
{
    HWND hMainWindow = nullptr;
    HWND hMacroList = nullptr;
    HWND hSearchBox = nullptr;
    HWND hCategoryList = nullptr;
    HWND hFavoriteList = nullptr;
    HWND hTemplateList = nullptr;
    HWND hNameEdit = nullptr;
    HWND hDescEdit = nullptr;
    HWND hCategoryEdit = nullptr;
    HWND hHotkeyEdit = nullptr;
    HWND hStatusLabel = nullptr;
    int selectedMacroId = -1;
    bool isRecording = false;
    bool isPlaying = false;
    std::wstring currentSearch;
    std::wstring currentCategory;
};

// Window procedures
LRESULT CALLBACK HallJoyV21MacroUI_MainProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Creation functions
HWND HallJoyV21MacroUI_CreateMainWindow(HINSTANCE hInst);
HWND HallJoyV21MacroUI_CreateQuickPanel(HWND hParent, HINSTANCE hInst);
HWND HallJoyV21MacroUI_CreateMacroList(HWND hParent, HINSTANCE hInst);
HWND HallJoyV21MacroUI_CreateTemplatePanel(HWND hParent, HINSTANCE hInst);

// UI update functions
void HallJoyV21MacroUI_RefreshMacroList(HWND hWnd);
void HallJoyV21MacroUI_RefreshCategoryList(HWND hWnd);
void HallJoyV21MacroUI_RefreshFavoriteList(HWND hWnd);
void HallJoyV21MacroUI_UpdateStatus(HWND hWnd, const std::wstring& status);
void HallJoyV21MacroUI_UpdateButtonStates(HWND hWnd);

// Action functions
void HallJoyV21MacroUI_CreateQuickMacro(HWND hWnd, const std::wstring& templateType);
void HallJoyV21MacroUI_StartRecording(HWND hWnd);
void HallJoyV21MacroUI_StopRecording(HWND hWnd);
void HallJoyV21MacroUI_PlaySelectedMacro(HWND hWnd);
void HallJoyV21MacroUI_DeleteSelectedMacro(HWND hWnd);
void HallJoyV21MacroUI_ToggleFavorite(HWND hWnd);
void HallJoyV21MacroUI_SearchMacros(HWND hWnd, const std::wstring& searchTerm);
void HallJoyV21MacroUI_FilterByCategory(HWND hWnd, const std::wstring& category);

// Quick access functions
void HallJoyV21MacroUI_ShowQuickCreateDialog(HWND hParent);
void HallJoyV21MacroUI_ShowMacroProperties(HWND hParent, int macroId);
void HallJoyV21MacroUI_ExecuteMacro(int macroId);

// Helper functions
std::wstring HallJoyV21MacroUI_GetMacroDisplayText(int macroId);
std::wstring HallJoyV21MacroUI_GetTemplateDisplayName(const std::wstring& templateType);
void HallJoyV21MacroUI_InitializeUIState(HallJoyV21UIState* state);

// Global UI state
extern HallJoyV21UIState g_v21UIState;
