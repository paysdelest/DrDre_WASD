// macro_advanced_ui.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <string>
#include <vector>
#include <algorithm>

#include "macro_advanced_ui.h"
#include "macro_advanced_system.h"
#include "ui_theme.h"
#include "win_util.h"
#include "premium_combo.h"

#pragma comment(lib, "Comctl32.lib")

static int S(HWND hwnd, int px) { return WinUtil_ScalePx(hwnd, px); }

// ============================================================================
// Help Text
// ============================================================================

const wchar_t* MACRO_HELP_TEXT = 
L"=== COMPOSITE MACRO GUIDE ===\n\n"
L"AutoHotkey-inspired syntax for creating complex macros.\n\n"
L"AVAILABLE COMMANDS:\n\n"
L"PlayMacro, <id>\n"
L"  Play a recorded macro (macro ID)\n\n"
L"Delay, <ms>\n"
L"  Wait for X milliseconds\n\n"
L"Send, <text>\n"
L"  Type text\n\n"
L"Click, <x>, <y>, <button>\n"
L"  Click at position (x,y)\n"
L"  Button: Left, Right, Middle\n\n"
L"MouseMove, <x>, <y>\n"
L"  Move mouse\n\n"
L"KeyPress, <key>\n"
L"  Press a key\n\n"
L"KeyRelease, <key>\n"
L"  Release a key\n\n"
L"KeyTap, <key>\n"
L"Random, <var>, <min>, <max>\n"
L"  Generate a random number\n\n"
L"If, <condition>\n"
L"  Execute next command if true\n"
L"  Example: If, myVar == 5\n\n"
L"Loop, <count>\n"
L"  Repeat commands\n\n"
L"Break\n"
L"  Exit from loop\n\n"
L"<label>:\n"
L"  Define a label\n\n"
L"Goto, <label>\n"
L"  Jump to a label\n\n"
L"; Comment\n"
L"  Comment line\n";

const wchar_t* MACRO_EXAMPLE_SCRIPT = 
L"; Composite macro example\n"
L"; This example shows different features\n\n"
L"; 1. Play a recorded macro\n"
L"PlayMacro, 0\n"
L"Delay, 1000\n\n"
L"; 2. Send text\n"
L"Send, Hello!\n"
L"Delay, 500\n\n"
L"; 3. Click at position\n"
L"Click, 500, 300, Left\n"
L"Delay, 200\n\n"
L"; 4. Use variables\n"
L"SetVar, counter, 0\n"
L"SetVar, maxCount, 5\n\n"
L"; 5. Loop with condition\n"
L"Loop, 5\n"
L"  Send, Iteration\n"
L"  Delay, 100\n\n"
L"; 6. Nombre aléatoire\n"
L"Random, randomDelay, 100, 500\n"
L"Delay, 300\n\n"
L"; 7. Déplacement de souris\n"
L"MouseMove, 100, 100\n"
L"Delay, 100\n"
L"MouseMove, 200, 200\n";

// ============================================================================
// UI Creation
// ============================================================================

HWND MacroAdvancedSubpage_Create(HWND hParent, HINSTANCE hInst)
{
    // Load RichEdit library for script editor
    LoadLibraryW(L"Msftedit.dll");
    
    HWND hWnd = CreateWindowW(L"MacroAdvancedSubpage", L"",
        WS_CHILD | WS_CLIPCHILDREN, 0, 0, 100, 100, hParent, nullptr, hInst, nullptr);
    
    if (!hWnd) return nullptr;
    
    // Create UI state
    MacroAdvancedUIState* state = new MacroAdvancedUIState();
    SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)state);
    
    // Create controls
    int y = S(hWnd, 10);
    int x = S(hWnd, 10);
    int width = S(hWnd, 700);
    int height = S(hWnd, 25);
    int gap = S(hWnd, 8);
    
    // Title
    HWND hTitle = CreateWindowW(L"STATIC", L"Composite Macros (AutoHotkey Style)",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, width, height, hWnd, nullptr, hInst, nullptr);
    UiTheme::ApplyToControl(hTitle);
    
    y += height + gap;
    
    // New Macro button
    HWND hNewBtn = CreateWindowW(L"BUTTON", L"+ New Composite Macro",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, S(hWnd, 200), height, hWnd, (HMENU)(uintptr_t)MACRO_ADV_ID_NEW, hInst, nullptr);
    UiTheme::ApplyToControl(hNewBtn);
    
    // Help button
    HWND hHelpBtn = CreateWindowW(L"BUTTON", L"? Help",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + S(hWnd, 210), y, S(hWnd, 80), height, hWnd, (HMENU)(uintptr_t)MACRO_ADV_ID_HELP, hInst, nullptr);
    UiTheme::ApplyToControl(hHelpBtn);
    
    // Example button
    HWND hExampleBtn = CreateWindowW(L"BUTTON", L"Example",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + S(hWnd, 300), y, S(hWnd, 80), height, hWnd, (HMENU)(uintptr_t)MACRO_ADV_ID_EXAMPLE, hInst, nullptr);
    UiTheme::ApplyToControl(hExampleBtn);
    
    y += height + gap;
    
    // Macro list
    state->hMacroList = PremiumCombo::Create(hWnd, hInst, x, y, width, height, MACRO_ADV_ID_MACRO_LIST);
    PremiumCombo::SetPlaceholderText(state->hMacroList, L"Select a composite macro...");
    UiTheme::ApplyToControl(state->hMacroList);
    
    y += height + gap;
    
    // Control buttons
    int btnWidth = S(hWnd, 100);
    int btnGap = S(hWnd, 8);
    
    state->hExecuteButton = CreateWindowW(L"BUTTON", L"▶ Execute",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, btnWidth, height, hWnd, (HMENU)(uintptr_t)MACRO_ADV_ID_EXECUTE, hInst, nullptr);
    UiTheme::ApplyToControl(state->hExecuteButton);
    
    HWND hDeleteBtn = CreateWindowW(L"BUTTON", L"Delete",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + btnWidth + btnGap, y, btnWidth, height, hWnd, (HMENU)(uintptr_t)MACRO_ADV_ID_DELETE, hInst, nullptr);
    UiTheme::ApplyToControl(hDeleteBtn);
    
    HWND hSaveBtn = CreateWindowW(L"BUTTON", L"💾 Save",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + (btnWidth + btnGap) * 2, y, btnWidth, height, hWnd, (HMENU)(uintptr_t)MACRO_ADV_ID_SAVE_SCRIPT, hInst, nullptr);
    UiTheme::ApplyToControl(hSaveBtn);
    
    HWND hValidateBtn = CreateWindowW(L"BUTTON", L"✓ Validate",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + (btnWidth + btnGap) * 3, y, btnWidth, height, hWnd, (HMENU)(uintptr_t)MACRO_ADV_ID_VALIDATE, hInst, nullptr);
    UiTheme::ApplyToControl(hValidateBtn);
    
    y += height + gap * 2;
    
    // Script editor label
    HWND hEditorLabel = CreateWindowW(L"STATIC", L"Script Editor:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, width, height, hWnd, nullptr, hInst, nullptr);
    UiTheme::ApplyToControl(hEditorLabel);
    
    y += height;
    
    // Script editor (RichEdit control for syntax highlighting potential)
    int editorHeight = S(hWnd, 300);
    state->hScriptEditor = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
        x, y, width, editorHeight,
        hWnd, (HMENU)(uintptr_t)MACRO_ADV_ID_SCRIPT_EDITOR, hInst, nullptr);
    
    // Set monospace font for editor
    HFONT hFont = CreateFontW(
        S(hWnd, 14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    SendMessageW(state->hScriptEditor, WM_SETFONT, (WPARAM)hFont, TRUE);
    UiTheme::ApplyToControl(state->hScriptEditor);
    
    y += editorHeight + gap;
    
    // Status section
    HWND hStatusLabel = CreateWindowW(L"STATIC", L"Status:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, width, height, hWnd, nullptr, hInst, nullptr);
    UiTheme::ApplyToControl(hStatusLabel);
    
    y += height;
    
    state->hStatusText = CreateWindowW(L"STATIC", L"Ready",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, width, height * 2, hWnd, nullptr, hInst, nullptr);
    UiTheme::ApplyToControl(state->hStatusText);
    
    // Start UI refresh timer
    SetTimer(hWnd, MACRO_ADV_UI_TIMER_ID, MACRO_ADV_UI_REFRESH_MS, nullptr);
    
    // Initial UI refresh
    MacroAdvancedSubpage_RefreshUI(hWnd);
    
    return hWnd;
}

void MacroAdvancedSubpage_RefreshUI(HWND hWnd)
{
    MacroAdvancedUIState* state = (MacroAdvancedUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state) return;
    
    // Update macro list
    PremiumCombo::Clear(state->hMacroList);
    
    std::vector<int> macroIds = AdvancedMacroSystem::GetAllCompositeMacroIds();
    for (int id : macroIds)
    {
        CompositeMacro* macro = AdvancedMacroSystem::GetCompositeMacro(id);
        if (macro)
        {
            PremiumCombo::AddString(state->hMacroList, macro->name.c_str());
        }
    }
    
    // Update button states
    bool hasSelection = state->selectedMacroId >= 0 && state->selectedMacroId < (int)macroIds.size();
    EnableWindow(state->hExecuteButton, hasSelection);
    
    // Load script if macro is selected
    if (hasSelection)
    {
        std::wstring script = AdvancedMacroSystem::GenerateScript(state->selectedMacroId);
        SetWindowTextW(state->hScriptEditor, script.c_str());
    }
}

void MacroAdvancedSubpage_UpdateExecutionStatus(HWND hWnd)
{
    MacroAdvancedUIState* state = (MacroAdvancedUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state) return;
    
    bool isExecuting = AdvancedMacroSystem::IsExecuting();
    
    if (isExecuting != state->isExecuting)
    {
        state->isExecuting = isExecuting;
        
        if (isExecuting)
        {
            SetWindowTextW(state->hExecuteButton, L"⏹ Stop");
            SetWindowLongPtrW(state->hExecuteButton, GWLP_ID, MACRO_ADV_ID_STOP);
            SetWindowTextW(state->hStatusText, L"Executing...");
        }
        else
        {
            SetWindowTextW(state->hExecuteButton, L"▶ Execute");
            SetWindowLongPtrW(state->hExecuteButton, GWLP_ID, MACRO_ADV_ID_EXECUTE);
            SetWindowTextW(state->hStatusText, L"Ready");
        }
    }
}

void MacroAdvancedSubpage_SaveScript(HWND hWnd)
{
    MacroAdvancedUIState* state = (MacroAdvancedUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state || state->selectedMacroId < 0) return;
    
    // Get script text from editor
    int length = GetWindowTextLengthW(state->hScriptEditor);
    std::wstring scriptText(length + 1, L'\0');
    GetWindowTextW(state->hScriptEditor, &scriptText[0], length + 1);
    scriptText.resize(length);
    
    // Parse and save
    if (AdvancedMacroSystem::ParseScript(state->selectedMacroId, scriptText))
    {
        SetWindowTextW(state->hStatusText, L"Script sauvegardé avec succès!");
    }
    else
    {
        SetWindowTextW(state->hStatusText, L"Erreur lors de la sauvegarde du script");
    }
}

void MacroAdvancedSubpage_ShowHelp(HWND hWnd)
{
    MessageBoxW(hWnd, MACRO_HELP_TEXT, L"Aide - Macros Composites", MB_OK | MB_ICONINFORMATION);
}

void MacroAdvancedSubpage_InsertExample(HWND hWnd)
{
    MacroAdvancedUIState* state = (MacroAdvancedUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state) return;
    
    SetWindowTextW(state->hScriptEditor, MACRO_EXAMPLE_SCRIPT);
    SetWindowTextW(state->hStatusText, L"Exemple chargé - Modifiez et sauvegardez!");
}

// ============================================================================
// Window Procedure
// ============================================================================

LRESULT CALLBACK MacroAdvancedSubpageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    MacroAdvancedUIState* state = (MacroAdvancedUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    
    switch (msg)
    {
    case WM_CREATE:
        return 0;
    
    case WM_DESTROY:
        if (state)
        {
            delete state;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;
    
    case WM_TIMER:
        if (wParam == MACRO_ADV_UI_TIMER_ID)
        {
            MacroAdvancedSubpage_UpdateExecutionStatus(hWnd);
            return 0;
        }
        break;
    
    case WM_COMMAND:
        if (state)
        {
            switch (LOWORD(wParam))
            {
            case MACRO_ADV_ID_NEW:
                {
                    static int macroCounter = 1;
                    wchar_t name[64];
                    swprintf_s(name, L"Macro Composite %d", macroCounter++);
                    
                    int newId = AdvancedMacroSystem::CreateCompositeMacro(name);
                    if (newId >= 0)
                    {
                        state->selectedMacroId = newId;
                        MacroAdvancedSubpage_RefreshUI(hWnd);
                        PremiumCombo::SetCurSel(state->hMacroList, newId);
                    }
                }
                break;
            
            case MACRO_ADV_ID_MACRO_LIST:
                if (HIWORD(wParam) == CBN_SELCHANGE)
                {
                    int sel = PremiumCombo::GetCurSel(state->hMacroList);
                    state->selectedMacroId = sel >= 0 ? sel : -1;
                    MacroAdvancedSubpage_RefreshUI(hWnd);
                }
                break;
            
            case MACRO_ADV_ID_EXECUTE:
                if (state->selectedMacroId >= 0)
                {
                    // Save script first
                    MacroAdvancedSubpage_SaveScript(hWnd);
                    
                    // Start execution
                    AdvancedMacroSystem::StartExecution(state->selectedMacroId);
                    MacroAdvancedSubpage_UpdateExecutionStatus(hWnd);
                }
                break;
            
            case MACRO_ADV_ID_STOP:
                AdvancedMacroSystem::StopExecution();
                MacroAdvancedSubpage_UpdateExecutionStatus(hWnd);
                break;
            
            case MACRO_ADV_ID_DELETE:
                if (state->selectedMacroId >= 0)
                {
                    AdvancedMacroSystem::DeleteCompositeMacro(state->selectedMacroId);
                    state->selectedMacroId = -1;
                    MacroAdvancedSubpage_RefreshUI(hWnd);
                }
                break;
            
            case MACRO_ADV_ID_SAVE_SCRIPT:
                MacroAdvancedSubpage_SaveScript(hWnd);
                break;
            
            case MACRO_ADV_ID_VALIDATE:
                {
                    // Validate script syntax
                    MacroAdvancedSubpage_SaveScript(hWnd);
                    CompositeMacro* macro = AdvancedMacroSystem::GetCompositeMacro(state->selectedMacroId);
                    if (macro && !macro->commands.empty())
                    {
                        wchar_t msg[128];
                        swprintf_s(msg, L"Script valide! %zu commandes détectées.", macro->commands.size());
                        SetWindowTextW(state->hStatusText, msg);
                    }
                    else
                    {
                        SetWindowTextW(state->hStatusText, L"Script vide ou invalide");
                    }
                }
                break;
            
            case MACRO_ADV_ID_HELP:
                MacroAdvancedSubpage_ShowHelp(hWnd);
                break;
            
            case MACRO_ADV_ID_EXAMPLE:
                MacroAdvancedSubpage_InsertExample(hWnd);
                break;
            }
        }
        return 0;
    
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
        {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, UiTheme::Color_Text());
            SetBkColor(hdc, UiTheme::Color_ControlBg());
            return (LRESULT)UiTheme::Brush_ControlBg();
        }
    
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            
            RECT rc;
            GetClientRect(hWnd, &rc);
            FillRect(hdc, &rc, UiTheme::Brush_PanelBg());
            
            EndPaint(hWnd, &ps);
            return 0;
        }
    
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    
    return 0;
}
