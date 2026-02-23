// c:\Users\LePaysDeLest\Documents\projet\Projet_TEST\Debug_Analog_CHEMIN\Macro_V_6.0_TEST _AVEC-LOG(AnalogManquand-Macro-Combo-AXE)_TEST_GEMINI\HallJoy\macro_ui.cpp

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cwctype>

#include "macro_ui.h"
#include "macro_system.h"
#include "macro_editor.h"
#include "macro_json_storage.h"
#include "ui_theme.h"
#include "win_util.h"
#include "premium_combo.h"
#include "mouse_combo_system.h"
#include "settings.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Comdlg32.lib")

// Macros de contrôles Windows manquantes
#ifndef ComboBox_AddString
#define ComboBox_AddString(hwnd, str)  SendMessage((hwnd), CB_ADDSTRING, 0, (LPARAM)(str))
#define ComboBox_SetCurSel(hwnd, index) SendMessage((hwnd), CB_SETCURSEL, (WPARAM)(index), 0)
#define ComboBox_GetCurSel(hwnd)       (int)SendMessage((hwnd), CB_GETCURSEL, 0, 0)
#endif

#ifndef Button_SetCheck
#define Button_SetCheck(hwnd, state)   SendMessage((hwnd), BM_SETCHECK, (WPARAM)(state), 0)
#define Button_GetCheck(hwnd)          (int)SendMessage((hwnd), BM_GETCHECK, 0, 0)
#endif

static constexpr int COMBO_ID_ADD_DELAY = 4100;

static int S(HWND hwnd, int px) { return WinUtil_ScalePx(hwnd, px); }

// NEW: Combo trigger names and action names
static const wchar_t* COMBO_TRIGGER_NAMES[] = {
    L"Right click held + Left click",
    L"Left click held + Right click",
    L"Middle click held + Left click",
    L"Middle click held + Right click",
    L"Double left click",
    L"Double right click",
    L"Triple left click",
    L"Left AND Right click together",
    L"Wheel up with right click held",
    L"Wheel down with right click held"
};

static const wchar_t* COMBO_ACTION_TYPE_NAMES[] = {
    L"Press key",
    L"Release key",
    L"Press+Release key",
    L"Type text",
    L"Mouse click",
    L"Wait (ms)"
};

// Helper: get executable directory (defaults for Export/Import)
static std::wstring GetExeDir()
{
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring path(buf);
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L".";
    return path.substr(0, pos);
}

// ============================================================================
// Macro Subpage Implementation
// ============================================================================

void MacroSubpage_RefreshUI(HWND hWnd)
{
    MacroUIState* state = (MacroUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state) return;

    if (state->currentMode == MacroComboMode::Macro)
    {
        // Update macro list
        PremiumCombo::Clear(state->hMacroList);

        std::vector<int> macroIds = MacroSystem::GetAllMacroIds();
        for (int id : macroIds)
        {
            Macro* macro = MacroSystem::GetMacro(id);
            if (macro)
            {
                PremiumCombo::AddString(state->hMacroList, macro->name.c_str());
            }
        }

        // Update button states
        bool hasMacros = !macroIds.empty();
        bool hasSelection = state->selectedMacroId >= 0 && state->selectedMacroId < (int)macroIds.size();

        EnableWindow(state->hRecordButton, hasSelection);
        EnableWindow(state->hPlayButton, hasSelection);

        // Update checkboxes
        if (hasSelection)
        {
            Macro* macro = MacroSystem::GetMacro(state->selectedMacroId);
            if (macro)
            {
                SendMessageW(state->hLoopCheckbox, BM_SETCHECK, macro->isLooping ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessageW(state->hBlockKeysCheckbox, BM_SETCHECK, macro->blockKeysDuringPlayback ? BST_CHECKED : BST_UNCHECKED, 0);

                // Update speed slider (convert from float to slider position)
                int sliderPos = (int)(macro->playbackSpeed * 10);
                sliderPos = std::clamp(sliderPos, 10, 50);
                SendMessageW(state->hSpeedSlider, TBM_SETPOS, TRUE, sliderPos);
            }
        }
    }
    else // Combo mode - use the dedicated function
    {
        MacroSubpage_UpdateComboList(hWnd);
        MacroSubpage_UpdateComboUI(hWnd);
    }
}

void MacroSubpage_UpdateRecordingStatus(HWND hWnd)
{
    MacroUIState* state = (MacroUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state) return;

    bool isRecording = MacroSystem::IsRecording();

    if (isRecording != state->isRecording)
    {
        state->isRecording = isRecording;

        if (isRecording)
        {
            SetWindowTextW(state->hRecordButton, L"\u23F9 Stop"); // Square for stop
            // Change button ID to stop recording
            SetWindowLongPtrW(state->hRecordButton, GWLP_ID, MACRO_ID_RECORD_STOP);
            InvalidateRect(state->hRecordButton, nullptr, TRUE);
        }
        else
        {
            SetWindowTextW(state->hRecordButton, L"\u25CF Rec"); // Circle for record
            // Change button ID back to start recording
            SetWindowLongPtrW(state->hRecordButton, GWLP_ID, MACRO_ID_RECORD_START);
            InvalidateRect(state->hRecordButton, nullptr, TRUE);
        }
    }

    // Update status text using state->hStatusText directly
    if (state->hStatusText)
    {
        if (isRecording)
        {
            uint32_t duration = MacroSystem::GetRecordingDuration();
            uint32_t seconds = duration / 1000;
            uint32_t minutes = seconds / 60;
            seconds = seconds % 60;

            wchar_t status[256];
            swprintf_s(status, L"Recording... %02u:%02u", minutes, seconds);
            SetWindowTextW(state->hStatusText, status);
        }
        else if (!MacroSystem::IsPlaying())
        {
            SetWindowTextW(state->hStatusText, L"Ready");
        }
    }
}

void MacroSubpage_UpdatePlaybackStatus(HWND hWnd)
{
    MacroUIState* state = (MacroUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state) return;

    bool isPlaying = MacroSystem::IsPlaying();

    if (isPlaying != state->isPlaying)
    {
        state->isPlaying = isPlaying;

        if (isPlaying)
        {
            SetWindowTextW(state->hPlayButton, L"\u23F9 Stop");
            SetWindowLongPtrW(state->hPlayButton, GWLP_ID, MACRO_ID_PLAY_STOP);
            InvalidateRect(state->hPlayButton, nullptr, TRUE);
        }
        else
        {
            SetWindowTextW(state->hPlayButton, L"\u25B6 Play"); // Play triangle
            SetWindowLongPtrW(state->hPlayButton, GWLP_ID, MACRO_ID_PLAY_START);
            InvalidateRect(state->hPlayButton, nullptr, TRUE);
        }
    }

    // Update status text
    if (state->hStatusText)
    {
        if (isPlaying)
        {
            int playingId = MacroSystem::GetPlayingMacroId();
            Macro* macro = MacroSystem::GetMacro(playingId);
            if (macro)
            {
                wchar_t status[256];
                swprintf_s(status, L"Playing: %s (speed %.1fx)", macro->name.c_str(), MacroSystem::GetPlaybackSpeed());
                SetWindowTextW(state->hStatusText, status);
            }
        }
        else if (!MacroSystem::IsRecording())
        {
            SetWindowTextW(state->hStatusText, L"Pret");
        }
    }
}

// ============================================================================
// Window Procedure
// ============================================================================

// UI creation helpers
HWND MacroSubpage_Create(HWND hParent, HINSTANCE hInst);
void MacroSubpage_RefreshUI(HWND hWnd);
void MacroSubpage_UpdateRecordingStatus(HWND hWnd);
void MacroSubpage_UpdatePlaybackStatus(HWND hWnd);

// Helper for font
static HFONT GetUIFont(HWND hwnd) {
    static HFONT hFont = nullptr;
    if (!hFont) {
        HDC hdc = GetDC(hwnd);
        int logPixelsY = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(hwnd, hdc);
        int height = -MulDiv(9, logPixelsY, 72); // 9pt
        hFont = CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Segoe UI");
    }
    return hFont;
}

// Simplified window creation function (no duplicates)
LRESULT MacroSubpage_OnCreate(HWND hWnd, LPARAM lParam)
{
    HINSTANCE hInst = ((LPCREATESTRUCTW)lParam)->hInstance;

    // Create UI state
    MacroUIState* state = new MacroUIState();
    state->currentMode = MacroComboMode::Combo; // Default to Combo
    SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)state);

    // Create controls directly in this window (not create child window)
    int y = S(hWnd, 10);
    int x = S(hWnd, 10);
    int width = S(hWnd, 500);
    int height = S(hWnd, 25);
    int gap = S(hWnd, 8);

    // Apply dark theme function (following the pattern from other tabs)
    HFONT hFont = GetUIFont(hWnd);
    auto ApplyDarkTheme = [hFont](HWND hwnd) {
        if (!hwnd) return;

        // Set font first (like other tabs)
        SendMessageW(hwnd, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Then apply theme
        UiTheme::ApplyToControl(hwnd);

        // Force repaint
        InvalidateRect(hwnd, nullptr, TRUE);
        UpdateWindow(hwnd);
        };

    // NEW: Mode selection combo (Macro/Combo)
    CreateWindowW(L"STATIC", L"Mode:",
        WS_CHILD | WS_VISIBLE,
        x, y, S(hWnd, 60), height, hWnd, nullptr, hInst, nullptr);

    // Use PremiumCombo for Mode selection
    state->hModeCombo = PremiumCombo::Create(hWnd, hInst, x + S(hWnd, 50), y, S(hWnd, 150), height, MACRO_ID_MODE_COMBO);
    PremiumCombo::AddString(state->hModeCombo, L"Combo");
    PremiumCombo::AddString(state->hModeCombo, L"Macro");
    PremiumCombo::SetCurSel(state->hModeCombo, 0); // Default to Combo (index 0)

    // New Macro/Combo button - Align to right on SAME ROW
    int btnNewW = S(hWnd, 130);
    state->hNewMacroBtn = CreateWindowW(L"BUTTON", L"\uFF0B New Combo",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x + width - btnNewW, y, btnNewW, height, hWnd, (HMENU)(uintptr_t)MACRO_ID_NEW_MACRO, hInst, nullptr);
    ApplyDarkTheme(state->hNewMacroBtn);

    y += height + gap;

    // Macro list
    state->hMacroList = PremiumCombo::Create(hWnd, hInst, x, y, width, height, MACRO_ID_MACRO_LIST);
    PremiumCombo::SetPlaceholderText(state->hMacroList, L"Select a macro...");
    UiTheme::ApplyToControl(state->hMacroList);

    y += height + gap;

    // Create ALL controls (both macro and combo) - we'll manage visibility later

    // Create macro controls
    // Macro options
    // Layout: [Record] [Play] [Delete] in one row
    int btnW = (width - gap * 2) / 3;

    state->hRecordButton = CreateWindowW(L"BUTTON", L"\u25CF Rec",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x, y, btnW, height, hWnd, (HMENU)(uintptr_t)MACRO_ID_RECORD_START, hInst, nullptr);
    ApplyDarkTheme(state->hRecordButton);

    state->hPlayButton = CreateWindowW(L"BUTTON", L"\u25B6 Play",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x + btnW + gap, y, btnW, height, hWnd, (HMENU)(uintptr_t)MACRO_ID_PLAY_START, hInst, nullptr);
    ApplyDarkTheme(state->hPlayButton);

    state->hDeleteMacroBtn = CreateWindowW(L"BUTTON", L"\U0001F5D1 Del",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x + (btnW + gap) * 2, y, btnW, height, hWnd, (HMENU)(uintptr_t)MACRO_ID_DELETE_MACRO, hInst, nullptr);
    ApplyDarkTheme(state->hDeleteMacroBtn);

    y += height + gap;

    state->hLoopCheckbox = CreateWindowW(L"BUTTON", L"Loop",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_OWNERDRAW,
        x, y, S(hWnd, 100), height, hWnd, (HMENU)(uintptr_t)MACRO_ID_LOOP_CHECKBOX, hInst, nullptr);
    ApplyDarkTheme(state->hLoopCheckbox);

    state->hBlockKeysCheckbox = CreateWindowW(L"BUTTON", L"Block keys during playback",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_OWNERDRAW,
        x + S(hWnd, 110), y, S(hWnd, 250), height, hWnd, (HMENU)(uintptr_t)MACRO_ID_BLOCK_KEYS_CHECKBOX, hInst, nullptr);
    ApplyDarkTheme(state->hBlockKeysCheckbox);

    y += height + gap;

    // Speed slider row
    state->hSpeedLabel = CreateWindowW(L"STATIC", L"Playback Speed:",
        WS_CHILD | WS_VISIBLE, x, y, S(hWnd, 120), height, hWnd, nullptr, hInst, nullptr);
    ApplyDarkTheme(state->hSpeedLabel);

    state->hSpeedSlider = CreateWindowW(TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_BOTH | TBS_NOTICKS,
        x + S(hWnd, 130), y, width - S(hWnd, 130), height, hWnd, (HMENU)(uintptr_t)MACRO_ID_SPEED_SLIDER, hInst, nullptr);

    // Configure slider with explicit settings
    SendMessageW(state->hSpeedSlider, TBM_SETRANGE, TRUE, MAKELONG(50, 200)); // 0.5x to 2.0x speed
    SendMessageW(state->hSpeedSlider, TBM_SETPOS, TRUE, 100); // Default 1.0x speed
    SendMessageW(state->hSpeedSlider, TBM_SETPAGESIZE, 0, 10);
    SendMessageW(state->hSpeedSlider, TBM_SETLINESIZE, 0, 5);

    // Force redraw
    InvalidateRect(state->hSpeedSlider, nullptr, TRUE);
    UpdateWindow(state->hSpeedSlider);

    // Add speed value label (hidden initially or updated)
    // We'll put it below or next to it. Let's put it below for now to save width space if needed, or skip creating a separate label if not strictly needed, but let's keep it.
    y += height;
    state->hSpeedValueLabel = CreateWindowW(L"STATIC", L"Speed: 1.0x",
        WS_CHILD | WS_VISIBLE, x, y, width, height, hWnd, nullptr, hInst, nullptr);
    ApplyDarkTheme(state->hSpeedValueLabel);

    y += height + gap;

    state->hActionsRecordedLabel = CreateWindowW(L"STATIC", L"Recorded Actions: 0",
        WS_CHILD | WS_VISIBLE, x, y, width, height, hWnd, nullptr, hInst, nullptr);
    y += height + gap;

    state->hActionsList = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        x, y, width, S(hWnd, 150), hWnd, (HMENU)(uintptr_t)MACRO_ID_ACTIONS_LIST, hInst, nullptr);
    UiTheme::ApplyToControl(state->hActionsList);

    y += S(hWnd, 150) + gap;

    // Edit timing and export/import buttons
    int btnWidth2 = (width - gap * 2) / 3;

    state->hEditTimingBtn = CreateWindowW(L"BUTTON", L"Editer Timing",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x, y, btnWidth2, height, hWnd, (HMENU)(uintptr_t)MACRO_ID_EDIT_TIMING, hInst, nullptr);
    ApplyDarkTheme(state->hEditTimingBtn);

    state->hExportJsonBtn = CreateWindowW(L"BUTTON", L"Exporter JSON",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x + btnWidth2 + gap, y, btnWidth2, height, hWnd, (HMENU)(uintptr_t)MACRO_ID_EXPORT_JSON, hInst, nullptr);
    ApplyDarkTheme(state->hExportJsonBtn);

    state->hImportJsonBtn = CreateWindowW(L"BUTTON", L"Importer JSON",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x + btnWidth2 * 2 + gap * 2, y, btnWidth2, height, hWnd, (HMENU)(uintptr_t)MACRO_ID_IMPORT_JSON, hInst, nullptr);
    ApplyDarkTheme(state->hImportJsonBtn);

    y += height + gap * 2;
    // Save location labels removed as requested
    state->hSaveLocationLabel = nullptr;
    state->hSaveDirLabel = nullptr;

    // Emergency stop button (Common)
    state->hEmergencyStopBtn = CreateWindowW(L"BUTTON", L"EMERGENCY STOP",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x, y, width, height, hWnd, (HMENU)(uintptr_t)MACRO_ID_PLAY_STOP, hInst, nullptr);
    ApplyDarkTheme(state->hEmergencyStopBtn);
    {
        RECT rcStop{};
        GetWindowRect(state->hEmergencyStopBtn, &rcStop);
        MapWindowPoints(nullptr, hWnd, (LPPOINT)&rcStop, 2);
        state->emergencyStopOrigRect = rcStop;
    }

    // --- COMBO CONTROLS ---
    // Reset Y to start after Macro List for combo controls
    RECT rcList;
    GetWindowRect(state->hMacroList, &rcList);
    y = S(hWnd, 10) + height + gap + height + gap + height + gap; // Approximate Y after list

    // Create combo controls
    // Trigger configuration
    state->hTriggerLabel = CreateWindowW(L"STATIC", L"Trigger:",
        WS_CHILD | WS_VISIBLE,
        x, y, width, height, hWnd, nullptr, hInst, nullptr);
    ApplyDarkTheme(state->hTriggerLabel);
    y += height;

    // Use PremiumCombo for Trigger List
    state->hTriggerList = PremiumCombo::Create(hWnd, hInst, x, y, width, height, COMBO_ID_TRIGGER_LIST);
    for (int i = 0; i < 10; ++i) {
        PremiumCombo::AddString(state->hTriggerList, COMBO_TRIGGER_NAMES[i]);
    }
    PremiumCombo::SetCurSel(state->hTriggerList, 0);

    y += height + gap;

    state->hComboStatusLabel = CreateWindowW(L"STATIC", L"No combo selected",
        WS_CHILD | WS_VISIBLE,
        x, y, width, height, hWnd, nullptr, hInst, nullptr);
    ApplyDarkTheme(state->hComboStatusLabel);

    y += height + gap;

    // Repeat checkbox and delay slider on same row
    state->hRepeatCheckbox = CreateWindowW(L"BUTTON", L"Automatic repeat",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x, y, width / 2 - gap / 2, height, hWnd, (HMENU)(uintptr_t)COMBO_ID_REPEAT_CHECKBOX, hInst, nullptr);
    ApplyDarkTheme(state->hRepeatCheckbox);

    // Delay slider on right side
    state->hDelayLabel = CreateWindowW(L"STATIC", L"Delay (ms):",
        WS_CHILD | WS_VISIBLE,
        x + width / 2 + gap / 2, y, width / 2 - gap / 2, height, hWnd, nullptr, hInst, nullptr);
    ApplyDarkTheme(state->hDelayLabel);
    y += height;

    state->hDelaySlider = CreateWindowW(TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
        x + width / 2 + gap / 2, y, width / 2 - gap / 2, S(hWnd, 30), hWnd, (HMENU)(uintptr_t)COMBO_ID_DELAY_SLIDER, hInst, nullptr);
    ApplyDarkTheme(state->hDelaySlider);
    SendMessageW(state->hDelaySlider, TBM_SETRANGE, FALSE, MAKELONG(10, 1000)); // 10ms to 1000ms
    SendMessageW(state->hDelaySlider, TBM_SETPOS, TRUE, 100); // Default 100ms

    y += S(hWnd, 30) + gap;

    // Action configuration
    state->hActionLabel = CreateWindowW(L"STATIC", L"Action:",
        WS_CHILD | WS_VISIBLE,
        x, y, width, height, hWnd, nullptr, hInst, nullptr);
    ApplyDarkTheme(state->hActionLabel);

    y += height;

    // Use PremiumCombo for Action Type
    state->hActionType = PremiumCombo::Create(hWnd, hInst, x, y, width, height, COMBO_ID_ACTION_TYPE);
    for (int i = 0; i < 6; ++i) {
        PremiumCombo::AddString(state->hActionType, COMBO_ACTION_TYPE_NAMES[i]);
    }

    y += height + gap;

    // Action Key input and buttons on same row
    state->hActionKey = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        x, y, S(hWnd, 100), height, hWnd, (HMENU)(uintptr_t)COMBO_ID_ACTION_KEY, hInst, nullptr);
    ApplyDarkTheme(state->hActionKey);

    // Add/Remove Action Buttons
    int actionBtnW = S(hWnd, 100);
    state->hAddActionButton = CreateWindowW(L"BUTTON", L"\uFF0B Add",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x + S(hWnd, 110), y, actionBtnW, height, hWnd, (HMENU)(uintptr_t)COMBO_ID_ADD_ACTION, hInst, nullptr);
    ApplyDarkTheme(state->hAddActionButton);

    // NEW: Delay button
    HWND hDelayBtn = CreateWindowW(L"BUTTON", L"\u23F1 Delay",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x + S(hWnd, 110) + actionBtnW + gap, y, actionBtnW, height, hWnd, (HMENU)(uintptr_t)COMBO_ID_ADD_DELAY, hInst, nullptr);
    ApplyDarkTheme(hDelayBtn);

    state->hRemoveActionButton = CreateWindowW(L"BUTTON", L"\u2212 Del",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x + S(hWnd, 110) + (actionBtnW + gap) * 2, y, actionBtnW, height, hWnd, (HMENU)(uintptr_t)COMBO_ID_REMOVE_ACTION, hInst, nullptr);
    ApplyDarkTheme(state->hRemoveActionButton);

    y += height + gap;

    // Information labels for actions
    state->hActionCountLabel = CreateWindowW(L"STATIC", L"Configured Actions: 0",
        WS_CHILD | WS_VISIBLE,
        x, y, width, height, hWnd, nullptr, hInst, nullptr);
    ApplyDarkTheme(state->hActionCountLabel);

    y += height;

    state->hLastActionLabel = CreateWindowW(L"STATIC", L"Last Action: None",
        WS_CHILD | WS_VISIBLE,
        x, y, width, height, hWnd, nullptr, hInst, nullptr);
    ApplyDarkTheme(state->hLastActionLabel);

    y += height + gap;

    // Dedicated actions list (simple two-column text via formatting)
    // ADDED: LBS_OWNERDRAWFIXED | LBS_HASSTRINGS for custom drawing
    state->hComboActionsList = CreateWindowW(L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_BORDER | WS_VSCROLL | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
        x, y, width, S(hWnd, 120), hWnd, (HMENU)(uintptr_t)COMBO_ID_ACTIONS_LIST, hInst, nullptr);
    ApplyDarkTheme(state->hComboActionsList);

    y += S(hWnd, 120) + gap;

    // Clear / Export / Import buttons
    state->hClearActionsButton = CreateWindowW(L"BUTTON", L"\U0001F5D1 Clear All",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x, y, btnWidth2, height, hWnd, (HMENU)(uintptr_t)COMBO_ID_CLEAR_ACTIONS, hInst, nullptr);
    ApplyDarkTheme(state->hClearActionsButton);

    state->hComboExportBtn = CreateWindowW(L"BUTTON", L"Export JSON",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x + btnWidth2 + gap, y, btnWidth2, height, hWnd, (HMENU)(uintptr_t)COMBO_ID_EXPORT_JSON, hInst, nullptr);
    ApplyDarkTheme(state->hComboExportBtn);

    state->hComboImportBtn = CreateWindowW(L"BUTTON", L"Import JSON",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x + btnWidth2 * 2 + gap * 2, y, btnWidth2, height, hWnd, (HMENU)(uintptr_t)COMBO_ID_IMPORT_JSON, hInst, nullptr);
    ApplyDarkTheme(state->hComboImportBtn);

    // Initially set visibility based on current mode
    MacroSubpage_SwitchMode(hWnd, state->currentMode);

    // Force hide macro controls if in combo mode
    if (state->currentMode == MacroComboMode::Combo)
    {
        // Hide macro controls explicitly
        if (state->hRecordButton) ShowWindow(state->hRecordButton, SW_HIDE);
        if (state->hPlayButton) ShowWindow(state->hPlayButton, SW_HIDE);
        if (state->hLoopCheckbox) ShowWindow(state->hLoopCheckbox, SW_HIDE);
        if (state->hBlockKeysCheckbox) ShowWindow(state->hBlockKeysCheckbox, SW_HIDE);
        if (state->hSpeedSlider) ShowWindow(state->hSpeedSlider, SW_HIDE);
        if (state->hEditTimingBtn) ShowWindow(state->hEditTimingBtn, SW_HIDE);
        if (state->hExportJsonBtn) ShowWindow(state->hExportJsonBtn, SW_HIDE);
        if (state->hImportJsonBtn) ShowWindow(state->hImportJsonBtn, SW_HIDE);
        if (state->hActionsList) ShowWindow(state->hActionsList, SW_HIDE);

        // Force hide all macro-related STATIC labels
        HWND hChild = GetWindow(hWnd, GW_CHILD);
        while (hChild)
        {
            HWND hNext = GetWindow(hChild, GW_HWNDNEXT);
            wchar_t className[256];
            wchar_t text[256];
            GetClassNameW(hChild, className, 256);
            GetWindowTextW(hChild, text, 256);

            if (wcscmp(className, L"STATIC") == 0)
            {
                // Hide ALL macro-related labels
                if (wcsstr(text, L"Options:") != nullptr ||
                    wcsstr(text, L"Speed") != nullptr ||
                    wcsstr(text, L"Status:") != nullptr ||
                    wcsstr(text, L"Recorded Actions") != nullptr ||
                    wcsstr(text, L"EMERGENCY STOP") != nullptr ||
                    wcsstr(text, L"Select a macro") != nullptr)
                {
                    ShowWindow(hChild, SW_HIDE);
                    SetWindowPos(hChild, nullptr, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOZORDER | SWP_NOACTIVATE);
                }
            }

            hChild = hNext;
        }
    }

    // Start UI refresh timer
    SetTimer(hWnd, MACRO_UI_TIMER_ID, MACRO_UI_REFRESH_MS, nullptr);

    // Initial UI refresh
    MacroSubpage_RefreshUI(hWnd);

    return 0;
}

// Custom draw function for macro buttons
auto DrawMacroButton = [](const DRAWITEMSTRUCT* dis) {
    if (!dis) return;

    RECT rc = dis->rcItem;
    HDC hdc = dis->hDC;

    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool hot = (dis->itemState & ODS_HOTLIGHT) != 0;

    // Modern flat style with rounded corners
    COLORREF bg = UiTheme::Color_ControlBg();
    COLORREF border = UiTheme::Color_Border();
    COLORREF textColor = UiTheme::Color_Text();

    // Special style for Stop/Emergency/Delete buttons
    bool isDestructive = (dis->CtlID == MACRO_ID_PLAY_STOP ||
        dis->CtlID == MACRO_ID_RECORD_STOP ||
        dis->CtlID == MACRO_ID_DELETE_MACRO ||
        dis->CtlID == COMBO_ID_REMOVE_ACTION ||
        dis->CtlID == COMBO_ID_CLEAR_ACTIONS);

    if (disabled) {
        textColor = UiTheme::Color_TextMuted();
        bg = RGB(25, 25, 25);
    }
    else {
        if (isDestructive) {
            bg = pressed ? RGB(160, 40, 40) : (hot ? RGB(190, 60, 60) : RGB(140, 40, 40));
            border = RGB(200, 80, 80);
        }
        else {
            bg = pressed ? UiTheme::Color_Accent() : (hot ? RGB(60, 60, 65) : RGB(45, 45, 48));
            if (pressed) { textColor = RGB(20, 20, 20); border = UiTheme::Color_Accent(); }
        }
    }

    // 1. Clear background (corners) to prevent artifacts/ghosting
    FillRect(hdc, &rc, UiTheme::Brush_PanelBg());

    // 2. Draw rounded button
    HBRUSH br = CreateSolidBrush(bg);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, br); // Use solid brush to fill

    // Rounded rectangle
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    DeleteObject(br);

    wchar_t text[128]{};
    GetWindowTextW(dis->hwndItem, text, (int)(sizeof(text) / sizeof(text[0])));

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textColor);
    DrawTextW(hdc, text, -1, &rc,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    };

// Custom draw for Action List Items
auto DrawActionListItem = [](const DRAWITEMSTRUCT* dis) {
    if (!dis || dis->itemID == -1) return;

    RECT rc = dis->rcItem;
    HDC hdc = dis->hDC;
    bool selected = (dis->itemState & ODS_SELECTED);

    // Colors
    COLORREF bg = selected ? UiTheme::Color_Accent() : UiTheme::Color_ControlBg();
    COLORREF text = selected ? RGB(20, 20, 20) : UiTheme::Color_Text();
    COLORREF textMuted = selected ? RGB(60, 60, 60) : UiTheme::Color_TextMuted();

    HBRUSH br = CreateSolidBrush(bg);
    FillRect(hdc, &rc, br);
    DeleteObject(br);

    // Get text
    wchar_t buf[256];
    SendMessageW(dis->hwndItem, LB_GETTEXT, dis->itemID, (LPARAM)buf);

    // Parse "Type | Details"
    std::wstring fullText(buf);
    std::wstring typeText = fullText;
    std::wstring detailText = L"";
    size_t sep = fullText.find(L" | ");
    if (sep != std::wstring::npos) {
        typeText = fullText.substr(0, sep);
        detailText = fullText.substr(sep + 3);
    }

    SetBkMode(hdc, TRANSPARENT);

    // Draw Type (Bold simulation via color/position)
    RECT rcType = rc;
    rcType.left += WinUtil_ScalePx(dis->hwndItem, 6);
    rcType.right = rc.left + WinUtil_ScalePx(dis->hwndItem, 100); // Fixed width for type

    HFONT hOldFont = (HFONT)SelectObject(hdc, (HFONT)SendMessage(dis->hwndItem, WM_GETFONT, 0, 0));

    SetTextColor(hdc, text);
    DrawTextW(hdc, typeText.c_str(), -1, &rcType, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Draw Details
    if (!detailText.empty()) {
        RECT rcDetail = rc;
        rcDetail.left = rcType.right + WinUtil_ScalePx(dis->hwndItem, 10);
        SetTextColor(hdc, textMuted);
        if (selected) SetTextColor(hdc, text);

        DrawTextW(hdc, detailText.c_str(), -1, &rcDetail, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    SelectObject(hdc, hOldFont);
    };

// Window Procedure
LRESULT CALLBACK MacroSubpageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    MacroUIState* state = (MacroUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    switch (msg)
    {
    case WM_CREATE:
    {
        // FIX: Ajouter WS_CLIPSIBLINGS pour que le WM_PAINT ne deborde pas
        // sur les onglets adjacents (onglet Combo visible sous l'onglet Macro)
        LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_STYLE);
        SetWindowLongPtrW(hWnd, GWL_STYLE, style | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
        SetWindowPos(hWnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        return MacroSubpage_OnCreate(hWnd, lParam);
    }

    case WM_MEASUREITEM:
        if (wParam == COMBO_ID_ACTIONS_LIST) {
            MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lParam;
            mis->itemHeight = S(hWnd, 22); // Comfortable height
            return TRUE;
        }
        break;

    case WM_DRAWITEM:
        // Handle custom drawing for buttons and listbox
        if (wParam) {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;

            if (dis->CtlID == COMBO_ID_ACTIONS_LIST) {
                DrawActionListItem(dis);
                return TRUE;
            }

            if (dis->CtlType == ODT_BUTTON) {
                // Check if it's one of our macro buttons
                switch (dis->CtlID) {
                case MACRO_ID_NEW_MACRO:
                case MACRO_ID_RECORD_START:
                case MACRO_ID_RECORD_STOP:
                case MACRO_ID_PLAY_START:
                case MACRO_ID_PLAY_STOP:
                case MACRO_ID_DELETE_MACRO:
                case MACRO_ID_LOOP_CHECKBOX:
                case MACRO_ID_BLOCK_KEYS_CHECKBOX:
                case MACRO_ID_EDIT_TIMING:
                case MACRO_ID_EXPORT_JSON:
                case MACRO_ID_IMPORT_JSON:
                case COMBO_ID_ADD_ACTION:
                case COMBO_ID_REMOVE_ACTION:
                case COMBO_ID_ADD_DELAY:
                case COMBO_ID_CLEAR_ACTIONS:
                case COMBO_ID_EXPORT_JSON:
                case COMBO_ID_IMPORT_JSON:
                    DrawMacroButton(dis);
                    return TRUE;
                }
            }
        }
        break;

    case WM_CTLCOLORSTATIC:
        // Force static controls to use dark theme colors
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetTextColor(hdc, UiTheme::Color_Text());
        SetBkColor(hdc, UiTheme::Color_ControlBg());
        return (LRESULT)UiTheme::Brush_ControlBg();
    }
    break;

    case WM_CTLCOLOREDIT:
        // Force edit controls to use dark theme colors
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetTextColor(hdc, UiTheme::Color_Text());
        SetBkColor(hdc, UiTheme::Color_ControlBg());
        return (LRESULT)UiTheme::Brush_ControlBg();
    }
    break;

    case WM_CTLCOLORLISTBOX:
        // Force combobox/listbox controls to use dark theme colors
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetTextColor(hdc, UiTheme::Color_Text());
        SetBkColor(hdc, UiTheme::Color_ControlBg());
        return (LRESULT)UiTheme::Brush_ControlBg();
    }
    break;

    case WM_CTLCOLORBTN:
        // Force button controls to use dark theme colors
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetTextColor(hdc, UiTheme::Color_Text());
        SetBkColor(hdc, UiTheme::Color_ControlBg());
        return (LRESULT)UiTheme::Brush_ControlBg();
    }
    break;

    case WM_CTLCOLORDLG:
        // Force dialog controls to use dark theme colors
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetTextColor(hdc, UiTheme::Color_Text());
        SetBkColor(hdc, UiTheme::Color_ControlBg());
        return (LRESULT)UiTheme::Brush_ControlBg();
    }
    break;

    case WM_CTLCOLORSCROLLBAR:
        // Force scrollbar controls to use dark theme colors
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetTextColor(hdc, UiTheme::Color_Text());
        SetBkColor(hdc, UiTheme::Color_ControlBg());
        return (LRESULT)UiTheme::Brush_ControlBg();
    }
    break;

    case WM_DESTROY:
        if (state)
        {
            delete state;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;

    case WM_TIMER:
        if (wParam == MACRO_UI_TIMER_ID)
        {
            MacroSubpage_UpdateRecordingStatus(hWnd);
            MacroSubpage_UpdatePlaybackStatus(hWnd);
            MacroSubpage_UpdateActionsList(hWnd);  // NEW: Mise à jour de la liste
            return 0;
        }
        break;

    case WM_HSCROLL:
        if (state)
        {
            if ((HWND)lParam == state->hSpeedSlider)
            {
                int pos = (int)SendMessageW(state->hSpeedSlider, TBM_GETPOS, 0, 0);
                float speed = pos / 10.0f;
                MacroSystem::SetPlaybackSpeed(speed);

                if (state->hSpeedValueLabel) {
                    wchar_t buf[64];
                    swprintf_s(buf, L"Vitesse: %.1fx", speed);
                    SetWindowTextW(state->hSpeedValueLabel, buf);
                }
            }
            else if ((HWND)lParam == state->hDelaySlider)
            {
                int pos = (int)SendMessageW(state->hDelaySlider, TBM_GETPOS, 0, 0);

                if (state->selectedComboId >= 0)
                {
                    MouseCombo* combo = MouseComboSystem::GetCombo(state->selectedComboId);
                    if (combo) combo->repeatDelayMs = pos;
                }

                if (state->hDelayLabel) {
                    wchar_t buf[64];
                    swprintf_s(buf, L"Delai (ms): %d", pos);
                    SetWindowTextW(state->hDelayLabel, buf);
                }
            }
        }
        break;

    case WM_COMMAND:
        if (state)
        {
            switch (LOWORD(wParam))
            {
                // NEW: Mode selection
            case MACRO_ID_MODE_COMBO:
                if (HIWORD(wParam) == CBN_SELCHANGE)
                {
                    int sel = PremiumCombo::GetCurSel(state->hModeCombo);
                    MacroComboMode newMode = (sel == 0) ? MacroComboMode::Combo : MacroComboMode::Macro;
                    MacroSubpage_SwitchMode(hWnd, newMode);
                }
                break;

            case MACRO_ID_NEW_MACRO:
            {
                if (state->currentMode == MacroComboMode::Macro)
                {
                    // Create a new macro with default name
                    static int macroCounter = 1;
                    wchar_t name[64];
                    swprintf_s(name, L"Macro %d", macroCounter++);

                    int newId = MacroSystem::CreateMacro(name);
                    if (newId >= 0)
                    {
                        state->selectedMacroId = newId;
                        MacroSubpage_RefreshUI(hWnd);
                        // Select the new macro in the combo
                        PremiumCombo::SetCurSel(state->hMacroList, newId);
                    }
                }
                else // Combo mode
                {
                    // Create a new combo with default settings
                    static int comboCounter = 1;
                    wchar_t name[64];
                    swprintf_s(name, L"Combo %d", comboCounter++);

                    int newId = MouseComboSystem::CreateCombo(name, ComboTriggerType::RightClickHeld_LeftClick);
                    if (newId >= 0)
                    {
                        // Enable repetition
                        MouseComboSystem::SetComboRepeat(newId, true, Settings_GetComboRepeatThrottleMs());

                        state->selectedComboId = newId;
                        MacroSubpage_UpdateComboList(hWnd);
                        // Select the new combo by index
                        int idx = -1;
                        for (int i = 0; i < (int)state->comboListIds.size(); ++i)
                        {
                            if (state->comboListIds[i] == newId) { idx = i; break; }
                        }
                        if (idx >= 0)
                            PremiumCombo::SetCurSel(state->hMacroList, idx);
                        MacroSubpage_UpdateComboUI(hWnd);
                        if (state->hComboStatusLabel)
                        {
                            std::wstring status = L"Selection: Combo " + std::to_wstring(newId);
                            SetWindowTextW(state->hComboStatusLabel, status.c_str());
                        }
                    }
                }
            }
            break;
            case MACRO_ID_MACRO_LIST:
                if (HIWORD(wParam) == CBN_SELCHANGE)
                {
                    int sel = PremiumCombo::GetCurSel(state->hMacroList);

                    if (state->currentMode == MacroComboMode::Macro)
                    {
                        state->selectedMacroId = sel >= 0 ? sel : -1;
                        MacroSubpage_RefreshUI(hWnd);
                    }
                    else // Combo mode
                    {
                        state->selectedComboId = (sel >= 0 && sel < (int)state->comboListIds.size()) ? state->comboListIds[sel] : -1;
                        if (state->selectedComboId >= 0 && state->hComboStatusLabel)
                        {
                            MouseCombo* combo = MouseComboSystem::GetCombo(state->selectedComboId);
                            if (combo)
                            {
                                std::wstring status = L"Selection: " + combo->name;
                                SetWindowTextW(state->hComboStatusLabel, status.c_str());
                            }
                        }
                        else if (state->hComboStatusLabel)
                        {
                            SetWindowTextW(state->hComboStatusLabel, L"No combo selected");
                        }
                        MacroSubpage_UpdateComboUI(hWnd);
                    }
                }
                break;

            case COMBO_ID_ACTION_TYPE:
                if (HIWORD(wParam) == CBN_SELCHANGE)
                {
                    int idx = PremiumCombo::GetCurSel(state->hActionType);
                    // Update Action label to reflect selection
                    if (state->hActionLabel)
                    {
                        const wchar_t* typeTxt = L"Action:";
                        switch (idx)
                        {
                        case 0: typeTxt = L"Action: Appuyer touche"; break;
                        case 1: typeTxt = L"Action: Relâcher touche"; break;
                        case 2: typeTxt = L"Action: Appuyer+Relâcher"; break;
                        case 3: typeTxt = L"Action: Taper texte"; break;
                        case 4: typeTxt = L"Action: Clic souris"; break;
                        case 5: typeTxt = L"Action: Attendre (ms)"; break;
                        }
                        SetWindowTextW(state->hActionLabel, typeTxt);
                    }
                    if (idx == 0 || idx == 1 || idx == 2) // key-based actions
                        SetWindowTextW(state->hActionKey, L"P");
                    else if (idx == 3) // TypeText
                        SetWindowTextW(state->hActionKey, L"Texte...");
                    else if (idx == 4) // MouseClick
                        SetWindowTextW(state->hActionKey, L"L/R/M");
                    else if (idx == 5) // Delay
                        SetWindowTextW(state->hActionKey, L"100");
                }
                break;

            case MACRO_ID_RECORD_START:
                if (state->selectedMacroId >= 0)
                {
                    MacroSystem::StartRecording(state->selectedMacroId);
                    MacroSubpage_RefreshUI(hWnd);
                }
                break;

            case MACRO_ID_RECORD_STOP:
                MacroSystem::StopRecording();
                MacroSubpage_RefreshUI(hWnd);
                break;

            case MACRO_ID_PLAY_START:
                if (state->selectedMacroId >= 0)
                {
                    MacroSystem::StartPlayback(state->selectedMacroId);
                    MacroSubpage_RefreshUI(hWnd);
                }
                break;

            case MACRO_ID_PLAY_STOP:
                MacroSystem::StopPlayback();
                MacroSubpage_RefreshUI(hWnd);
                break;

            case MACRO_ID_DELETE_MACRO:
                if (state->selectedMacroId >= 0)
                {
                    MacroSystem::DeleteMacro(state->selectedMacroId);
                    state->selectedMacroId = -1;
                    MacroSubpage_RefreshUI(hWnd);
                }
                break;

            case MACRO_ID_LOOP_CHECKBOX:
                if (state->selectedMacroId >= 0)
                {
                    bool checked = SendMessageW(state->hLoopCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    MacroSystem::SetLoopMacro(state->selectedMacroId, checked);
                }
                break;

            case MACRO_ID_BLOCK_KEYS_CHECKBOX:
                if (state->selectedMacroId >= 0)
                {
                    bool checked = SendMessageW(state->hBlockKeysCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    MacroSystem::SetBlockKeysDuringPlayback(checked);
                }
                break;

            case MACRO_ID_EDIT_TIMING:
                if (state->selectedMacroId >= 0)
                {
                    MacroEditor::OpenEditor(hWnd, state->selectedMacroId);
                }
                break;

            case MACRO_ID_EXPORT_JSON:
            {
                wchar_t filePath[MAX_PATH] = L"";
                OPENFILENAMEW ofn = {};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hWnd;
                ofn.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
                ofn.lpstrFile = filePath;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_OVERWRITEPROMPT;
                ofn.lpstrDefExt = L"json";
                ofn.lpstrTitle = L"Exporter les macros";

                std::wstring exeDir = GetExeDir();
                ofn.lpstrInitialDir = exeDir.c_str();

                if (GetSaveFileNameW(&ofn))
                {
                    if (MacroJsonStorage::SaveToJson(filePath))
                    {
                        MessageBoxW(hWnd, L"Macros exportees avec succes!",
                            L"Export", MB_OK | MB_ICONINFORMATION);
                    }
                    else
                    {
                        MessageBoxW(hWnd, L"Erreur lors de l'export!",
                            L"Erreur", MB_OK | MB_ICONERROR);
                    }
                }
            }
            break;

            case MACRO_ID_IMPORT_JSON:
            {
                wchar_t filePath[MAX_PATH] = L"";
                OPENFILENAMEW ofn = {};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hWnd;
                ofn.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
                ofn.lpstrFile = filePath;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST;
                ofn.lpstrTitle = L"Importer des macros";

                std::wstring exeDir = GetExeDir();
                ofn.lpstrInitialDir = exeDir.c_str();

                if (GetOpenFileNameW(&ofn))
                {
                    if (MacroJsonStorage::LoadFromJson(filePath))
                    {
                        MacroSubpage_RefreshUI(hWnd);
                        MessageBoxW(hWnd, L"Macros importees avec succes!",
                            L"Import", MB_OK | MB_ICONINFORMATION);
                    }
                    else
                    {
                        MessageBoxW(hWnd, L"Erreur lors de l'import!",
                            L"Erreur", MB_OK | MB_ICONERROR);
                    }
                }
            }
            break;

            // NEW: Combo-specific controls
            case COMBO_ID_TRIGGER_LIST:
                if (HIWORD(wParam) == CBN_SELCHANGE && state->selectedComboId >= 0)
                {
                    int triggerIndex = PremiumCombo::GetCurSel(state->hTriggerList);
                    MouseCombo* combo = MouseComboSystem::GetCombo(state->selectedComboId);
                    if (combo && triggerIndex >= 0)
                    {
                        combo->trigger = (ComboTriggerType)(triggerIndex + 1);
                        MacroSubpage_UpdateComboUI(hWnd);
                    }
                }
                break;

            case COMBO_ID_REPEAT_CHECKBOX:
                if (state->selectedComboId >= 0)
                {
                    bool checked = Button_GetCheck(state->hRepeatCheckbox) == BST_CHECKED;
                    MouseCombo* combo = MouseComboSystem::GetCombo(state->selectedComboId);
                    if (combo)
                    {
                        combo->repeatWhileHeld = checked;
                    }
                }
                break;

            case COMBO_ID_ADD_ACTION:
                if (state->selectedComboId >= 0)
                {
                    int actionTypeIndex = PremiumCombo::GetCurSel(state->hActionType);
                    if (actionTypeIndex == CB_ERR) break;

                    wchar_t keyBuffer[64] = {};
                    GetWindowTextW(state->hActionKey, keyBuffer, 63);
                    // trim spaces (simple)
                    int start = 0, end = (int)wcslen(keyBuffer) - 1;
                    while (start <= end && iswspace(keyBuffer[start])) start++;
                    while (end >= start && iswspace(keyBuffer[end])) end--;
                    if (start > end) keyBuffer[0] = L'\0';
                    else {
                        keyBuffer[end + 1] = L'\0';
                        if (start > 0) wmemmove(keyBuffer, keyBuffer + start, end - start + 2);
                    }

                    ComboAction action = {};

                    switch (actionTypeIndex)
                    {
                    case 0: // Press Key
                        action.type = ComboActionType::PressKey;
                        if (keyBuffer[0]) action.keyHid = VkToHid(towupper(keyBuffer[0]));
                        break;
                    case 1: // Release Key
                        action.type = ComboActionType::ReleaseKey;
                        if (keyBuffer[0]) action.keyHid = VkToHid(towupper(keyBuffer[0]));
                        break;
                    case 2: // Tap Key
                        action.type = ComboActionType::TapKey;
                        if (keyBuffer[0]) action.keyHid = VkToHid(towupper(keyBuffer[0]));
                        break;
                    case 3: // Type Text
                        action.type = ComboActionType::TypeText;
                        action.text = keyBuffer;
                        break;
                    case 4: // Mouse Click
                        action.type = ComboActionType::MouseClick;
                        {
                            int btn = 0;
                            if (!keyBuffer[0]) btn = 1;
                            else
                            {
                                wchar_t c = towupper(keyBuffer[0]);
                                if (c == L'L') btn = 1;
                                else if (c == L'R') btn = 2;
                                else if (c == L'M') btn = 3;
                                else btn = _wtoi(keyBuffer);
                                if (btn <= 0) btn = 1;
                            }
                            action.mouseButton = btn;
                        }
                        break;
                    case 5: // Delay
                        action.type = ComboActionType::Delay;
                        {
                            int d = _wtoi(keyBuffer);
                            if (d < 10) d = 10;
                            if (d > 1000) d = 1000;
                            action.delayMs = d;
                        }
                        break;
                    }

                    if (action.type != ComboActionType::None)
                    {
                        MouseComboSystem::AddAction(state->selectedComboId, action);
                        MacroSubpage_UpdateComboUI(hWnd);
                        // Update info labels
                        MouseCombo* combo = MouseComboSystem::GetCombo(state->selectedComboId);
                        if (combo)
                        {
                            if (state->hActionCountLabel)
                            {
                                std::wstring countText = L"Actions configurees : " + std::to_wstring(combo->actions.size());
                                SetWindowTextW(state->hActionCountLabel, countText.c_str());
                            }
                            if (state->hLastActionLabel)
                            {
                                auto HidToLabel = [&](uint16_t hid)->std::wstring
                                    {
                                        if (hid == 0) return std::wstring(L"(none)");
                                        WORD vk = HidToVk(hid);
                                        if (vk >= 'A' && vk <= 'Z') { wchar_t s[2] = { (wchar_t)vk, 0 }; return std::wstring(s); }
                                        if (vk >= '0' && vk <= '9') { wchar_t s[2] = { (wchar_t)vk, 0 }; return std::wstring(s); }
                                        UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
                                        LONG lParam = (sc << 16);
                                        wchar_t name[64] = {};
                                        if (GetKeyNameTextW(lParam, name, (int)std::size(name)) > 0)
                                            return std::wstring(name);
                                        return std::to_wstring(hid);
                                    };

                                std::wstring lastText = L"Derniere action : ";
                                const auto& a = combo->actions.back();
                                switch (a.type)
                                {
                                case ComboActionType::PressKey:    lastText += L"Press " + HidToLabel(a.keyHid); break;
                                case ComboActionType::ReleaseKey:  lastText += L"Release " + HidToLabel(a.keyHid); break;
                                case ComboActionType::TapKey:      lastText += L"Tap " + HidToLabel(a.keyHid); break;
                                case ComboActionType::TypeText:    lastText += L"Texte"; break;
                                case ComboActionType::MouseClick:  lastText += L"Click " + std::to_wstring(a.mouseButton); break;
                                case ComboActionType::Delay:       lastText += L"Delay " + std::to_wstring(a.delayMs) + L"ms"; break;
                                default:                           lastText += L"Inconnu"; break;
                                }
                                SetWindowTextW(state->hLastActionLabel, lastText.c_str());
                            }
                            // Select the last action in listbox
                            if (state->hComboActionsList)
                            {
                                int count = (int)SendMessageW(state->hComboActionsList, LB_GETCOUNT, 0, 0);
                                if (count > 0)
                                    SendMessageW(state->hComboActionsList, LB_SETCURSEL, count - 1, 0);
                            }
                        }
                    }
                }
                break;

            case COMBO_ID_ADD_DELAY:
                if (state->selectedComboId >= 0)
                {
                    ComboAction action = {};
                    action.type = ComboActionType::Delay;
                    action.delayMs = 100; // Default 100ms
                    MouseComboSystem::AddAction(state->selectedComboId, action);
                    MacroSubpage_UpdateComboUI(hWnd);

                    // Update count label
                    MouseCombo* combo = MouseComboSystem::GetCombo(state->selectedComboId);
                    if (combo && state->hActionCountLabel)
                    {
                        std::wstring countText = L"Actions configurees : " + std::to_wstring(combo->actions.size());
                        SetWindowTextW(state->hActionCountLabel, countText.c_str());
                    }
                }
                break;

            case COMBO_ID_REMOVE_ACTION:
                if (state->selectedComboId >= 0)
                {
                    MouseCombo* combo = MouseComboSystem::GetCombo(state->selectedComboId);
                    if (combo && !combo->actions.empty())
                    {
                        int selIdx = -1;
                        if (state->hComboActionsList)
                            selIdx = (int)SendMessageW(state->hComboActionsList, LB_GETCURSEL, 0, 0);
                        if (selIdx >= 0 && selIdx < (int)combo->actions.size())
                            combo->actions.erase(combo->actions.begin() + selIdx);
                        else
                            combo->actions.pop_back();
                        MacroSubpage_UpdateComboUI(hWnd);
                        if (state->hActionCountLabel)
                        {
                            std::wstring countText = L"Actions configurees : " + std::to_wstring(combo->actions.size());
                            SetWindowTextW(state->hActionCountLabel, countText.c_str());
                        }
                        if (state->hLastActionLabel)
                        {
                            std::wstring lastText = combo->actions.empty() ? L"Last Action: None" : L"Last Action: Modified";
                            SetWindowTextW(state->hLastActionLabel, lastText.c_str());
                        }
                    }
                }
                break;

            case COMBO_ID_CLEAR_ACTIONS:
                if (state->selectedComboId >= 0)
                {
                    MouseComboSystem::ClearActions(state->selectedComboId);
                    MacroSubpage_UpdateComboUI(hWnd);
                    if (state->hActionCountLabel)
                        SetWindowTextW(state->hActionCountLabel, L"Configured Actions: 0");
                    if (state->hLastActionLabel)
                        SetWindowTextW(state->hLastActionLabel, L"Last Action: None");
                    if (state->hComboActionsList)
                        SendMessageW(state->hComboActionsList, LB_RESETCONTENT, 0, 0);
                }
                break;

            case COMBO_ID_EXPORT_JSON:
            {
                std::wstring iniDir = GetExeDir();
                std::wstring defPath = iniDir + L"\\combos.json";
                wchar_t path[MAX_PATH]{};
                wcsncpy_s(path, defPath.c_str(), _TRUNCATE);
                OPENFILENAMEW ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hWnd;
                ofn.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
                ofn.lpstrFile = path;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
                ofn.lpstrInitialDir = iniDir.c_str();
                if (GetSaveFileNameW(&ofn))
                {
                    if (MouseComboSystem::SaveToFile(path))
                    {
                        MessageBoxW(hWnd, L"Export reussi.", L"Export", MB_OK | MB_ICONINFORMATION);
                    }
                    else
                    {
                        MessageBoxW(hWnd, L"Echec de l'export.", L"Export", MB_OK | MB_ICONERROR);
                    }
                }
            }
            break;

            case COMBO_ID_IMPORT_JSON:
            {
                std::wstring iniDir = GetExeDir();
                wchar_t path[MAX_PATH]{};
                OPENFILENAMEW ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hWnd;
                ofn.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
                ofn.lpstrFile = path;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                ofn.lpstrInitialDir = iniDir.c_str();
                if (GetOpenFileNameW(&ofn))
                {
                    if (MouseComboSystem::LoadFromFile(path))
                    {
                        MacroSubpage_UpdateComboList(hWnd);
                        // Try to keep selection on first item
                        PremiumCombo::SetCurSel(state->hMacroList, 0);
                        int sel = PremiumCombo::GetCurSel(state->hMacroList);
                        state->selectedComboId = (sel >= 0 && sel < (int)state->comboListIds.size()) ? state->comboListIds[sel] : -1;
                        MacroSubpage_UpdateComboUI(hWnd);
                        MessageBoxW(hWnd, L"Import reussi.", L"Import", MB_OK | MB_ICONINFORMATION);
                    }
                    else
                    {
                        MessageBoxW(hWnd, L"Echec de l'import.", L"Import", MB_OK | MB_ICONERROR);
                    }
                }
            }
            break;
            }
            break;
        }
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rcClient;
        GetClientRect(hWnd, &rcClient);

        // FIX: Utiliser ps.rcPaint au lieu de rcClient pour ne peindre
        // que la zone invalidee, evite le debordement sur les onglets adjacents
        FillRect(hdc, &ps.rcPaint, UiTheme::Brush_PanelBg());

        // Draw grouping panels for Combo mode
        if (state && state->currentMode == MacroComboMode::Combo) {
            auto GetRect = [&](HWND c) -> RECT {
                RECT r = {};
                if (c && IsWindowVisible(c)) {
                    GetWindowRect(c, &r);
                    MapWindowPoints(nullptr, hWnd, (LPPOINT)&r, 2);
                }
                return r;
                };

            // Trigger Panel
            RECT rTrigger = GetRect(state->hTriggerLabel);
            RECT rDelay = GetRect(state->hDelaySlider);
            if (!IsRectEmpty(&rTrigger) && !IsRectEmpty(&rDelay)) {
                RECT rPanel = rTrigger;
                rPanel.right = rcClient.right - S(hWnd, 10); // Use full width minus margin
                rPanel.bottom = rDelay.bottom + S(hWnd, 10);
                rPanel.top -= S(hWnd, 8);
                rPanel.left -= S(hWnd, 8);

                HBRUSH br = CreateSolidBrush(UiTheme::Color_ControlBg());
                HPEN pen = CreatePen(PS_SOLID, 1, UiTheme::Color_Border());
                HGDIOBJ oldBr = SelectObject(hdc, br);
                HGDIOBJ oldPen = SelectObject(hdc, pen);
                RoundRect(hdc, rPanel.left, rPanel.top, rPanel.right, rPanel.bottom, S(hWnd, 10), S(hWnd, 10));
                SelectObject(hdc, oldPen);
                SelectObject(hdc, oldBr);
                DeleteObject(pen);
                DeleteObject(br);
            }

            // Action Panel
            RECT rAction = GetRect(state->hActionLabel);
            RECT rImport = GetRect(state->hComboImportBtn);
            if (!IsRectEmpty(&rAction) && !IsRectEmpty(&rImport)) {
                RECT rPanel = rAction;
                rPanel.right = rcClient.right - S(hWnd, 10);
                rPanel.bottom = rImport.bottom + S(hWnd, 10);
                rPanel.top -= S(hWnd, 8);
                rPanel.left -= S(hWnd, 8);

                HBRUSH br = CreateSolidBrush(UiTheme::Color_ControlBg());
                HPEN pen = CreatePen(PS_SOLID, 1, UiTheme::Color_Border());
                HGDIOBJ oldBr = SelectObject(hdc, br);
                HGDIOBJ oldPen = SelectObject(hdc, pen);
                RoundRect(hdc, rPanel.left, rPanel.top, rPanel.right, rPanel.bottom, S(hWnd, 10), S(hWnd, 10));
                SelectObject(hdc, oldPen);
                SelectObject(hdc, oldBr);
                DeleteObject(pen);
                DeleteObject(br);
            }
        }

        EndPaint(hWnd, &ps);
        return 0;
    }

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    return 0;
}


// ============================================================================
// Actions List Update - NEW FUNCTION
// ============================================================================

void MacroSubpage_UpdateActionsList(HWND hWnd)
{
    MacroUIState* state = (MacroUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state || !state->hActionsList) return;

    // If no macro selected, clear list
    if (state->selectedMacroId < 0)
    {
        SetWindowTextW(state->hActionsList, L"No macro selected");
        return;
    }

    Macro* macro = MacroSystem::GetMacro(state->selectedMacroId);
    if (!macro)
    {
        SetWindowTextW(state->hActionsList, L"Error: macro not found");
        return;
    }

    // Check if number of actions has changed
    if ((int)macro->actions.size() == state->lastActionCount && !MacroSystem::IsRecording())
        return;  // No change, no need to update

    state->lastActionCount = (int)macro->actions.size();

    // Build list text
    std::wstring text;

    if (macro->actions.empty())
    {
        text = L"No recorded actions.\r\n\r\n";
        text += L"Click 'Record' to start.\r\n";
    }
    else
    {
        wchar_t header[256];
        swprintf_s(header, L"=== %zu recorded actions ===\r\n\r\n", macro->actions.size());
        text += header;

        uint32_t lastTimestamp = 0;

        for (size_t i = 0; i < macro->actions.size(); ++i)
        {
            const MacroAction& action = macro->actions[i];

            wchar_t line[512];
            float seconds = action.timestamp / 1000.0f;
            float deltaSeconds = (action.timestamp - lastTimestamp) / 1000.0f;

            // Afficher le numéro d'action et le temps
            swprintf_s(line, L"[%3zu] %06.3fs", i + 1, seconds);
            text += line;

            // Afficher le delta si ce n'est pas la première action
            if (i > 0)
            {
                swprintf_s(line, L" (+%.3fs)", deltaSeconds);
                text += line;
            }

            text += L" - ";

            // Afficher le type d'action
            switch (action.type)
            {
            case MacroActionType::MouseLeftDown:
                swprintf_s(line, L"Clic GAUCHE DOWN a (%d, %d)",
                    action.mousePos.x, action.mousePos.y);
                break;
            case MacroActionType::MouseLeftUp:
                swprintf_s(line, L"Clic GAUCHE UP   a (%d, %d)",
                    action.mousePos.x, action.mousePos.y);
                break;
            case MacroActionType::MouseRightDown:
                swprintf_s(line, L"Clic DROIT DOWN  a (%d, %d)",
                    action.mousePos.x, action.mousePos.y);
                break;
            case MacroActionType::MouseRightUp:
                swprintf_s(line, L"Clic DROIT UP    a (%d, %d)",
                    action.mousePos.x, action.mousePos.y);
                break;
            case MacroActionType::MouseMiddleDown:
                swprintf_s(line, L"Clic MILIEU DOWN a (%d, %d)",
                    action.mousePos.x, action.mousePos.y);
                break;
            case MacroActionType::MouseMiddleUp:
                swprintf_s(line, L"Clic MILIEU UP   a (%d, %d)",
                    action.mousePos.x, action.mousePos.y);
                break;
            case MacroActionType::MouseWheelUp:
                swprintf_s(line, L"Molette HAUT");
                break;
            case MacroActionType::MouseWheelDown:
                swprintf_s(line, L"Molette BAS");
                break;
            case MacroActionType::KeyPress:
                swprintf_s(line, L"Touche PRESSEE  (HID: 0x%04X)", action.hid);
                break;
            case MacroActionType::KeyRelease:
                swprintf_s(line, L"Touche RELACHEE (HID: 0x%04X)", action.hid);
                break;
            case MacroActionType::Delay:
                swprintf_s(line, L"Delai de %u ms", action.delayMs);
                break;
            default:
                swprintf_s(line, L"Action inconnue (type: %d)", (int)action.type);
                break;
            }

            text += line;
            text += L"\r\n";

            lastTimestamp = action.timestamp;
        }

        // Ajouter un résumé
        text += L"\r\n";
        wchar_t summary[256];
        swprintf_s(summary, L"=== Duree totale: %.3f secondes ===",
            macro->totalDuration / 1000.0f);
        text += summary;
    }

    // Mettre à jour le contrôle
    SetWindowTextW(state->hActionsList, text.c_str());

    // Scroller vers le bas si en enregistrement
    if (MacroSystem::IsRecording())
    {
        int lineCount = (int)SendMessageW(state->hActionsList, EM_GETLINECOUNT, 0, 0);
        SendMessageW(state->hActionsList, EM_LINESCROLL, 0, lineCount);
    }
}

// ============================================================================
// NEW: Combo-specific functions
// ============================================================================

void MacroSubpage_SwitchMode(HWND hWnd, MacroComboMode newMode)
{
    MacroUIState* state = (MacroUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state) return;

    // Update mode
    state->currentMode = newMode;
    if (state->hModeCombo)
        PremiumCombo::SetCurSel(state->hModeCombo, (newMode == MacroComboMode::Combo) ? 0 : 1);
    if (state->hNewMacroBtn)
        SetWindowTextW(state->hNewMacroBtn, (newMode == MacroComboMode::Combo) ? L"\uFF0B New Combo" : L"\uFF0B New Macro");

    // Safe approach: Show/hide controls instead of destroying them
    if (newMode == MacroComboMode::Macro)
    {
        // Show macro controls
        if (state->hRecordButton) ShowWindow(state->hRecordButton, SW_SHOW);
        if (state->hPlayButton) ShowWindow(state->hPlayButton, SW_SHOW);
        if (state->hLoopCheckbox) ShowWindow(state->hLoopCheckbox, SW_SHOW);
        if (state->hDeleteMacroBtn) ShowWindow(state->hDeleteMacroBtn, SW_SHOW);
        if (state->hBlockKeysCheckbox) ShowWindow(state->hBlockKeysCheckbox, SW_SHOW);
        if (state->hSpeedSlider) ShowWindow(state->hSpeedSlider, SW_SHOW);
        if (state->hEditTimingBtn) ShowWindow(state->hEditTimingBtn, SW_SHOW);
        if (state->hExportJsonBtn) ShowWindow(state->hExportJsonBtn, SW_SHOW);
        if (state->hImportJsonBtn) ShowWindow(state->hImportJsonBtn, SW_SHOW);
        if (state->hActionsList) ShowWindow(state->hActionsList, SW_SHOW);
        if (state->hEmergencyStopBtn)
        {
            ShowWindow(state->hEmergencyStopBtn, SW_SHOW);
            // Restore position if needed, or keep it at bottom
        }
        if (state->hOptionsLabel) ShowWindow(state->hOptionsLabel, SW_SHOW);
        if (state->hSpeedLabel) ShowWindow(state->hSpeedLabel, SW_SHOW);
        if (state->hSpeedValueLabel) ShowWindow(state->hSpeedValueLabel, SW_SHOW);
        if (state->hStatusText) ShowWindow(state->hStatusText, SW_SHOW);
        if (state->hActionsRecordedLabel) ShowWindow(state->hActionsRecordedLabel, SW_SHOW);

        // Hide combo controls
        if (state->hTriggerLabel) ShowWindow(state->hTriggerLabel, SW_HIDE);
        if (state->hTriggerList) ShowWindow(state->hTriggerList, SW_HIDE);
        if (state->hComboStatusLabel) ShowWindow(state->hComboStatusLabel, SW_HIDE);
        if (state->hRepeatCheckbox) ShowWindow(state->hRepeatCheckbox, SW_HIDE);
        if (state->hDelayLabel) ShowWindow(state->hDelayLabel, SW_HIDE);
        if (state->hDelaySlider) ShowWindow(state->hDelaySlider, SW_HIDE);
        if (state->hActionLabel) ShowWindow(state->hActionLabel, SW_HIDE);
        if (state->hActionType) ShowWindow(state->hActionType, SW_HIDE);
        if (state->hActionKey) ShowWindow(state->hActionKey, SW_HIDE);
        if (state->hAddActionButton) ShowWindow(state->hAddActionButton, SW_HIDE);
        if (state->hRemoveActionButton) ShowWindow(state->hRemoveActionButton, SW_HIDE);
        if (state->hActionCountLabel) ShowWindow(state->hActionCountLabel, SW_HIDE);
        if (state->hLastActionLabel) ShowWindow(state->hLastActionLabel, SW_HIDE);
        if (state->hComboActionsList) ShowWindow(state->hComboActionsList, SW_HIDE);
        if (state->hClearActionsButton) ShowWindow(state->hClearActionsButton, SW_HIDE);
        if (state->hComboExportBtn) ShowWindow(state->hComboExportBtn, SW_HIDE);
        if (state->hComboImportBtn) ShowWindow(state->hComboImportBtn, SW_HIDE);
        HWND hDelayBtn = GetDlgItem(hWnd, COMBO_ID_ADD_DELAY);
        if (hDelayBtn) ShowWindow(hDelayBtn, SW_HIDE);

        // Update list text
        if (state->hActionsList) SetWindowTextW(state->hActionsList, L"Select a macro...");
    }
    else // Combo mode
    {
        // Hide macro controls
        if (state->hRecordButton) ShowWindow(state->hRecordButton, SW_HIDE);
        if (state->hPlayButton) ShowWindow(state->hPlayButton, SW_HIDE);
        if (state->hLoopCheckbox) ShowWindow(state->hLoopCheckbox, SW_HIDE);
        if (state->hDeleteMacroBtn) ShowWindow(state->hDeleteMacroBtn, SW_HIDE);
        if (state->hBlockKeysCheckbox) ShowWindow(state->hBlockKeysCheckbox, SW_HIDE);
        if (state->hSpeedSlider) ShowWindow(state->hSpeedSlider, SW_HIDE);
        if (state->hSpeedValueLabel) ShowWindow(state->hSpeedValueLabel, SW_HIDE);
        if (state->hEditTimingBtn) ShowWindow(state->hEditTimingBtn, SW_HIDE);
        if (state->hExportJsonBtn) ShowWindow(state->hExportJsonBtn, SW_HIDE);
        if (state->hImportJsonBtn) ShowWindow(state->hImportJsonBtn, SW_HIDE);
        if (state->hActionsList) ShowWindow(state->hActionsList, SW_HIDE);
        // En mode Combo, garder le bouton d'urgence visible mais le remonter sous la liste des combos
        if (state->hEmergencyStopBtn)
        {
            ShowWindow(state->hEmergencyStopBtn, SW_SHOW);
            RECT rcList{};
            if (state->hMacroList)
            {
                GetWindowRect(state->hMacroList, &rcList);
                MapWindowPoints(nullptr, hWnd, (LPPOINT)&rcList, 2);
                int newY = rcList.bottom + S(hWnd, 8);
                int w = state->emergencyStopOrigRect.right - state->emergencyStopOrigRect.left;
                int h = state->emergencyStopOrigRect.bottom - state->emergencyStopOrigRect.top;
                SetWindowPos(state->hEmergencyStopBtn, nullptr,
                    state->emergencyStopOrigRect.left, newY, w, h,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        if (state->hOptionsLabel) ShowWindow(state->hOptionsLabel, SW_HIDE);
        if (state->hSpeedLabel) ShowWindow(state->hSpeedLabel, SW_HIDE);
        if (state->hStatusText) ShowWindow(state->hStatusText, SW_HIDE);
        if (state->hActionsRecordedLabel) ShowWindow(state->hActionsRecordedLabel, SW_HIDE);

        // Show combo controls
        if (state->hTriggerLabel) ShowWindow(state->hTriggerLabel, SW_SHOW);
        if (state->hTriggerList) ShowWindow(state->hTriggerList, SW_SHOW);
        if (state->hComboStatusLabel) ShowWindow(state->hComboStatusLabel, SW_HIDE); // Hide redundant status label
        if (state->hRepeatCheckbox) ShowWindow(state->hRepeatCheckbox, SW_SHOW);
        if (state->hDelayLabel) ShowWindow(state->hDelayLabel, SW_SHOW);
        if (state->hDelaySlider) ShowWindow(state->hDelaySlider, SW_SHOW);
        if (state->hActionLabel) ShowWindow(state->hActionLabel, SW_SHOW);
        if (state->hActionType) ShowWindow(state->hActionType, SW_SHOW);
        if (state->hActionKey) ShowWindow(state->hActionKey, SW_SHOW);
        if (state->hAddActionButton) ShowWindow(state->hAddActionButton, SW_SHOW);
        if (state->hRemoveActionButton) ShowWindow(state->hRemoveActionButton, SW_SHOW);
        if (state->hActionCountLabel) ShowWindow(state->hActionCountLabel, SW_SHOW);
        if (state->hLastActionLabel) ShowWindow(state->hLastActionLabel, SW_SHOW);
        if (state->hComboActionsList) ShowWindow(state->hComboActionsList, SW_SHOW);
        if (state->hClearActionsButton) ShowWindow(state->hClearActionsButton, SW_SHOW);
        if (state->hComboExportBtn) ShowWindow(state->hComboExportBtn, SW_SHOW);
        if (state->hComboImportBtn) ShowWindow(state->hComboImportBtn, SW_SHOW);
        HWND hDelayBtn = GetDlgItem(hWnd, COMBO_ID_ADD_DELAY);
        if (hDelayBtn) ShowWindow(hDelayBtn, SW_SHOW);

        // Update list text and refresh combo list
        if (state->hActionsList)
        {
            SetWindowTextW(state->hActionsList, L"Configuration des combos de souris...");
            MacroSubpage_UpdateComboList(hWnd);
        }

        // Relayout combo controls
        if (state->hMacroList)
        {
            RECT rcList{};
            GetWindowRect(state->hMacroList, &rcList);
            MapWindowPoints(nullptr, hWnd, (LPPOINT)&rcList, 2);
            int yStep = rcList.bottom + S(hWnd, 12); // Start right after list

            auto moveCtl = [&](HWND ctl)
                {
                    if (!ctl) return;
                    RECT r{};
                    GetWindowRect(ctl, &r);
                    MapWindowPoints(nullptr, hWnd, (LPPOINT)&r, 2);
                    int x = r.left;
                    int w = r.right - r.left;
                    int h = r.bottom - r.top;
                    SetWindowPos(ctl, nullptr, x, yStep, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
                    yStep += h + S(hWnd, 8);
                };
            moveCtl(state->hTriggerLabel);
            moveCtl(state->hTriggerList);
            // moveCtl(state->hComboStatusLabel); // Skip status label
            // place repeat checkbox and delay label/slider on same row baseline
            if (state->hRepeatCheckbox || state->hDelayLabel || state->hDelaySlider)
            {
                int rowY = yStep;
                if (state->hRepeatCheckbox)
                {
                    RECT r{};
                    GetWindowRect(state->hRepeatCheckbox, &r);
                    MapWindowPoints(nullptr, hWnd, (LPPOINT)&r, 2);
                    SetWindowPos(state->hRepeatCheckbox, HWND_TOP, r.left, rowY, r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE);
                }
                if (state->hDelayLabel)
                {
                    RECT r{};
                    GetWindowRect(state->hDelayLabel, &r);
                    MapWindowPoints(nullptr, hWnd, (LPPOINT)&r, 2);
                    SetWindowPos(state->hDelayLabel, HWND_TOP, r.left, rowY, r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE);
                }
                if (state->hDelaySlider)
                {
                    RECT rLabel{};
                    GetWindowRect(state->hDelayLabel, &rLabel);
                    MapWindowPoints(nullptr, hWnd, (LPPOINT)&rLabel, 2);
                    RECT rSlider{};
                    GetWindowRect(state->hDelaySlider, &rSlider);
                    MapWindowPoints(nullptr, hWnd, (LPPOINT)&rSlider, 2);
                    // Place slider just below the label
                    SetWindowPos(state->hDelaySlider, HWND_TOP, rSlider.left, rowY + (rLabel.bottom - rLabel.top) + S(hWnd, 4), rSlider.right - rSlider.left, rSlider.bottom - rSlider.top, SWP_NOACTIVATE);
                    yStep = rowY + (rLabel.bottom - rLabel.top) + (rSlider.bottom - rSlider.top) + S(hWnd, 24); // Increased gap for panel
                }
                else
                {
                    yStep = rowY + S(hWnd, 30);
                }
            }
            moveCtl(state->hActionLabel);
            moveCtl(state->hActionType);
            moveCtl(state->hActionKey);

            // add/delay/remove buttons baseline - align with Action Key
            HWND hDelay = GetDlgItem(hWnd, COMBO_ID_ADD_DELAY);
            if (state->hAddActionButton || state->hRemoveActionButton || hDelay)
            {
                // They are on the same row as Action Key in OnCreate, but let's ensure they move together
                // We moved hActionKey above. Let's get its Y.
                RECT rKey{};
                GetWindowRect(state->hActionKey, &rKey);
                MapWindowPoints(nullptr, hWnd, (LPPOINT)&rKey, 2);
                int rowY = rKey.top;
                int rowH = 0;
                int currentX = 0;

                if (state->hAddActionButton)
                {
                    RECT r{};
                    GetWindowRect(state->hAddActionButton, &r);
                    MapWindowPoints(nullptr, hWnd, (LPPOINT)&r, 2);
                    SetWindowPos(state->hAddActionButton, nullptr, r.left, rowY, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
                    rowH = std::max(rowH, (int)(r.bottom - r.top));
                    currentX = r.right + S(hWnd, 8);
                }

                if (hDelay)
                {
                    RECT r{};
                    GetWindowRect(hDelay, &r);
                    // Keep width/height, update pos
                    SetWindowPos(hDelay, nullptr, currentX, rowY, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
                    rowH = std::max(rowH, (int)(r.bottom - r.top));
                    currentX += (r.right - r.left) + S(hWnd, 8);
                }

                if (state->hRemoveActionButton)
                {
                    RECT r{};
                    GetWindowRect(state->hRemoveActionButton, &r);
                    MapWindowPoints(nullptr, hWnd, (LPPOINT)&r, 2);
                    SetWindowPos(state->hRemoveActionButton, nullptr, currentX, rowY, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
                    rowH = std::max(rowH, (int)(r.bottom - r.top));
                }
                yStep = rowY + rowH + S(hWnd, 12);
            }
            moveCtl(state->hActionCountLabel);
            moveCtl(state->hLastActionLabel);
            moveCtl(state->hComboActionsList);
            // place clear/export/import buttons on same baseline row
            if (state->hClearActionsButton || state->hComboExportBtn || state->hComboImportBtn)
            {
                int rowY = yStep;
                int rowH = 0;
                auto placeOnRow = [&](HWND ctl)
                    {
                        if (!ctl) return;
                        RECT r{};
                        GetWindowRect(ctl, &r);
                        MapWindowPoints(nullptr, hWnd, (LPPOINT)&r, 2);
                        // keep original X, align Y to row baseline
                        SetWindowPos(ctl, nullptr, r.left, rowY, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
                        rowH = std::max(rowH, (int)(r.bottom - r.top));
                    };
                placeOnRow(state->hClearActionsButton);
                placeOnRow(state->hComboExportBtn);
                placeOnRow(state->hComboImportBtn);
                yStep = rowY + rowH + S(hWnd, 12);
            }

            // Place Emergency Stop at the very bottom
            if (state->hEmergencyStopBtn)
            {
                ShowWindow(state->hEmergencyStopBtn, SW_SHOW);
                int w = state->emergencyStopOrigRect.right - state->emergencyStopOrigRect.left;
                int h = state->emergencyStopOrigRect.bottom - state->emergencyStopOrigRect.top;
                SetWindowPos(state->hEmergencyStopBtn, nullptr,
                    state->emergencyStopOrigRect.left, yStep + S(hWnd, 10), w, h,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
    }

    // Show/hide STATIC labels by iterating through all child windows
    HWND hChild = GetWindow(hWnd, GW_CHILD);
    while (hChild)
    {
        HWND hNext = GetWindow(hChild, GW_HWNDNEXT);
        wchar_t className[256];
        wchar_t text[256];
        GetClassNameW(hChild, className, 256);
        GetWindowTextW(hChild, text, 256);

        if (wcscmp(className, L"STATIC") == 0)
        {
            if (newMode == MacroComboMode::Macro)
            {
                // Show macro-related labels
                if (wcsstr(text, L"Options:") != nullptr ||
                    wcsstr(text, L"Speed") != nullptr ||
                    wcsstr(text, L"Status:") != nullptr ||
                    wcsstr(text, L"Recorded Actions") != nullptr ||
                    wcsstr(text, L"EMERGENCY STOP") != nullptr ||
                    wcsstr(text, L"Select a macro") != nullptr)
                {
                    ShowWindow(hChild, SW_SHOW);
                }
                // Hide combo-related labels in macro mode
                else if (wcsstr(text, L"Trigger") != nullptr ||
                    wcsstr(text, L"No combo") != nullptr ||
                    wcsstr(text, L"Automatic") != nullptr ||
                    wcsstr(text, L"Delay") != nullptr ||
                    wcsstr(text, L"Action:") != nullptr ||
                    wcsstr(text, L"Configured Actions") != nullptr ||
                    wcsstr(text, L"Last Action") != nullptr ||
                    wcsstr(text, L"Combo configuration") != nullptr)
                {
                    ShowWindow(hChild, SW_HIDE);
                }
            }
            else // Combo mode
            {
                // Hide ALL macro-related labels in combo mode
                if (wcsstr(text, L"Options:") != nullptr ||
                    wcsstr(text, L"Speed") != nullptr ||
                    wcsstr(text, L"Status:") != nullptr ||
                    wcsstr(text, L"Recorded Actions") != nullptr ||
                    wcsstr(text, L"EMERGENCY STOP") != nullptr ||
                    wcsstr(text, L"Select a macro") != nullptr)
                {
                    ShowWindow(hChild, SW_HIDE);
                    SetWindowPos(hChild, nullptr, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOZORDER | SWP_NOACTIVATE);
                }
                // Show combo-related labels
                else if (wcsstr(text, L"Trigger") != nullptr ||
                    wcsstr(text, L"No combo") != nullptr ||
                    wcsstr(text, L"Automatic") != nullptr ||
                    wcsstr(text, L"Delay") != nullptr ||
                    wcsstr(text, L"Action:") != nullptr ||
                    wcsstr(text, L"Configured Actions") != nullptr ||
                    wcsstr(text, L"Last Action") != nullptr ||
                    wcsstr(text, L"Combo configuration") != nullptr)
                {
                    ShowWindow(hChild, SW_SHOW);
                }
            }
        }

        hChild = hNext;
    }

    // Force full redraw after mass show/hide/reposition to avoid visual leftovers
    // when switching between Combo and Macro layouts.
    RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

void MacroSubpage_UpdateComboList(HWND hWnd)
{
    MacroUIState* state = (MacroUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state || !state->hMacroList) return;

    // Update macro list to show combos instead
    PremiumCombo::Clear(state->hMacroList);
    state->comboListIds.clear();

    std::vector<int> comboIds = MouseComboSystem::GetAllComboIds();
    for (int id : comboIds)
    {
        MouseCombo* combo = MouseComboSystem::GetCombo(id);
        if (combo)
        {
            std::wstring displayName = combo->name +
                (combo->enabled ? L"" : L" (disabled)");
            PremiumCombo::AddString(state->hMacroList, displayName.c_str());
            state->comboListIds.push_back(id);
        }
    }

    if (comboIds.empty())
    {
        PremiumCombo::AddString(state->hMacroList, L"No combo - click 'New'");
    }
}

void MacroSubpage_UpdateComboUI(HWND hWnd)
{
    MacroUIState* state = (MacroUIState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!state || state->selectedComboId < 0) return;

    MouseCombo* combo = MouseComboSystem::GetCombo(state->selectedComboId);
    if (!combo) return;

    if (state->hComboStatusLabel)
    {
        std::wstring status = L"Selection: " + combo->name;
        SetWindowTextW(state->hComboStatusLabel, status.c_str());
    }

    // Update trigger selection
    if (state->hTriggerList)
    {
        int count = PremiumCombo::GetCount(state->hTriggerList);
        if (count < 10)
        {
            PremiumCombo::Clear(state->hTriggerList);
            for (int i = 0; i < 10; ++i)
                PremiumCombo::AddString(state->hTriggerList, COMBO_TRIGGER_NAMES[i]);
        }
        PremiumCombo::SetCurSel(state->hTriggerList, std::max(0, (int)combo->trigger - 1));
    }

    // Update repeat checkbox and delay
    Button_SetCheck(state->hRepeatCheckbox, combo->repeatWhileHeld ? BST_CHECKED : BST_UNCHECKED);
    SendMessageW(state->hDelaySlider, TBM_SETPOS, TRUE, combo->repeatDelayMs);
    if (state->hDelayLabel) {
        wchar_t buf[64];
        swprintf_s(buf, L"Delai (ms): %d", combo->repeatDelayMs);
        SetWindowTextW(state->hDelayLabel, buf);
    }

    // Update actions display
    std::wstring actionsText = L"Actions du combo:\r\n\r\n";
    for (size_t i = 0; i < combo->actions.size(); ++i)
    {
        const auto& action = combo->actions[i];
        actionsText += L"Action " + std::to_wstring(i + 1) + L": ";

        switch (action.type)
        {
        case ComboActionType::PressKey:
            actionsText += L"Appuyer touche " + std::to_wstring(action.keyHid);
            break;
        case ComboActionType::ReleaseKey:
            actionsText += L"Relacher touche " + std::to_wstring(action.keyHid);
            break;
        case ComboActionType::TapKey:
            actionsText += L"Tap touche " + std::to_wstring(action.keyHid);
            break;
        case ComboActionType::TypeText:
            actionsText += L"Taper: " + action.text;
            break;
        case ComboActionType::MouseClick:
            actionsText += L"Clic souris " + std::to_wstring(action.mouseButton);
            break;
        case ComboActionType::Delay:
            actionsText += L"Attendre " + std::to_wstring(action.delayMs) + L"ms";
            break;
        }
        actionsText += L"\r\n";
    }

    SetWindowTextW(state->hActionsList, actionsText.c_str());

    // Update dedicated actions listbox
    if (state->hComboActionsList)
    {
        SendMessageW(state->hComboActionsList, LB_RESETCONTENT, 0, 0);
        for (const auto& a : combo->actions)
        {
            std::wstring line;
            switch (a.type)
            {
            case ComboActionType::PressKey:
            case ComboActionType::ReleaseKey:
            case ComboActionType::TapKey:
            {
                WORD vk = HidToVk(a.keyHid);
                wchar_t ch = vk ? (wchar_t)vk : L'?';
                const wchar_t* typeTxt = (a.type == ComboActionType::PressKey) ? L"Appuyer" :
                    (a.type == ComboActionType::ReleaseKey) ? L"Relâcher" : L"Tap";
                line = std::wstring(typeTxt) + L" | " + ch;
            }
            break;
            case ComboActionType::TypeText:
                line = L"Taper | " + a.text;
                break;
            case ComboActionType::MouseClick:
            {
                const wchar_t* btn = (a.mouseButton == 1) ? L"Gauche" :
                    (a.mouseButton == 2) ? L"Droite" :
                    (a.mouseButton == 3) ? L"Milieu" : L"?";
                line = std::wstring(L"Clic | ") + btn;
            }
            break;
            case ComboActionType::Delay:
                line = L"Attendre | " + std::to_wstring(a.delayMs) + L" ms";
                break;
            default:
                line = L"Inconnu";
                break;
            }
            SendMessageW(state->hComboActionsList, LB_ADDSTRING, 0, (LPARAM)line.c_str());
        }
    }

    // Ensure action type dropdown contains all 6 options
    if (state->hActionType)
    {
        int count = PremiumCombo::GetCount(state->hActionType);
        if (count < 6)
        {
            PremiumCombo::Clear(state->hActionType);
            for (int i = 0; i < 6; ++i)
                PremiumCombo::AddString(state->hActionType, COMBO_ACTION_TYPE_NAMES[i]);
        }
    }
}
