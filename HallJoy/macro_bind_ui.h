#pragma once
#include <windows.h>
#include <vector>
#include <string>

// UI State for HallJoy Bind Macro System
struct BindMacroUIState
{
    // Main controls
    HWND hMacroList = nullptr;
    HWND hMacroNameEdit = nullptr;
    HWND hCreateButton = nullptr;
    HWND hDeleteButton = nullptr;
    HWND hRecordButton = nullptr;
    HWND hPlayButton = nullptr;
    HWND hStopButton = nullptr;
    
    // Direct binding controls
    HWND hBindingGroupBox = nullptr;
    HWND hMouseLeftDownCheck = nullptr;
    HWND hMouseLeftUpCheck = nullptr;
    HWND hMouseRightDownCheck = nullptr;
    HWND hMouseRightUpCheck = nullptr;
    HWND hMouseLeftPressedCheck = nullptr;
    HWND hMouseRightPressedCheck = nullptr;
    HWND hKeyBindingEdit = nullptr;
    HWND hAddTriggerButton = nullptr;
    HWND hRemoveTriggerButton = nullptr;
    HWND hTriggerList = nullptr;
    HWND hCooldownEdit = nullptr;
    
    // Advanced controls
    HWND hConditionalGroupBox = nullptr;
    HWND hConditionTypeCombo = nullptr;
    HWND hConditionValueEdit = nullptr;
    HWND hAddConditionButton = nullptr;
    HWND hConditionList = nullptr;
    HWND hRequireAllRadio = nullptr;
    HWND hRequireAnyRadio = nullptr;
    
    // Settings
    HWND hLoopCheckbox = nullptr;
    HWND hBlockKeysCheckbox = nullptr;
    HWND hSpeedSlider = nullptr;
    HWND hSpeedLabel = nullptr;
    HWND hEnableDirectBindingCheckbox = nullptr;
    
    // Status
    HWND hStatusLabel = nullptr;
    HWND hRecordingTimeLabel = nullptr;
    HWND hExecutionCountLabel = nullptr;
    
    // Actions list
    HWND hActionsList = nullptr;
    HWND hAddActionButton = nullptr;
    HWND hEditActionButton = nullptr;
    HWND hRemoveActionButton = nullptr;
    
    int selectedMacroId = -1;
    int selectedTriggerId = -1;
    int selectedConditionId = -1;
    int selectedActionId = -1;
    bool isRecording = false;
    bool isPlaying = false;
};

// Control IDs
#define BIND_MACRO_ID_LIST 1001
#define BIND_MACRO_ID_NAME_EDIT 1002
#define BIND_MACRO_ID_CREATE_BUTTON 1003
#define BIND_MACRO_ID_DELETE_BUTTON 1004
#define BIND_MACRO_ID_RECORD_BUTTON 1005
#define BIND_MACRO_ID_PLAY_BUTTON 1006
#define BIND_MACRO_ID_STOP_BUTTON 1007

#define BIND_MACRO_ID_BINDING_GROUP 1008
#define BIND_MACRO_ID_MOUSE_LEFT_DOWN_CHECK 1009
#define BIND_MACRO_ID_MOUSE_LEFT_UP_CHECK 1010
#define BIND_MACRO_ID_MOUSE_RIGHT_DOWN_CHECK 1011
#define BIND_MACRO_ID_MOUSE_RIGHT_UP_CHECK 1012
#define BIND_MACRO_ID_MOUSE_LEFT_PRESSED_CHECK 1013
#define BIND_MACRO_ID_MOUSE_RIGHT_PRESSED_CHECK 1014
#define BIND_MACRO_ID_KEY_BINDING_EDIT 1015
#define BIND_MACRO_ID_ADD_TRIGGER_BUTTON 1016
#define BIND_MACRO_ID_REMOVE_TRIGGER_BUTTON 1017
#define BIND_MACRO_ID_TRIGGER_LIST 1018
#define BIND_MACRO_ID_COOLDOWN_EDIT 1019

#define BIND_MACRO_ID_CONDITIONAL_GROUP 1020
#define BIND_MACRO_ID_CONDITION_TYPE_COMBO 1021
#define BIND_MACRO_ID_CONDITION_VALUE_EDIT 1022
#define BIND_MACRO_ID_ADD_CONDITION_BUTTON 1023
#define BIND_MACRO_ID_CONDITION_LIST 1024
#define BIND_MACRO_ID_REQUIRE_ALL_RADIO 1025
#define BIND_MACRO_ID_REQUIRE_ANY_RADIO 1026

#define BIND_MACRO_ID_LOOP_CHECKBOX 1027
#define BIND_MACRO_ID_BLOCK_KEYS_CHECKBOX 1028
#define BIND_MACRO_ID_SPEED_SLIDER 1029
#define BIND_MACRO_ID_SPEED_LABEL 1030
#define BIND_MACRO_ID_ENABLE_DIRECT_BINDING_CHECKBOX 1031

#define BIND_MACRO_ID_STATUS_LABEL 1032
#define BIND_MACRO_ID_RECORDING_TIME_LABEL 1033
#define BIND_MACRO_ID_EXECUTION_COUNT_LABEL 1034

#define BIND_MACRO_ID_ACTIONS_LIST 1035
#define BIND_MACRO_ID_ADD_ACTION_BUTTON 1036
#define BIND_MACRO_ID_EDIT_ACTION_BUTTON 1037
#define BIND_MACRO_ID_REMOVE_ACTION_BUTTON 1038

// Functions
HWND BindMacroSubpage_Create(HWND hParent, HINSTANCE hInst);
LRESULT CALLBACK BindMacroSubpageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void BindMacroSubpage_RefreshUI(HWND hWnd);
void BindMacroSubpage_UpdateRecordingStatus(HWND hWnd);
void BindMacroSubpage_UpdatePlaybackStatus(HWND hWnd);
void BindMacroSubpage_UpdateMacroList(HWND hWnd);
void BindMacroSubpage_UpdateTriggerList(HWND hWnd);
void BindMacroSubpage_UpdateConditionList(HWND hWnd);
void BindMacroSubpage_UpdateActionsList(HWND hWnd);
void BindMacroSubpage_UpdateStatistics(HWND hWnd);
