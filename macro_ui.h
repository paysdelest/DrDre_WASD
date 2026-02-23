#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <vector>

// Macro/Combo UI System for HallJoy
// Provides UI components for macro and combo management

// Mode selection
enum class MacroComboMode
{
    Macro = 0,
    Combo = 1
};

// Macro control IDs
constexpr int MACRO_ID_MODE_COMBO = 8000;  // NEW: Mode selection combo
constexpr int MACRO_ID_NEW_MACRO = 8001;
constexpr int MACRO_ID_DELETE_MACRO = 8002;
constexpr int MACRO_ID_RENAME_MACRO = 8003;
constexpr int MACRO_ID_RECORD_START = 8004;
constexpr int MACRO_ID_RECORD_STOP = 8005;
constexpr int MACRO_ID_PLAY_START = 8006;
constexpr int MACRO_ID_PLAY_STOP = 8007;
constexpr int MACRO_ID_LOOP_CHECKBOX = 8008;
constexpr int MACRO_ID_BLOCK_KEYS_CHECKBOX = 8009;
constexpr int MACRO_ID_SPEED_SLIDER = 8010;
constexpr int MACRO_ID_MACRO_LIST = 8011;
constexpr int MACRO_ID_ACTIONS_LIST = 8013;
constexpr int MACRO_ID_EDIT_TIMING = 8014;
constexpr int MACRO_ID_EXPORT_JSON = 8015;
constexpr int MACRO_ID_IMPORT_JSON = 8016;
constexpr int MACRO_ID_SAVE_LOCATION = 8017;

// NEW: Combo-specific control IDs
constexpr int COMBO_ID_TRIGGER_LIST = 8018;     // Trigger type dropdown
constexpr int COMBO_ID_REPEAT_CHECKBOX = 8019;  // Repeat checkbox
constexpr int COMBO_ID_DELAY_SLIDER = 8020;      // Delay slider for repetition
constexpr int COMBO_ID_ACTION_KEY = 8021;        // Key input for action
constexpr int COMBO_ID_ACTION_TYPE = 8022;       // Action type dropdown
constexpr int COMBO_ID_ADD_ACTION = 8023;         // Add action button
constexpr int COMBO_ID_REMOVE_ACTION = 8024;     // Remove action button
constexpr int COMBO_ID_CLEAR_ACTIONS = 8025;     // Clear all actions
constexpr int COMBO_ID_ACTIONS_LIST = 8026;      // List of actions (listbox)
constexpr int COMBO_ID_EXPORT_JSON = 8027;       // Export combos to JSON
constexpr int COMBO_ID_IMPORT_JSON = 8028;       // Import combos from JSON

// Timer IDs
constexpr UINT_PTR MACRO_UI_TIMER_ID = 8012;
constexpr UINT MACRO_UI_REFRESH_MS = 100;

// Window procedure for macro subpage
LRESULT CALLBACK MacroSubpageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// UI creation helpers
HWND MacroSubpage_Create(HWND hParent, HINSTANCE hInst);
void MacroSubpage_RefreshUI(HWND hWnd);
void MacroSubpage_UpdateRecordingStatus(HWND hWnd);
void MacroSubpage_UpdatePlaybackStatus(HWND hWnd);
void MacroSubpage_UpdateActionsList(HWND hWnd);  // NEW: Mise à jour de la liste

// NEW: Combo-specific functions
void MacroSubpage_UpdateComboUI(HWND hWnd);      // Update combo-specific controls
void MacroSubpage_SwitchMode(HWND hWnd, MacroComboMode newMode);  // Switch between Macro/Combo mode
void MacroSubpage_UpdateComboList(HWND hWnd);     // Update combo list
void MacroSubpage_UpdateComboActionsList(HWND hWnd); // Update combo actions list

// UI state management
struct MacroUIState
{
    HWND hMacroList = nullptr;
    HWND hRecordButton = nullptr;
    HWND hPlayButton = nullptr;
    HWND hLoopCheckbox = nullptr;
    HWND hDeleteMacroBtn = nullptr;  // NEW: Delete macro button
    HWND hBlockKeysCheckbox = nullptr;
    HWND hSpeedSlider = nullptr;
    HWND hRenameEdit = nullptr;
    HWND hStatusText = nullptr;
    HWND hActionsList = nullptr;  // NEW: Liste des actions
    HWND hNewMacroBtn = nullptr;  // NEW: New Macro/Combo button
    
    // NEW: Combo-specific controls
    HWND hModeCombo = nullptr;           // Mode selection (Macro/Combo)
    HWND hTriggerLabel = nullptr;         // Trigger label
    HWND hTriggerList = nullptr;          // Trigger type dropdown
    HWND hRepeatCheckbox = nullptr;       // Repeat checkbox
    HWND hDelayLabel = nullptr;           // Delay label
    HWND hDelaySlider = nullptr;          // Delay slider
    HWND hActionLabel = nullptr;          // Action label
    HWND hActionKey = nullptr;            // Key input
    HWND hActionType = nullptr;           // Action type dropdown
    HWND hAddActionButton = nullptr;      // Add action button
    HWND hRemoveActionButton = nullptr;   // Remove action button
    HWND hClearActionsButton = nullptr;   // Clear all actions button
    HWND hComboActionsList = nullptr;     // ListBox for actions
    HWND hComboExportBtn = nullptr;       // Export combos
    HWND hComboImportBtn = nullptr;       // Import combos
    
    // NEW: Information labels for combo status
    HWND hComboStatusLabel = nullptr;     // Status under trigger
    HWND hActionCountLabel = nullptr;     // Action count under action
    HWND hLastActionLabel = nullptr;      // Last action info
    
    // Macro-specific controls (for proper hiding in combo mode)
    HWND hEditTimingBtn = nullptr;        // Edit timing button
    HWND hExportJsonBtn = nullptr;         // Export JSON button
    HWND hImportJsonBtn = nullptr;         // Import JSON button
    HWND hEmergencyStopBtn = nullptr;
    HWND hOptionsLabel = nullptr;
    HWND hSpeedLabel = nullptr;
    HWND hSpeedValueLabel = nullptr;
    HWND hActionsRecordedLabel = nullptr;
    HWND hSaveLocationLabel = nullptr;
    HWND hSaveDirLabel = nullptr;
    RECT emergencyStopOrigRect{};
    
    // Mode and state
    MacroComboMode currentMode = MacroComboMode::Macro;
    bool isRecording = false;
    bool isPlaying = false;
    int selectedMacroId = -1;
    int selectedComboId = -1;  // NEW: Selected combo ID
    uint32_t lastUpdateTime = 0;
    int lastActionCount = 0;  // NEW: Pour détecter les changements
    std::vector<int> comboListIds;
};
