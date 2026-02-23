#include "HallJoy_V21_Macro_UI.h"
#include "HallJoy_V21_Macro.h"
#include <commctrl.h>
#include <shellapi.h>

// Global UI state
HallJoyV21UIState g_v21UIState;

// ============================================================================
// Window Procedures
// ============================================================================

LRESULT CALLBACK HallJoyV21MacroUI_MainProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HallJoyV21UIState* state = (HallJoyV21UIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    
    switch (msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        state = (HallJoyV21UIState*)cs->lpCreateParams;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)state);
        state->hMainWindow = hWnd;
        
        // Create UI elements
        state->hSearchBox = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | WS_BORDER,
            10, 10, 200, 25, hWnd, (HMENU)V21UI_SEARCH_BOX, GetModuleHandleW(nullptr), nullptr);
        
        state->hMacroList = CreateWindowW(L"LISTBOX", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | LBS_STANDARD,
            10, 45, 300, 200, hWnd, (HMENU)V21UI_MACRO_LIST, GetModuleHandleW(nullptr), nullptr);
        
        state->hCategoryList = CreateWindowW(L"LISTBOX", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | LBS_STANDARD,
            320, 45, 150, 200, hWnd, (HMENU)V21UI_CATEGORY_LIST, GetModuleHandleW(nullptr), nullptr);
        
        state->hFavoriteList = CreateWindowW(L"LISTBOX", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | LBS_STANDARD,
            480, 45, 150, 200, hWnd, (HMENU)V21UI_FAVORITE_LIST, GetModuleHandleW(nullptr), nullptr);
        
        state->hTemplateList = CreateWindowW(L"LISTBOX", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | LBS_STANDARD,
            10, 255, 300, 100, hWnd, (HMENU)V21UI_TEMPLATE_LIST, GetModuleHandleW(nullptr), nullptr);
        
        // Quick action buttons
        CreateWindowW(L"BUTTON", L"Quick Ping", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            10, 365, 100, 30, hWnd, (HMENU)V21UI_QUICK_PING_BTN, GetModuleHandleW(nullptr), nullptr);
        
        CreateWindowW(L"BUTTON", L"Quick Hello", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            120, 365, 100, 30, hWnd, (HMENU)V21UI_QUICK_HELLO_BTN, GetModuleHandleW(nullptr), nullptr);
        
        CreateWindowW(L"BUTTON", L"Quick Click", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            230, 365, 100, 30, hWnd, (HMENU)V21UI_QUICK_CLICK_BTN, GetModuleHandleW(nullptr), nullptr);
        
        CreateWindowW(L"BUTTON", L"Quick Text", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            340, 365, 100, 30, hWnd, (HMENU)V21UI_QUICK_TEXT_BTN, GetModuleHandleW(nullptr), nullptr);
        
        // Control buttons
        CreateWindowW(L"BUTTON", L"Create Macro", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            10, 405, 100, 30, hWnd, (HMENU)V21UI_QUICK_CREATE_BTN, GetModuleHandleW(nullptr), nullptr);
        
        CreateWindowW(L"BUTTON", L"Record", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            120, 405, 80, 30, hWnd, (HMENU)V21UI_RECORD_BTN, GetModuleHandleW(nullptr), nullptr);
        
        CreateWindowW(L"BUTTON", L"Play", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            210, 405, 80, 30, hWnd, (HMENU)V21UI_PLAY_BTN, GetModuleHandleW(nullptr), nullptr);
        
        CreateWindowW(L"BUTTON", L"Stop", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            300, 405, 80, 30, hWnd, (HMENU)V21UI_STOP_BTN, GetModuleHandleW(nullptr), nullptr);
        
        // Status label
        state->hStatusLabel = CreateWindowW(L"STATIC", L"Ready", WS_VISIBLE | WS_CHILD | SS_LEFT,
            10, 445, 620, 25, hWnd, (HMENU)V21UI_STATUS_LABEL, GetModuleHandleW(nullptr), nullptr);
        
        // Initialize UI
        HallJoyV21MacroUI_RefreshMacroList(hWnd);
        HallJoyV21MacroUI_RefreshCategoryList(hWnd);
        HallJoyV21MacroUI_RefreshFavoriteList(hWnd);
        HallJoyV21MacroUI_UpdateButtonStates(hWnd);
        
        // Add template items
        SendMessageW(state->hTemplateList, LB_ADDSTRING, 0, (LPARAM)L"Quick Ping - Press P key");
        SendMessageW(state->hTemplateList, LB_ADDSTRING, 0, (LPARAM)L"Quick Hello - Type 'Hello'");
        SendMessageW(state->hTemplateList, LB_ADDSTRING, 0, (LPARAM)L"Quick Click - Double left click");
        SendMessageW(state->hTemplateList, LB_ADDSTRING, 0, (LPARAM)L"Quick Text - Type custom text");
        
        break;
    }
    
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);
        
        switch (wmId)
        {
        case V21UI_QUICK_CREATE_BTN:
            HallJoyV21MacroUI_ShowQuickCreateDialog(hWnd);
            break;
            
        case V21UI_RECORD_BTN:
            if (!state->isRecording)
                HallJoyV21MacroUI_StartRecording(hWnd);
            else
                HallJoyV21MacroUI_StopRecording(hWnd);
            break;
            
        case V21UI_PLAY_BTN:
            HallJoyV21MacroUI_PlaySelectedMacro(hWnd);
            break;
            
        case V21UI_STOP_BTN:
            HallJoy_V21_Macro::StopPlayback();
            HallJoyV21MacroUI_UpdateButtonStates(hWnd);
            HallJoyV21MacroUI_UpdateStatus(hWnd, L"Playback stopped");
            break;
            
        case V21UI_QUICK_PING_BTN:
            HallJoyV21MacroUI_CreateQuickMacro(hWnd, L"ping");
            break;
            
        case V21UI_QUICK_HELLO_BTN:
            HallJoyV21MacroUI_CreateQuickMacro(hWnd, L"hello");
            break;
            
        case V21UI_QUICK_CLICK_BTN:
            HallJoyV21MacroUI_CreateQuickMacro(hWnd, L"double_click");
            break;
            
        case V21UI_QUICK_TEXT_BTN:
            HallJoyV21MacroUI_ShowQuickCreateDialog(hWnd);
            break;
            
        case V21UI_SEARCH_BOX:
            if (wmEvent == EN_CHANGE)
            {
                wchar_t buffer[256];
                GetWindowTextW(state->hSearchBox, buffer, 256);
                HallJoyV21MacroUI_SearchMacros(hWnd, buffer);
            }
            break;
            
        case V21UI_MACRO_LIST:
            if (wmEvent == LBN_SELCHANGE)
            {
                int sel = (int)SendMessageW(state->hMacroList, LB_GETCURSEL, 0, 0);
                if (sel >= 0)
                {
                    std::vector<int> macros = HallJoy_V21_Macro::GetAllMacroIds();
                    if (sel < (int)macros.size())
                    {
                        state->selectedMacroId = macros[sel];
                        HallJoyV21MacroUI_UpdateButtonStates(hWnd);
                    }
                }
            }
            break;
            
        case V21UI_CATEGORY_LIST:
            if (wmEvent == LBN_SELCHANGE)
            {
                int sel = (int)SendMessageW(state->hCategoryList, LB_GETCURSEL, 0, 0);
                if (sel >= 0)
                {
                    wchar_t buffer[256];
                    SendMessageW(state->hCategoryList, LB_GETTEXT, sel, (LPARAM)buffer);
                    HallJoyV21MacroUI_FilterByCategory(hWnd, buffer);
                }
            }
            break;
            
        case V21UI_FAVORITE_LIST:
            if (wmEvent == LBN_DBLCLK)
            {
                int sel = (int)SendMessageW(state->hFavoriteList, LB_GETCURSEL, 0, 0);
                if (sel >= 0)
                {
                    std::vector<int> favorites = HallJoy_V21_Macro::GetFavoriteMacros();
                    if (sel < (int)favorites.size())
                    {
                        HallJoyV21MacroUI_ExecuteMacro(favorites[sel]);
                    }
                }
            }
            break;
            
        case V21UI_TEMPLATE_LIST:
            if (wmEvent == LBN_DBLCLK)
            {
                int sel = (int)SendMessageW(state->hTemplateList, LB_GETCURSEL, 0, 0);
                if (sel == 0) HallJoyV21MacroUI_CreateQuickMacro(hWnd, L"ping");
                else if (sel == 1) HallJoyV21MacroUI_CreateQuickMacro(hWnd, L"hello");
                else if (sel == 2) HallJoyV21MacroUI_CreateQuickMacro(hWnd, L"double_click");
                else if (sel == 3) HallJoyV21MacroUI_ShowQuickCreateDialog(hWnd);
            }
            break;
        }
        break;
    }
    
    case WM_SIZE:
    {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        
        // Resize controls
        SetWindowPos(state->hSearchBox, nullptr, 10, 10, width - 20, 25, SWP_NOZORDER);
        SetWindowPos(state->hMacroList, nullptr, 10, 45, 300, height - 150, SWP_NOZORDER);
        SetWindowPos(state->hCategoryList, nullptr, 320, 45, 150, height - 150, SWP_NOZORDER);
        SetWindowPos(state->hFavoriteList, nullptr, 480, 45, 150, height - 150, SWP_NOZORDER);
        SetWindowPos(state->hTemplateList, nullptr, 10, height - 100, 300, 90, SWP_NOZORDER);
        
        break;
    }
    
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;
        
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================================
// Creation Functions
// ============================================================================

HWND HallJoyV21MacroUI_CreateMainWindow(HINSTANCE hInst)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = HallJoyV21MacroUI_MainProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"HallJoyV21MacroUI";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    
    RegisterClassExW(&wc);
    
    return CreateWindowExW(0, L"HallJoyV21MacroUI", L"HallJoy V.2.1 Macros - Advanced System",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 650, 500,
        nullptr, nullptr, hInst, &g_v21UIState);
}

// ============================================================================
// UI Update Functions
// ============================================================================

void HallJoyV21MacroUI_RefreshMacroList(HWND hWnd)
{
    HallJoyV21UIState* state = (HallJoyV21UIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state || !state->hMacroList)
        return;
    
    SendMessageW(state->hMacroList, LB_RESETCONTENT, 0, 0);
    
    std::vector<int> macros = HallJoy_V21_Macro::GetAllMacroIds();
    for (int macroId : macros)
    {
        std::wstring displayText = HallJoyV21MacroUI_GetMacroDisplayText(macroId);
        SendMessageW(state->hMacroList, LB_ADDSTRING, 0, (LPARAM)displayText.c_str());
    }
}

void HallJoyV21MacroUI_RefreshCategoryList(HWND hWnd)
{
    HallJoyV21UIState* state = (HallJoyV21UIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state || !state->hCategoryList)
        return;
    
    SendMessageW(state->hCategoryList, LB_RESETCONTENT, 0, 0);
    
    // Add common categories
    SendMessageW(state->hCategoryList, LB_ADDSTRING, 0, (LPARAM)L"All Macros");
    SendMessageW(state->hCategoryList, LB_ADDSTRING, 0, (LPARAM)L"Favorites");
    SendMessageW(state->hCategoryList, LB_ADDSTRING, 0, (LPARAM)L"Quick Actions");
    SendMessageW(state->hCategoryList, LB_ADDSTRING, 0, (LPARAM)L"Text Macros");
    SendMessageW(state->hCategoryList, LB_ADDSTRING, 0, (LPARAM)L"Mouse Actions");
    SendMessageW(state->hCategoryList, LB_ADDSTRING, 0, (LPARAM)L"Keyboard Actions");
}

void HallJoyV21MacroUI_RefreshFavoriteList(HWND hWnd)
{
    HallJoyV21UIState* state = (HallJoyV21UIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state || !state->hFavoriteList)
        return;
    
    SendMessageW(state->hFavoriteList, LB_RESETCONTENT, 0, 0);
    
    std::vector<int> favorites = HallJoy_V21_Macro::GetFavoriteMacros();
    for (int macroId : favorites)
    {
        HallJoyV21Macro* macro = HallJoy_V21_Macro::GetMacro(macroId);
        if (macro)
        {
            std::wstring displayText = L"★ " + macro->name;
            SendMessageW(state->hFavoriteList, LB_ADDSTRING, 0, (LPARAM)displayText.c_str());
        }
    }
}

void HallJoyV21MacroUI_UpdateStatus(HWND hWnd, const std::wstring& status)
{
    HallJoyV21UIState* state = (HallJoyV21UIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state || !state->hStatusLabel)
        return;
    
    SetWindowTextW(state->hStatusLabel, status.c_str());
}

void HallJoyV21MacroUI_UpdateButtonStates(HWND hWnd)
{
    HallJoyV21UIState* state = (HallJoyV21UIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state)
        return;
    
    // Update record button
    HWND hRecordBtn = GetDlgItem(hWnd, V21UI_RECORD_BTN);
    if (hRecordBtn)
    {
        SetWindowTextW(hRecordBtn, state->isRecording ? L"Stop Recording" : L"Record");
    }
    
    // Update play button
    HWND hPlayBtn = GetDlgItem(hWnd, V21UI_PLAY_BTN);
    if (hPlayBtn)
    {
        bool canPlay = (state->selectedMacroId >= 0 && !HallJoy_V21_Macro::IsPlaying());
        EnableWindow(hPlayBtn, canPlay);
    }
    
    // Update stop button
    HWND hStopBtn = GetDlgItem(hWnd, V21UI_STOP_BTN);
    if (hStopBtn)
    {
        EnableWindow(hStopBtn, HallJoy_V21_Macro::IsPlaying());
    }
}

// ============================================================================
// Action Functions
// ============================================================================

void HallJoyV21MacroUI_CreateQuickMacro(HWND hWnd, const std::wstring& templateType)
{
    std::wstring macroName = L"Quick " + templateType;
    int macroId = HallJoy_V21_Macro::CreateQuickMacro(macroName, templateType);
    
    if (macroId >= 0)
    {
        HallJoyV21MacroUI_RefreshMacroList(hWnd);
        HallJoyV21MacroUI_UpdateStatus(hWnd, L"Quick macro created: " + macroName);
    }
    else
    {
        HallJoyV21MacroUI_UpdateStatus(hWnd, L"Failed to create quick macro");
    }
}

void HallJoyV21MacroUI_StartRecording(HWND hWnd)
{
    HallJoyV21UIState* state = (HallJoyV21UIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state)
        return;
    
    // Create a new macro for recording
    int macroId = HallJoy_V21_Macro::CreateMacro(L"Recording Macro", L"Macro being recorded");
    if (macroId >= 0)
    {
        if (HallJoy_V21_Macro::StartRecording(macroId))
        {
            state->isRecording = true;
            state->selectedMacroId = macroId;
            HallJoyV21MacroUI_RefreshMacroList(hWnd);
            HallJoyV21MacroUI_UpdateButtonStates(hWnd);
            HallJoyV21MacroUI_UpdateStatus(hWnd, L"Recording started...");
        }
    }
}

void HallJoyV21MacroUI_StopRecording(HWND hWnd)
{
    HallJoyV21UIState* state = (HallJoyV21UIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state)
        return;
    
    if (HallJoy_V21_Macro::StopRecording())
    {
        state->isRecording = false;
        HallJoyV21MacroUI_RefreshMacroList(hWnd);
        HallJoyV21MacroUI_UpdateButtonStates(hWnd);
        HallJoyV21MacroUI_UpdateStatus(hWnd, L"Recording stopped");
    }
}

void HallJoyV21MacroUI_PlaySelectedMacro(HWND hWnd)
{
    HallJoyV21UIState* state = (HallJoyV21UIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state || state->selectedMacroId < 0)
        return;
    
    if (HallJoy_V21_Macro::StartPlayback(state->selectedMacroId))
    {
        HallJoyV21MacroUI_UpdateButtonStates(hWnd);
        HallJoyV21MacroUI_UpdateStatus(hWnd, L"Playing macro...");
    }
}

void HallJoyV21MacroUI_SearchMacros(HWND hWnd, const std::wstring& searchTerm)
{
    HallJoyV21UIState* state = (HallJoyV21UIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state || !state->hMacroList)
        return;
    
    SendMessageW(state->hMacroList, LB_RESETCONTENT, 0, 0);
    
    if (searchTerm.empty())
    {
        // Show all macros
        std::vector<int> macros = HallJoy_V21_Macro::GetAllMacroIds();
        for (int macroId : macros)
        {
            std::wstring displayText = HallJoyV21MacroUI_GetMacroDisplayText(macroId);
            SendMessageW(state->hMacroList, LB_ADDSTRING, 0, (LPARAM)displayText.c_str());
        }
    }
    else
    {
        // Search macros
        std::vector<int> results = HallJoy_V21_Macro::SearchMacros(searchTerm);
        for (int macroId : results)
        {
            std::wstring displayText = HallJoyV21MacroUI_GetMacroDisplayText(macroId);
            SendMessageW(state->hMacroList, LB_ADDSTRING, 0, (LPARAM)displayText.c_str());
        }
    }
}

void HallJoyV21MacroUI_FilterByCategory(HWND hWnd, const std::wstring& category)
{
    HallJoyV21UIState* state = (HallJoyV21UIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state || !state->hMacroList)
        return;
    
    SendMessageW(state->hMacroList, LB_RESETCONTENT, 0, 0);
    
    std::vector<int> macros;
    
    if (category == L"All Macros")
    {
        macros = HallJoy_V21_Macro::GetAllMacroIds();
    }
    else if (category == L"Favorites")
    {
        macros = HallJoy_V21_Macro::GetFavoriteMacros();
    }
    else
    {
        macros = HallJoy_V21_Macro::GetMacrosByCategory(category);
    }
    
    for (int macroId : macros)
    {
        std::wstring displayText = HallJoyV21MacroUI_GetMacroDisplayText(macroId);
        SendMessageW(state->hMacroList, LB_ADDSTRING, 0, (LPARAM)displayText.c_str());
    }
}

void HallJoyV21MacroUI_ExecuteMacro(int macroId)
{
    if (HallJoy_V21_Macro::StartPlayback(macroId))
    {
        HallJoyV21Macro* macro = HallJoy_V21_Macro::GetMacro(macroId);
        if (macro)
        {
            // Update status in main window if it exists
            if (g_v21UIState.hMainWindow)
            {
                HallJoyV21MacroUI_UpdateStatus(g_v21UIState.hMainWindow, L"Executing: " + macro->name);
            }
        }
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

std::wstring HallJoyV21MacroUI_GetMacroDisplayText(int macroId)
{
    HallJoyV21Macro* macro = HallJoy_V21_Macro::GetMacro(macroId);
    if (!macro)
        return L"";
    
    std::wstring display = macro->name;
    if (macro->isFavorite)
        display = L"★ " + display;
    
    display += L" (" + std::to_wstring(macro->actions.size()) + L" actions)";
    
    return display;
}

void HallJoyV21MacroUI_ShowQuickCreateDialog(HWND hParent)
{
    // Simple message box for now - could be expanded to a proper dialog
    MessageBoxW(hParent, L"Quick create dialog would open here\nFor now, use the template list below", 
                L"Quick Create Macro", MB_OK | MB_ICONINFORMATION);
}

void HallJoyV21MacroUI_ShowMacroProperties(HWND hParent, int macroId)
{
    HallJoyV21Macro* macro = HallJoy_V21_Macro::GetMacro(macroId);
    if (!macro)
        return;
    
    std::wstring info = L"Name: " + macro->name + L"\n";
    info += L"Description: " + macro->description + L"\n";
    info += L"Actions: " + std::to_wstring(macro->actions.size()) + L"\n";
    info += L"Executions: " + std::to_wstring(macro->executionCount) + L"\n";
    info += L"Favorite: " + (macro->isFavorite ? L"Yes" : L"No");
    
    MessageBoxW(hParent, info.c_str(), L"Macro Properties", MB_OK | MB_ICONINFORMATION);
}
