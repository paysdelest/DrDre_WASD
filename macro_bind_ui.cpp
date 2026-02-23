#include "macro_bind_ui.h"
#include "macro_bind_system.h"
#include "ui_theme.h"
#include <commctrl.h>
#include <sstream>
#include <iomanip>

// Custom drawing function for bind macro buttons
auto DrawBindMacroButton = [](const DRAWITEMSTRUCT* dis) {
    if (!dis) return;
    
    RECT rc = dis->rcItem;
    HDC hdc = dis->hDC;
    
    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool hot = (dis->itemState & ODS_HOTLIGHT) != 0;
    
    COLORREF bg = UiTheme::Color_ControlBg();
    if (pressed)
        bg = RGB(42, 42, 44);
    else if (hot)
        bg = RGB(40, 40, 42);
    
    HBRUSH br = CreateSolidBrush(bg);
    FillRect(hdc, &rc, br);
    DeleteObject(br);
    
    HPEN pen = CreatePen(PS_SOLID, 1, UiTheme::Color_Border());
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    
    wchar_t text[128]{};
    GetWindowTextW(dis->hwndItem, text, (int)(sizeof(text) / sizeof(text[0])));
    
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, disabled ? UiTheme::Color_TextMuted() : UiTheme::Color_Text());
    DrawTextW(hdc, text, -1, &rc,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    
    if (dis->itemState & ODS_FOCUS)
    {
        RECT focus = rc;
        InflateRect(&focus, -4, -4);
        DrawFocusRect(hdc, &focus);
    }
};

// Window creation function
int BindMacroSubpage_OnCreate(HWND hWnd, LPARAM lParam)
{
    BindMacroUIState* state = new BindMacroUIState();
    SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)state);
    
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    
    // Apply dark theme to all controls
    auto ApplyDarkTheme = [](HWND hwnd) {
        if (!hwnd) return;
        
        UiTheme::ApplyToControl(hwnd);
        
        wchar_t className[256];
        GetClassNameW(hwnd, className, 256);
        
        if (wcscmp(className, L"BUTTON") == 0 || wcscmp(className, L"STATIC") == 0) {
            SendMessageW(hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
            
            HMODULE hUxtheme = LoadLibraryW(L"uxtheme.dll");
            if (hUxtheme) {
                typedef HRESULT(WINAPI* SetWindowThemePtr)(HWND, LPCWSTR, LPCWSTR);
                auto pSetWindowTheme = (SetWindowThemePtr)GetProcAddress(hUxtheme, "SetWindowTheme");
                if (pSetWindowTheme) {
                    pSetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
                }
                FreeLibrary(hUxtheme);
            }
        }
        
        InvalidateRect(hwnd, nullptr, TRUE);
        UpdateWindow(hwnd);
    };
    
    // Create main controls
    state->hMacroList = CreateWindowW(L"LISTBOX", L"", 
        WS_VISIBLE | WS_CHILD | WS_BORDER | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY,
        10, 10, 200, 150, hWnd, (HMENU)BIND_MACRO_ID_LIST, hInst, nullptr);
    
    state->hMacroNameEdit = CreateWindowW(L"EDIT", L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        220, 10, 150, 25, hWnd, (HMENU)BIND_MACRO_ID_NAME_EDIT, hInst, nullptr);
    
    state->hCreateButton = CreateWindowW(L"BUTTON", L"🔴 Create",
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        380, 10, 100, 25, hWnd, (HMENU)BIND_MACRO_ID_CREATE_BUTTON, hInst, nullptr);
    
    state->hDeleteButton = CreateWindowW(L"BUTTON", L"🗑️ Delete",
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        490, 10, 100, 25, hWnd, (HMENU)BIND_MACRO_ID_DELETE_BUTTON, hInst, nullptr);
    
    state->hRecordButton = CreateWindowW(L"BUTTON", L"🔴 Record",
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        220, 45, 100, 25, hWnd, (HMENU)BIND_MACRO_ID_RECORD_BUTTON, hInst, nullptr);
    
    state->hPlayButton = CreateWindowW(L"BUTTON", L"▶️ Play",
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        330, 45, 100, 25, hWnd, (HMENU)BIND_MACRO_ID_PLAY_BUTTON, hInst, nullptr);
    
    state->hStopButton = CreateWindowW(L"BUTTON", L"⏹️ Stop",
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        440, 45, 100, 25, hWnd, (HMENU)BIND_MACRO_ID_STOP_BUTTON, hInst, nullptr);
    
    // Direct binding group
    state->hBindingGroupBox = CreateWindowW(L"BUTTON", L"Direct Binding",
        WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
        10, 85, 580, 120, hWnd, nullptr, hInst, nullptr);
    
    state->hMouseLeftDownCheck = CreateWindowW(L"BUTTON", L"Left click ↓",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        20, 105, 120, 20, hWnd, (HMENU)BIND_MACRO_ID_MOUSE_LEFT_DOWN_CHECK, hInst, nullptr);
    
    state->hMouseLeftUpCheck = CreateWindowW(L"BUTTON", L"Left click ↑",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        150, 105, 120, 20, hWnd, (HMENU)BIND_MACRO_ID_MOUSE_LEFT_UP_CHECK, hInst, nullptr);
    
    state->hMouseRightDownCheck = CreateWindowW(L"BUTTON", L"Right click ↓",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        280, 105, 120, 20, hWnd, (HMENU)BIND_MACRO_ID_MOUSE_RIGHT_DOWN_CHECK, hInst, nullptr);
    
    state->hMouseRightUpCheck = CreateWindowW(L"BUTTON", L"Right click ↑",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        410, 105, 120, 20, hWnd, (HMENU)BIND_MACRO_ID_MOUSE_RIGHT_UP_CHECK, hInst, nullptr);
    
    state->hMouseLeftPressedCheck = CreateWindowW(L"BUTTON", L"Left click (held)",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        20, 130, 150, 20, hWnd, (HMENU)BIND_MACRO_ID_MOUSE_LEFT_PRESSED_CHECK, hInst, nullptr);
    
    state->hMouseRightPressedCheck = CreateWindowW(L"BUTTON", L"Right click (held)",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        180, 130, 150, 20, hWnd, (HMENU)BIND_MACRO_ID_MOUSE_RIGHT_PRESSED_CHECK, hInst, nullptr);
    
    state->hKeyBindingEdit = CreateWindowW(L"EDIT", L"Key...",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        340, 130, 100, 20, hWnd, (HMENU)BIND_MACRO_ID_KEY_BINDING_EDIT, hInst, nullptr);
    
    state->hAddTriggerButton = CreateWindowW(L"BUTTON", L"+ Add",
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        450, 130, 80, 20, hWnd, (HMENU)BIND_MACRO_ID_ADD_TRIGGER_BUTTON, hInst, nullptr);
    
    state->hTriggerList = CreateWindowW(L"LISTBOX", L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY,
        20, 160, 250, 80, hWnd, (HMENU)BIND_MACRO_ID_TRIGGER_LIST, hInst, nullptr);
    
    state->hCooldownEdit = CreateWindowW(L"EDIT", L"100",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
        280, 160, 60, 20, hWnd, (HMENU)BIND_MACRO_ID_COOLDOWN_EDIT, hInst, nullptr);
    
    state->hRemoveTriggerButton = CreateWindowW(L"BUTTON", L"🗑️",
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        350, 160, 40, 20, hWnd, (HMENU)BIND_MACRO_ID_REMOVE_TRIGGER_BUTTON, hInst, nullptr);
    
    // Conditional group
    state->hConditionalGroupBox = CreateWindowW(L"BUTTON", L"Conditions",
        WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
        10, 250, 580, 100, hWnd, nullptr, hInst, nullptr);
    
    state->hConditionTypeCombo = CreateWindowW(L"COMBOBOX", L"",
        WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
        20, 270, 150, 200, hWnd, (HMENU)BIND_MACRO_ID_CONDITION_TYPE_COMBO, hInst, nullptr);
    
    state->hConditionValueEdit = CreateWindowW(L"EDIT", L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        180, 270, 100, 20, hWnd, (HMENU)BIND_MACRO_ID_CONDITION_VALUE_EDIT, hInst, nullptr);
    
    state->hAddConditionButton = CreateWindowW(L"BUTTON", L"+ Add",
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        290, 270, 80, 20, hWnd, (HMENU)BIND_MACRO_ID_ADD_CONDITION_BUTTON, hInst, nullptr);
    
    state->hConditionList = CreateWindowW(L"LISTBOX", L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY,
        20, 295, 350, 40, hWnd, (HMENU)BIND_MACRO_ID_CONDITION_LIST, hInst, nullptr);
    
    state->hRequireAllRadio = CreateWindowW(L"BUTTON", L"All conditions (AND)",
        WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
        380, 295, 180, 20, hWnd, (HMENU)BIND_MACRO_ID_REQUIRE_ALL_RADIO, hInst, nullptr);
    
    state->hRequireAnyRadio = CreateWindowW(L"BUTTON", L"Any condition (OR)",
        WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
        380, 320, 180, 20, hWnd, (HMENU)BIND_MACRO_ID_REQUIRE_ANY_RADIO, hInst, nullptr);
    
    // Settings
    state->hLoopCheckbox = CreateWindowW(L"BUTTON", L"Loop playback",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        20, 360, 120, 20, hWnd, (HMENU)BIND_MACRO_ID_LOOP_CHECKBOX, hInst, nullptr);
    
    state->hBlockKeysCheckbox = CreateWindowW(L"BUTTON", L"Block keys",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        150, 360, 120, 20, hWnd, (HMENU)BIND_MACRO_ID_BLOCK_KEYS_CHECKBOX, hInst, nullptr);
    
    state->hSpeedSlider = CreateWindowW(L"TRACKBAR", L"",
        WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
        280, 360, 150, 20, hWnd, (HMENU)BIND_MACRO_ID_SPEED_SLIDER, hInst, nullptr);
    
    state->hSpeedLabel = CreateWindowW(L"STATIC", L"Speed: 1.0x",
        WS_VISIBLE | WS_CHILD,
        440, 360, 80, 20, hWnd, (HMENU)BIND_MACRO_ID_SPEED_LABEL, hInst, nullptr);
    
    state->hEnableDirectBindingCheckbox = CreateWindowW(L"BUTTON", L"Enable direct binding",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        20, 385, 150, 20, hWnd, (HMENU)BIND_MACRO_ID_ENABLE_DIRECT_BINDING_CHECKBOX, hInst, nullptr);
    
    // Status
    state->hStatusLabel = CreateWindowW(L"STATIC", L"Status: Ready",
        WS_VISIBLE | WS_CHILD,
        20, 415, 200, 20, hWnd, (HMENU)BIND_MACRO_ID_STATUS_LABEL, hInst, nullptr);
    
    state->hRecordingTimeLabel = CreateWindowW(L"STATIC", L"Time: 0s",
        WS_VISIBLE | WS_CHILD,
        230, 415, 100, 20, hWnd, (HMENU)BIND_MACRO_ID_RECORDING_TIME_LABEL, hInst, nullptr);
    
    state->hExecutionCountLabel = CreateWindowW(L"STATIC", L"Executions: 0",
        WS_VISIBLE | WS_CHILD,
        340, 415, 100, 20, hWnd, (HMENU)BIND_MACRO_ID_EXECUTION_COUNT_LABEL, hInst, nullptr);
    
    // Actions list
    state->hActionsList = CreateWindowW(L"LISTBOX", L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY,
        20, 445, 400, 100, hWnd, (HMENU)BIND_MACRO_ID_ACTIONS_LIST, hInst, nullptr);
    
    state->hAddActionButton = CreateWindowW(L"BUTTON", L"+ Add",
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        430, 445, 80, 25, hWnd, (HMENU)BIND_MACRO_ID_ADD_ACTION_BUTTON, hInst, nullptr);
    
    state->hEditActionButton = CreateWindowW(L"BUTTON", L"✏️ Edit",
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        515, 445, 80, 25, hWnd, (HMENU)BIND_MACRO_ID_EDIT_ACTION_BUTTON, hInst, nullptr);
    
    state->hRemoveActionButton = CreateWindowW(L"BUTTON", L"🗑️ Delete",
        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        430, 475, 80, 25, hWnd, (HMENU)BIND_MACRO_ID_REMOVE_ACTION_BUTTON, hInst, nullptr);
    
    // Apply dark theme to all controls
    ApplyDarkTheme(state->hMacroList);
    ApplyDarkTheme(state->hMacroNameEdit);
    ApplyDarkTheme(state->hCreateButton);
    ApplyDarkTheme(state->hDeleteButton);
    ApplyDarkTheme(state->hRecordButton);
    ApplyDarkTheme(state->hPlayButton);
    ApplyDarkTheme(state->hStopButton);
    ApplyDarkTheme(state->hBindingGroupBox);
    ApplyDarkTheme(state->hMouseLeftDownCheck);
    ApplyDarkTheme(state->hMouseLeftUpCheck);
    ApplyDarkTheme(state->hMouseRightDownCheck);
    ApplyDarkTheme(state->hMouseRightUpCheck);
    ApplyDarkTheme(state->hMouseLeftPressedCheck);
    ApplyDarkTheme(state->hMouseRightPressedCheck);
    ApplyDarkTheme(state->hKeyBindingEdit);
    ApplyDarkTheme(state->hAddTriggerButton);
    ApplyDarkTheme(state->hTriggerList);
    ApplyDarkTheme(state->hCooldownEdit);
    ApplyDarkTheme(state->hRemoveTriggerButton);
    ApplyDarkTheme(state->hConditionalGroupBox);
    ApplyDarkTheme(state->hConditionTypeCombo);
    ApplyDarkTheme(state->hConditionValueEdit);
    ApplyDarkTheme(state->hAddConditionButton);
    ApplyDarkTheme(state->hConditionList);
    ApplyDarkTheme(state->hRequireAllRadio);
    ApplyDarkTheme(state->hRequireAnyRadio);
    ApplyDarkTheme(state->hLoopCheckbox);
    ApplyDarkTheme(state->hBlockKeysCheckbox);
    ApplyDarkTheme(state->hSpeedSlider);
    ApplyDarkTheme(state->hSpeedLabel);
    ApplyDarkTheme(state->hEnableDirectBindingCheckbox);
    ApplyDarkTheme(state->hStatusLabel);
    ApplyDarkTheme(state->hRecordingTimeLabel);
    ApplyDarkTheme(state->hExecutionCountLabel);
    ApplyDarkTheme(state->hActionsList);
    ApplyDarkTheme(state->hAddActionButton);
    ApplyDarkTheme(state->hEditActionButton);
    ApplyDarkTheme(state->hRemoveActionButton);
    
    // Initialize combo box
    SendMessageW(state->hConditionTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"Key pressed");
    SendMessageW(state->hConditionTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"Mouse button pressed");
    SendMessageW(state->hConditionTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"Variable = value");
    SendMessageW(state->hConditionTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"Variable > value");
    SendMessageW(state->hConditionTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"Variable < value");
    SendMessageW(state->hConditionTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"Random chance");
    SendMessageW(state->hConditionTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"Time elapsed");
    SendMessageW(state->hConditionTypeCombo, CB_SETCURSEL, 0, 0);
    
    // Initialize slider
    SendMessageW(state->hSpeedSlider, TBM_SETRANGE, TRUE, MAKELONG(1, 50));
    SendMessageW(state->hSpeedSlider, TBM_SETPOS, TRUE, 10);
    
    // Set default radio button
    SendMessageW(state->hRequireAllRadio, BM_SETCHECK, BST_CHECKED, 0);
    
    // Initialize BindMacroSystem
    BindMacroSystem::Initialize();
    
    // Refresh UI
    BindMacroSubpage_UpdateMacroList(hWnd);
    
    return 0;
}

// Window Procedure
LRESULT CALLBACK BindMacroSubpageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    BindMacroUIState* state = (BindMacroUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    switch (msg)
    {
    case WM_CREATE:
        return BindMacroSubpage_OnCreate(hWnd, lParam);
        
    case WM_DRAWITEM:
        if (wParam) {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
            if (dis->CtlType == ODT_BUTTON) {
                switch (dis->CtlID) {
                case BIND_MACRO_ID_CREATE_BUTTON:
                case BIND_MACRO_ID_DELETE_BUTTON:
                case BIND_MACRO_ID_RECORD_BUTTON:
                case BIND_MACRO_ID_PLAY_BUTTON:
                case BIND_MACRO_ID_STOP_BUTTON:
                case BIND_MACRO_ID_ADD_TRIGGER_BUTTON:
                case BIND_MACRO_ID_REMOVE_TRIGGER_BUTTON:
                case BIND_MACRO_ID_ADD_CONDITION_BUTTON:
                case BIND_MACRO_ID_ADD_ACTION_BUTTON:
                case BIND_MACRO_ID_EDIT_ACTION_BUTTON:
                case BIND_MACRO_ID_REMOVE_ACTION_BUTTON:
                    DrawBindMacroButton(dis);
                    return TRUE;
                }
            }
        }
        break;
        
    case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, UiTheme::Color_Text());
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)UiTheme::Brush_ControlBg();
        }
        break;
        
    case WM_COMMAND:
        if (state && HIWORD(wParam) == LBN_SELCHANGE)
        {
            switch (LOWORD(wParam))
            {
            case BIND_MACRO_ID_LIST:
                state->selectedMacroId = (int)SendMessageW(state->hMacroList, LB_GETCURSEL, 0, 0);
                BindMacroSubpage_RefreshUI(hWnd);
                break;
            case BIND_MACRO_ID_TRIGGER_LIST:
                state->selectedTriggerId = (int)SendMessageW(state->hTriggerList, LB_GETCURSEL, 0, 0);
                break;
            case BIND_MACRO_ID_CONDITION_LIST:
                state->selectedConditionId = (int)SendMessageW(state->hConditionList, LB_GETCURSEL, 0, 0);
                break;
            case BIND_MACRO_ID_ACTIONS_LIST:
                state->selectedActionId = (int)SendMessageW(state->hActionsList, LB_GETCURSEL, 0, 0);
                break;
            }
        }
        else if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case BIND_MACRO_ID_CREATE_BUTTON:
                {
                    wchar_t name[256];
                    GetWindowTextW(state->hMacroNameEdit, name, 256);
                    if (wcslen(name) > 0)
                    {
                        int macroId = BindMacroSystem::CreateMacro(name);
                        BindMacroSubpage_UpdateMacroList(hWnd);
                        SendMessageW(state->hMacroList, LB_SETCURSEL, macroId, 0);
                        state->selectedMacroId = macroId;
                        BindMacroSubpage_RefreshUI(hWnd);
                        SetWindowTextW(state->hMacroNameEdit, L"");
                    }
                }
                break;
                
            case BIND_MACRO_ID_DELETE_BUTTON:
                if (state->selectedMacroId >= 0)
                {
                    BindMacroSystem::DeleteMacro(state->selectedMacroId);
                    BindMacroSubpage_UpdateMacroList(hWnd);
                    state->selectedMacroId = -1;
                    BindMacroSubpage_RefreshUI(hWnd);
                }
                break;
                
            case BIND_MACRO_ID_RECORD_BUTTON:
                if (state->selectedMacroId >= 0)
                {
                    if (!state->isRecording)
                    {
                        BindMacroSystem::StartRecording(state->selectedMacroId);
                        state->isRecording = true;
                        SetWindowTextW(state->hRecordButton, L"⏹️ Stop");
                        BindMacroSubpage_UpdateRecordingStatus(hWnd);
                    }
                    else
                    {
                        BindMacroSystem::StopRecording();
                        state->isRecording = false;
                        SetWindowTextW(state->hRecordButton, L"🔴 Record");
                        BindMacroSubpage_UpdateRecordingStatus(hWnd);
                        BindMacroSubpage_UpdateActionsList(hWnd);
                    }
                }
                break;
                
            case BIND_MACRO_ID_PLAY_BUTTON:
                if (state->selectedMacroId >= 0)
                {
                    BindMacroSystem::StartPlayback(state->selectedMacroId);
                    state->isPlaying = true;
                    BindMacroSubpage_UpdatePlaybackStatus(hWnd);
                }
                break;
                
            case BIND_MACRO_ID_STOP_BUTTON:
                BindMacroSystem::StopPlayback();
                state->isPlaying = false;
                BindMacroSubpage_UpdatePlaybackStatus(hWnd);
                break;
                
            case BIND_MACRO_ID_ADD_TRIGGER_BUTTON:
                if (state->selectedMacroId >= 0)
                {
                    // Add trigger based on checked boxes
                    if (SendMessageW(state->hMouseLeftDownCheck, BM_GETCHECK, 0, 0) == BST_CHECKED)
                    {
                        BindMacroSystem::AddTrigger(state->selectedMacroId, BindTriggerType::MouseLeftDown);
                    }
                    if (SendMessageW(state->hMouseRightDownCheck, BM_GETCHECK, 0, 0) == BST_CHECKED)
                    {
                        BindMacroSystem::AddTrigger(state->selectedMacroId, BindTriggerType::MouseRightDown);
                    }
                    // Add more trigger types as needed...
                    BindMacroSubpage_UpdateTriggerList(hWnd);
                }
                break;
                
            case BIND_MACRO_ID_REMOVE_TRIGGER_BUTTON:
                if (state->selectedMacroId >= 0 && state->selectedTriggerId >= 0)
                {
                    // Remove selected trigger
                    BindMacroSubpage_UpdateTriggerList(hWnd);
                }
                break;
                
            case BIND_MACRO_ID_ADD_CONDITION_BUTTON:
                if (state->selectedMacroId >= 0)
                {
                    // Add condition based on combo box selection
                    int conditionType = (int)SendMessageW(state->hConditionTypeCombo, CB_GETCURSEL, 0, 0);
                    // Add condition implementation...
                    BindMacroSubpage_UpdateConditionList(hWnd);
                }
                break;
                
            case BIND_MACRO_ID_ENABLE_DIRECT_BINDING_CHECKBOX:
                {
                    bool enabled = SendMessageW((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    BindMacroSystem::EnableDirectBinding(enabled);
                }
                break;
            }
        }
        else if (HIWORD(wParam) == TRBN_THUMBPOSITION || HIWORD(wParam) == TRBN_THUMBTRACK)
        {
            if (LOWORD(wParam) == BIND_MACRO_ID_SPEED_SLIDER)
            {
                int pos = (int)SendMessageW(state->hSpeedSlider, TBM_GETPOS, 0, 0);
                float speed = pos / 10.0f;
                BindMacroSystem::SetPlaybackSpeed(speed);
                
                wchar_t speedText[64];
                swprintf_s(speedText, L"Speed: %.1fx", speed);
                SetWindowTextW(state->hSpeedLabel, speedText);
            }
        }
        break;
        
    case WM_TIMER:
        if (wParam == 1) // Update timer
        {
            if (state->isRecording)
            {
                BindMacroSubpage_UpdateRecordingStatus(hWnd);
            }
            if (state->isPlaying)
            {
                BindMacroSubpage_UpdatePlaybackStatus(hWnd);
            }
            BindMacroSubpage_UpdateStatistics(hWnd);
        }
        break;
        
    case WM_DESTROY:
        if (state)
        {
            delete state;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;
    }
    
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// UI Update Functions
void BindMacroSubpage_RefreshUI(HWND hWnd)
{
    BindMacroUIState* state = (BindMacroUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state) return;
    
    if (state->selectedMacroId >= 0)
    {
        BindMacro* macro = BindMacroSystem::GetMacro(state->selectedMacroId);
        if (macro)
        {
            SetWindowTextW(state->hMacroNameEdit, macro->name.c_str());
            SendMessageW(state->hLoopCheckbox, BM_SETCHECK, macro->isLooping ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessageW(state->hBlockKeysCheckbox, BM_SETCHECK, macro->blockKeysDuringPlayback ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessageW(state->hEnableDirectBindingCheckbox, BM_SETCHECK, macro->isDirectBinding ? BST_CHECKED : BST_UNCHECKED, 0);
            
            int speedPos = (int)(macro->playbackSpeed * 10);
            SendMessageW(state->hSpeedSlider, TBM_SETPOS, TRUE, speedPos);
            
            BindMacroSubpage_UpdateTriggerList(hWnd);
            BindMacroSubpage_UpdateConditionList(hWnd);
            BindMacroSubpage_UpdateActionsList(hWnd);
            BindMacroSubpage_UpdateStatistics(hWnd);
        }
    }
    else
    {
        SetWindowTextW(state->hMacroNameEdit, L"");
        SendMessageW(state->hLoopCheckbox, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessageW(state->hBlockKeysCheckbox, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessageW(state->hEnableDirectBindingCheckbox, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessageW(state->hSpeedSlider, TBM_SETPOS, TRUE, 10);
        
        SendMessageW(state->hTriggerList, LB_RESETCONTENT, 0, 0);
        SendMessageW(state->hConditionList, LB_RESETCONTENT, 0, 0);
        SendMessageW(state->hActionsList, LB_RESETCONTENT, 0, 0);
    }
}

void BindMacroSubpage_UpdateMacroList(HWND hWnd)
{
    BindMacroUIState* state = (BindMacroUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state) return;
    
    SendMessageW(state->hMacroList, LB_RESETCONTENT, 0, 0);
    
    std::vector<int> macroIds = BindMacroSystem::GetAllMacroIds();
    for (int id : macroIds)
    {
        BindMacro* macro = BindMacroSystem::GetMacro(id);
        if (macro)
        {
            std::wstring display = macro->name + (macro->isDirectBinding ? L" [BIND]" : L"");
            SendMessageW(state->hMacroList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
        }
    }
}

void BindMacroSubpage_UpdateTriggerList(HWND hWnd)
{
    BindMacroUIState* state = (BindMacroUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state || state->selectedMacroId < 0) return;
    
    SendMessageW(state->hTriggerList, LB_RESETCONTENT, 0, 0);
    
    BindMacro* macro = BindMacroSystem::GetMacro(state->selectedMacroId);
    if (macro)
    {
        for (const auto& trigger : macro->triggers)
        {
            std::wstring triggerText;
            switch (trigger.type)
            {
            case BindTriggerType::MouseLeftDown:
                triggerText = L"Left click ↓";
                break;
            case BindTriggerType::MouseLeftUp:
                triggerText = L"Left click ↑";
                break;
            case BindTriggerType::MouseRightDown:
                triggerText = L"Right click ↓";
                break;
            case BindTriggerType::MouseRightUp:
                triggerText = L"Right click ↑";
                break;
            case BindTriggerType::MouseLeftPressed:
                triggerText = L"Left click (held)";
                break;
            case BindTriggerType::MouseRightPressed:
                triggerText = L"Right click (held)";
                break;
            default:
                triggerText = L"Unknown trigger";
                break;
            }
            
            triggerText += L" (CD: " + std::to_wstring(trigger.cooldownMs) + L"ms)";
            SendMessageW(state->hTriggerList, LB_ADDSTRING, 0, (LPARAM)triggerText.c_str());
        }
    }
}

void BindMacroSubpage_UpdateConditionList(HWND hWnd)
{
    BindMacroUIState* state = (BindMacroUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state || state->selectedMacroId < 0) return;
    
    SendMessageW(state->hConditionList, LB_RESETCONTENT, 0, 0);
    
    BindMacro* macro = BindMacroSystem::GetMacro(state->selectedMacroId);
    if (macro)
    {
        for (const auto& condition : macro->conditions)
        {
            std::wstring conditionText;
            // Add condition text based on type
            SendMessageW(state->hConditionList, LB_ADDSTRING, 0, (LPARAM)conditionText.c_str());
        }
    }
}

void BindMacroSubpage_UpdateActionsList(HWND hWnd)
{
    BindMacroUIState* state = (BindMacroUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state || state->selectedMacroId < 0) return;
    
    SendMessageW(state->hActionsList, LB_RESETCONTENT, 0, 0);
    
    BindMacro* macro = BindMacroSystem::GetMacro(state->selectedMacroId);
    if (macro)
    {
        for (const auto& action : macro->actions)
        {
            std::wstring actionText;
            switch (action.type)
            {
            case BindMacroActionType::KeyPress:
                actionText = L"Key ↓: " + std::to_wstring(action.hid);
                break;
            case BindMacroActionType::KeyRelease:
                actionText = L"Key ↑: " + std::to_wstring(action.hid);
                break;
            case BindMacroActionType::MouseLeftDown:
                actionText = L"Left click ↓";
                break;
            case BindMacroActionType::MouseLeftUp:
                actionText = L"Left click ↑";
                break;
            case BindMacroActionType::MouseRightDown:
                actionText = L"Right click ↓";
                break;
            case BindMacroActionType::MouseRightUp:
                actionText = L"Right click ↑";
                break;
            case BindMacroActionType::Delay:
                actionText = L"Delay: " + std::to_wstring(action.delayMs) + L"ms";
                break;
            default:
                actionText = L"Unknown action";
                break;
            }
            
            actionText += L" (+" + std::to_wstring(action.timestamp) + L"ms)";
            SendMessageW(state->hActionsList, LB_ADDSTRING, 0, (LPARAM)actionText.c_str());
        }
    }
}

void BindMacroSubpage_UpdateRecordingStatus(HWND hWnd)
{
    BindMacroUIState* state = (BindMacroUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state) return;
    
    if (state->isRecording)
    {
        uint32_t duration = BindMacroSystem::GetRecordingDuration();
        SetWindowTextW(state->hStatusLabel, L"Status: Recording...");
        SetWindowTextW(state->hRecordingTimeLabel, (L"Time: " + std::to_wstring(duration / 1000) + L"s").c_str());
    }
    else
    {
        SetWindowTextW(state->hStatusLabel, L"Status: Ready");
        SetWindowTextW(state->hRecordingTimeLabel, L"Time: 0s");
    }
}

void BindMacroSubpage_UpdatePlaybackStatus(HWND hWnd)
{
    BindMacroUIState* state = (BindMacroUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state) return;
    
    if (state->isPlaying)
    {
        SetWindowTextW(state->hStatusLabel, L"Status: Playing...");
    }
    else
    {
        SetWindowTextW(state->hStatusLabel, L"Status: Ready");
    }
}

void BindMacroSubpage_UpdateStatistics(HWND hWnd)
{
    BindMacroUIState* state = (BindMacroUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state || state->selectedMacroId < 0) return;
    
    BindMacro* macro = BindMacroSystem::GetMacro(state->selectedMacroId);
    if (macro)
    {
        SetWindowTextW(state->hExecutionCountLabel, (L"Executions: " + std::to_wstring(macro->executionCount)).c_str());
    }
    else
    {
        SetWindowTextW(state->hExecutionCountLabel, L"Executions: 0");
    }
}

// Creation function
HWND BindMacroSubpage_Create(HWND hParent, HINSTANCE hInst)
{
    WNDCLASSW wc{};
    wc.lpfnWndProc = BindMacroSubpageProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"BindMacroSubpage";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = UiTheme::Brush_ControlBg();
    
    if (!GetClassInfoExW(hInst, L"BindMacroSubpage", nullptr))
    {
        RegisterClassW(&wc);
    }
    
    return CreateWindowW(L"BindMacroSubpage", L"", 
        WS_VISIBLE | WS_CHILD,
        0, 0, 600, 550, hParent, nullptr, hInst, nullptr);
}
