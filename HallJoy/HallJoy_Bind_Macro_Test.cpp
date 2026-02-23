#include "halljoy_bind_macro_test.h"
#include "halljoy_bind_macro.h"
#include "ui_theme.h"
#include <commctrl.h>
#include <sstream>

// Control IDs
#define TEST_ID_CREATE_PING_BUTTON 1001
#define TEST_ID_CREATE_COMPOSITE_BUTTON 1002
#define TEST_ID_TOGGLE_BINDING_BUTTON 1003
#define TEST_ID_CLEAR_ALL_BUTTON 1004
#define TEST_ID_STATUS_LABEL 1005
#define TEST_ID_MACRO_LIST 1006

// Window creation function
int HallJoyBindMacroTest_OnCreate(HWND hWnd, LPARAM lParam)
{
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    
    // Create test buttons
    CreateWindowW(L"BUTTON", L"🎯 Créer 'Ping au tir' (clic droit → P)",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        10, 10, 250, 30, hWnd, (HMENU)TEST_ID_CREATE_PING_BUTTON, hInst, nullptr);
    
    CreateWindowW(L"BUTTON", L"🔧 Créer 'Composite' (gauche + droit → P)",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        10, 50, 250, 30, hWnd, (HMENU)TEST_ID_CREATE_COMPOSITE_BUTTON, hInst, nullptr);
    
    CreateWindowW(L"BUTTON", L"⚡ Activer/Désactiver Binding Direct",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        10, 90, 250, 30, hWnd, (HMENU)TEST_ID_TOGGLE_BINDING_BUTTON, hInst, nullptr);
    
    CreateWindowW(L"BUTTON", L"🗑️ Supprimer toutes les macros",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        10, 130, 250, 30, hWnd, (HMENU)TEST_ID_CLEAR_ALL_BUTTON, hInst, nullptr);
    
    // Status label
    CreateWindowW(L"STATIC", L"Statut: Prêt",
        WS_VISIBLE | WS_CHILD,
        10, 170, 250, 20, hWnd, (HMENU)TEST_ID_STATUS_LABEL, hInst, nullptr);
    
    // Macro list
    CreateWindowW(L"LISTBOX", L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | LBS_NOINTEGRALHEIGHT,
        10, 200, 250, 150, hWnd, (HMENU)TEST_ID_MACRO_LIST, hInst, nullptr);
    
    // Apply dark theme
    UiTheme::ApplyToControl(hWnd);
    
    return 0;
}

// Window Procedure
LRESULT CALLBACK HallJoyBindMacroTestProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        return HallJoyBindMacroTest_OnCreate(hWnd, lParam);
        
    case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, UiTheme::Color_Text());
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)UiTheme::Brush_ControlBg();
        }
        break;
        
    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case TEST_ID_CREATE_PING_BUTTON:
                HallJoyBindMacroTest_CreatePingMacro(hWnd);
                break;
            case TEST_ID_CREATE_COMPOSITE_BUTTON:
                HallJoyBindMacroTest_CreateCompositeMacro(hWnd);
                break;
            case TEST_ID_TOGGLE_BINDING_BUTTON:
                HallJoyBindMacroTest_ToggleDirectBinding(hWnd);
                break;
            case TEST_ID_CLEAR_ALL_BUTTON:
                HallJoyBindMacroTest_ClearAllMacros(hWnd);
                break;
            }
        }
        break;
        
    case WM_DESTROY:
        return 0;
    }
    
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// Test Functions
void HallJoyBindMacroTest_CreatePingMacro(HWND hWnd)
{
    // Create "Ping au tir" macro - "Si clic droit → alors appuyer sur P"
    int macroId = HallJoy_Bind_Macro::CreateMacro(L"Ping au tir");
    
    if (macroId >= 0)
    {
        // Add trigger: right mouse button down
        HallJoy_Bind_Macro::AddTrigger(macroId, HallJoyBindTriggerType::MouseRightDown, 0, 100);
        
        // Add action: press "P" key
        HallJoyBindMacroAction action;
        action.type = HallJoyBindMacroActionType::KeyPress;
        action.hid = 0x50; // P key
        action.timestamp = 50;
        
        HallJoyBindMacro* macro = HallJoy_Bind_Macro::GetMacro(macroId);
        if (macro)
        {
            macro->actions.push_back(action);
            
            // Add key release
            action.type = HallJoyBindMacroActionType::KeyRelease;
            action.timestamp = 100;
            macro->actions.push_back(action);
        }
        
        // Update status
        SetWindowTextW(GetDlgItem(hWnd, TEST_ID_STATUS_LABEL), L"✅ Macro 'Ping au tir' créée !");
        
        // Refresh macro list
        HallJoyBindMacroTest_RefreshMacroList(hWnd);
    }
}

void HallJoyBindMacroTest_CreateCompositeMacro(HWND hWnd)
{
    // Create composite macro - "Si clic gauche + clic droit → alors appuyer sur P"
    int macroId = HallJoy_Bind_Macro::CreateMacro(L"Composite Ping");
    
    if (macroId >= 0)
    {
        // Create composite trigger: left + right mouse buttons
        std::vector<HallJoyBindTriggerType> triggers = {
            HallJoyBindTriggerType::MouseLeftDown,
            HallJoyBindTriggerType::MouseRightDown
        };
        HallJoy_Bind_Macro::CreateCompositeTrigger(macroId, triggers);
        
        // Add action: press "P" key
        HallJoyBindMacroAction action;
        action.type = HallJoyBindMacroActionType::KeyPress;
        action.hid = 0x50; // P key
        action.timestamp = 50;
        
        HallJoyBindMacro* macro = HallJoy_Bind_Macro::GetMacro(macroId);
        if (macro)
        {
            macro->actions.push_back(action);
            
            // Add key release
            action.type = HallJoyBindMacroActionType::KeyRelease;
            action.timestamp = 100;
            macro->actions.push_back(action);
        }
        
        // Update status
        SetWindowTextW(GetDlgItem(hWnd, TEST_ID_STATUS_LABEL), L"✅ Macro 'Composite' créée !");
        
        // Refresh macro list
        HallJoyBindMacroTest_RefreshMacroList(hWnd);
    }
}

void HallJoyBindMacroTest_ToggleDirectBinding(HWND hWnd)
{
    bool current = HallJoy_Bind_Macro::IsDirectBindingEnabled();
    HallJoy_Bind_Macro::EnableDirectBinding(!current);
    
    std::wstring status = !current ? L"⚡ Binding Direct ACTIVÉ" : L"⏸️ Binding Direct DÉSACTIVÉ";
    SetWindowTextW(GetDlgItem(hWnd, TEST_ID_STATUS_LABEL), status.c_str());
}

void HallJoyBindMacroTest_ClearAllMacros(HWND hWnd)
{
    // Get all macro IDs and delete them
    std::vector<int> macroIds = HallJoy_Bind_Macro::GetAllMacroIds();
    
    for (int id : macroIds)
    {
        HallJoy_Bind_Macro::DeleteMacro(id);
    }
    
    // Update status
    SetWindowTextW(GetDlgItem(hWnd, TEST_ID_STATUS_LABEL), L"🗑️ Toutes les macros supprimées");
    
    // Refresh macro list
    HallJoyBindMacroTest_RefreshMacroList(hWnd);
}

void HallJoyBindMacroTest_RefreshMacroList(HWND hWnd)
{
    HWND hMacroList = GetDlgItem(hWnd, TEST_ID_MACRO_LIST);
    if (!hMacroList) return;
    
    // Clear list
    SendMessageW(hMacroList, LB_RESETCONTENT, 0, 0);
    
    // Add all macros
    std::vector<int> macroIds = HallJoy_Bind_Macro::GetAllMacroIds();
    
    for (int id : macroIds)
    {
        HallJoyBindMacro* macro = HallJoy_Bind_Macro::GetMacro(id);
        if (macro)
        {
            std::wstring display = macro->name;
            
            // Add trigger info
            if (!macro->triggers.empty())
            {
                display += L" [";
                for (size_t i = 0; i < macro->triggers.size(); i++)
                {
                    if (i > 0) display += L" + ";
                    
                    switch (macro->triggers[i].type)
                    {
                    case HallJoyBindTriggerType::MouseLeftDown:
                        display += L"Gauche↓";
                        break;
                    case HallJoyBindTriggerType::MouseRightDown:
                        display += L"Droit↓";
                        break;
                    default:
                        display += L"Trigger";
                        break;
                    }
                }
                display += L" → P]";
            }
            
            // Add statistics
            uint32_t execCount = HallJoy_Bind_Macro::GetMacroExecutionCount(id);
            display += L" (Exécutions: " + std::to_wstring(execCount) + L")";
            
            SendMessageW(hMacroList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
        }
    }
}

// Creation function
HWND HallJoyBindMacroTest_Create(HWND hParent, HINSTANCE hInst)
{
    WNDCLASSW wc{};
    wc.lpfnWndProc = HallJoyBindMacroTestProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"HallJoyBindMacroTest";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = UiTheme::Brush_ControlBg();
    
    if (!GetClassInfoExW(hInst, L"HallJoyBindMacroTest", nullptr))
    {
        RegisterClassW(&wc);
    }
    
    return CreateWindowW(L"HallJoyBindMacroTest", L"🎮 HallJoy Bind Macro - Test",
        WS_VISIBLE | WS_CHILD | WS_BORDER,
        0, 0, 270, 360, hParent, nullptr, hInst, nullptr);
}
