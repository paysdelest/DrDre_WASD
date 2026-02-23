// mouse_combo_ui.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <string>

#include "mouse_combo_ui.h"
#include "mouse_combo_system.h"
#include "settings.h"

#pragma comment(lib, "Comctl32.lib")

// Macros de contrôles Windows
#ifndef ListBox_ResetContent
#define ListBox_ResetContent(hwnd)      SendMessage((hwnd), LB_RESETCONTENT, 0, 0)
#define ListBox_AddString(hwnd, str)   SendMessage((hwnd), LB_ADDSTRING, 0, (LPARAM)(str))
#define ListBox_GetCurSel(hwnd)        (int)SendMessage((hwnd), LB_GETCURSEL, 0, 0)
#define ListBox_GetText(hwnd, index, buf, len) SendMessage((hwnd), LB_GETTEXT, (WPARAM)(index), (LPARAM)(buf))
#endif

#ifndef ComboBox_AddString
#define ComboBox_AddString(hwnd, str)  SendMessage((hwnd), CB_ADDSTRING, 0, (LPARAM)(str))
#define ComboBox_SetCurSel(hwnd, index) SendMessage((hwnd), CB_SETCURSEL, (WPARAM)(index), 0)
#define ComboBox_GetCurSel(hwnd)       (int)SendMessage((hwnd), CB_GETCURSEL, 0, 0)
#endif

#ifndef Button_SetCheck
#define Button_SetCheck(hwnd, state)   SendMessage((hwnd), BM_SETCHECK, (WPARAM)(state), 0)
#define Button_GetCheck(hwnd)          (int)SendMessage((hwnd), BM_GETCHECK, 0, 0)
#endif

static HWND g_hComboList = nullptr;
static HWND g_hComboName = nullptr;
static HWND g_hComboTrigger = nullptr;
static HWND g_hActionList = nullptr;
static HWND g_hActionKey = nullptr;
static HWND g_hActionType = nullptr;
static HWND g_hRepeatCheck = nullptr;
static HWND g_hRepeatDelay = nullptr;
static HWND g_hRepeatLocalCheck = nullptr;
static int g_currentComboId = -1;

// Noms des types de triggers pour l'UI
static const wchar_t* TRIGGER_NAMES[] = {
    L"Clic droit maintenu + Clic gauche",
    L"Clic gauche maintenu + Clic droit", 
    L"Clic milieu maintenu + Clic gauche",
    L"Clic milieu maintenu + Clic droit",
    L"Double clic gauche",
    L"Double clic droit",
    L"Triple clic gauche",
    L"Clic gauche ET droit en même temps",
    L"Molette haut avec clic droit maintenu",
    L"Molette bas avec clic droit maintenu"
};

// Noms des types d'actions pour l'UI
static const wchar_t* ACTION_TYPE_NAMES[] = {
    L"Appuyer touche",
    L"Relâcher touche", 
    L"Appuyer+Relâcher touche",
    L"Taper texte",
    L"Clic souris",
    L"Attendre (ms)"
};

static void RefreshActionList()
{
    if (!g_hActionList || g_currentComboId < 0)
        return;
        
    MouseCombo* combo = MouseComboSystem::GetCombo(g_currentComboId);
    if (!combo)
        return;
        
    ListBox_ResetContent(g_hActionList);
    
    for (size_t i = 0; i < combo->actions.size(); ++i)
    {
        const auto& action = combo->actions[i];
        std::wstring text;
        
        switch (action.type)
        {
        case ComboActionType::PressKey:
            text = L"Appuyer: " + std::to_wstring(action.keyHid);
            break;
        case ComboActionType::ReleaseKey:
            text = L"Relâcher: " + std::to_wstring(action.keyHid);
            break;
        case ComboActionType::TapKey:
            text = L"Tap: " + std::to_wstring(action.keyHid);
            break;
        case ComboActionType::TypeText:
            text = L"Texte: " + action.text;
            break;
        case ComboActionType::MouseClick:
            text = L"Clic: " + std::to_wstring(action.mouseButton);
            break;
        case ComboActionType::Delay:
            text = L"Attendre: " + std::to_wstring(action.delayMs) + L"ms";
            break;
        default:
            text = L"Inconnu";
            break;
        }
        
        ListBox_AddString(g_hActionList, text.c_str());
    }
}

static void UpdateComboControls()
{
    if (g_currentComboId < 0)
        return;
        
    MouseCombo* combo = MouseComboSystem::GetCombo(g_currentComboId);
    if (!combo)
        return;
        
    // Mettre à jour les contrôles avec les données du combo
    SetWindowTextW(g_hComboName, combo->name.c_str());
    ComboBox_SetCurSel(g_hComboTrigger, (int)combo->trigger);
    Button_SetCheck(g_hRepeatCheck, combo->repeatWhileHeld ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(g_hRepeatLocalCheck, combo->useLocalRepeat ? BST_CHECKED : BST_UNCHECKED);
    
    wchar_t delayStr[32];
    _itow_s(combo->repeatDelayMs, delayStr, 32, 10);
    SetWindowTextW(g_hRepeatDelay, delayStr);
    
    RefreshActionList();
}

HWND MouseComboUI::CreateComboPage(HWND parent, HINSTANCE hInst)
{
    // Create main page
    HWND hPage = CreateWindowExW(
        0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_NOTIFY,
        0, 0, 0, 0, parent, nullptr, hInst, nullptr
    );
    
    // Combo list
    g_hComboList = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | LBS_STANDARD | WS_VSCROLL,
        10, 10, 200, 200,
        hPage, (HMENU)IDC_COMBO_LIST, hInst, nullptr
    );
    
    // Combo name
    CreateWindowExW(0, L"STATIC", L"Name:", WS_CHILD | WS_VISIBLE,
        220, 10, 100, 20, hPage, nullptr, hInst, nullptr);
    g_hComboName = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        220, 30, 200, 20, hPage, (HMENU)IDC_COMBO_NAME, hInst, nullptr);
    
    // Trigger type
    CreateWindowExW(0, L"STATIC", L"Trigger:", WS_CHILD | WS_VISIBLE,
        220, 60, 100, 20, hPage, nullptr, hInst, nullptr);
    g_hComboTrigger = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        220, 80, 200, 200, hPage, (HMENU)IDC_COMBO_TRIGGER, hInst, nullptr);
    
    // Fill trigger list
    for (int i = 0; i < 10; ++i)
    {
        ComboBox_AddString(g_hComboTrigger, TRIGGER_NAMES[i]);
    }
    
    // Actions list
    CreateWindowExW(0, L"STATIC", L"Actions:", WS_CHILD | WS_VISIBLE,
        10, 220, 100, 20, hPage, nullptr, hInst, nullptr);
    g_hActionList = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | LBS_STANDARD | WS_VSCROLL,
        10, 240, 300, 100,
        hPage, (HMENU)IDC_ACTION_LIST, hInst, nullptr
    );
    
    // Action type
    CreateWindowExW(0, L"STATIC", L"Type:", WS_CHILD | WS_VISIBLE,
        320, 240, 50, 20, hPage, nullptr, hInst, nullptr);
    g_hActionType = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        320, 260, 150, 200, hPage, (HMENU)IDC_ACTION_TYPE, hInst, nullptr);
    
    // Fill action type list
    for (int i = 0; i < 6; ++i)
    {
        ComboBox_AddString(g_hActionType, ACTION_TYPE_NAMES[i]);
    }
    
    // Key for action
    CreateWindowExW(0, L"STATIC", L"Key:", WS_CHILD | WS_VISIBLE,
        320, 290, 50, 20, hPage, nullptr, hInst, nullptr);
    g_hActionKey = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"P",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        320, 310, 50, 20, hPage, (HMENU)IDC_ACTION_KEY, hInst, nullptr);
    
    // Buttons
    CreateWindowExW(0, L"BUTTON", L"Add action", WS_CHILD | WS_VISIBLE,
        380, 310, 100, 20, hPage, (HMENU)IDC_ADD_ACTION, hInst, nullptr);
    
    // Repetition
    g_hRepeatCheck = CreateWindowExW(0, L"BUTTON", L"Repeat", 
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        10, 350, 80, 20, hPage, (HMENU)IDC_REPEAT_CHECK, hInst, nullptr);
    
    CreateWindowExW(0, L"STATIC", L"Delay (ms):", WS_CHILD | WS_VISIBLE,
        100, 350, 60, 20, hPage, nullptr, hInst, nullptr);
    g_hRepeatDelay = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"100",
        WS_CHILD | WS_VISIBLE | ES_NUMBER,
        170, 350, 50, 20, hPage, (HMENU)IDC_REPEAT_DELAY, hInst, nullptr);
    
    // Checkbox to allow per-combo override of global throttle
    g_hRepeatLocalCheck = CreateWindowExW(0, L"BUTTON", L"Use custom interval",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        230, 350, 170, 20, hPage, (HMENU)IDC_REPEAT_LOCAL, hInst, nullptr);
    
    // Save buttons
    CreateWindowExW(0, L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE,
        10, 380, 80, 25, hPage, (HMENU)IDC_SAVE_COMBO, hInst, nullptr);
    
    CreateWindowExW(0, L"BUTTON", L"New combo", WS_CHILD | WS_VISIBLE,
        100, 380, 80, 25, hPage, (HMENU)IDC_ADD_ACTION, hInst, nullptr);
    
    CreateWindowExW(0, L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE,
        190, 380, 80, 25, hPage, (HMENU)IDC_DELETE_COMBO_BUTTON, hInst, nullptr);
    
    RefreshComboList(hPage);
    
    return hPage;
}

void MouseComboUI::RefreshComboList(HWND hComboPage)
{
    if (!g_hComboList)
        return;
        
    ListBox_ResetContent(g_hComboList);
    
    std::vector<int> comboIds = MouseComboSystem::GetAllComboIds();
    for (int id : comboIds)
    {
        MouseCombo* combo = MouseComboSystem::GetCombo(id);
        if (combo)
        {
            std::wstring displayName = combo->name + 
                (combo->enabled ? L"" : L" (disabled)");
            ListBox_AddString(g_hComboList, displayName.c_str());
        }
    }
}

void MouseComboUI::UpdateComboUI(HWND hComboPage, int comboId)
{
    g_currentComboId = comboId;
    UpdateComboControls();
}

void MouseComboUI::OnAddCombo(HWND hComboPage)
{
    int newId = MouseComboSystem::CreateCombo(
        L"New combo", 
        ComboTriggerType::RightClickHeld_LeftClick
    );
    
    // Add default action (key P)
    MouseComboSystem::AddAction(newId, 
        MouseComboSystem::MakeTapKeyAction(VkToHid('P'))
    );
    
    // Enable repetition
    MouseComboSystem::SetComboRepeat(newId, true, Settings_GetComboRepeatThrottleMs());
    
    RefreshComboList(hComboPage);
    UpdateComboUI(hComboPage, newId);
}

void MouseComboUI::OnSaveCombo(HWND hComboPage)
{
    if (g_currentComboId < 0)
        return;
        
    MouseCombo* combo = MouseComboSystem::GetCombo(g_currentComboId);
    if (!combo)
        return;
        
    // Mettre à jour le nom
    wchar_t nameBuffer[256];
    GetWindowTextW(g_hComboName, nameBuffer, 256);
    combo->name = nameBuffer;
    
    // Mettre à jour le trigger
    int triggerIndex = ComboBox_GetCurSel(g_hComboTrigger);
    if (triggerIndex >= 0)
        combo->trigger = (ComboTriggerType)triggerIndex;
    
    // Mettre à jour la répétition
    combo->repeatWhileHeld = (Button_GetCheck(g_hRepeatCheck) == BST_CHECKED);
    // Local override
    combo->useLocalRepeat = (Button_GetCheck(g_hRepeatLocalCheck) == BST_CHECKED);
    
    wchar_t delayBuffer[32];
    GetWindowTextW(g_hRepeatDelay, delayBuffer, 32);
    combo->repeatDelayMs = _wtoi(delayBuffer);
    
    RefreshComboList(hComboPage);
}
