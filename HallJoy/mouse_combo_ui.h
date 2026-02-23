#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>

// Interface utilisateur pour les combos de souris
namespace MouseComboUI
{
    // Créer la page de configuration des combos
    HWND CreateComboPage(HWND parent, HINSTANCE hInst);
    
    // Rafraîchir la liste des combos
    void RefreshComboList(HWND hComboPage);
    
    // Mettre à jour l'interface pour un combo sélectionné
    void UpdateComboUI(HWND hComboPage, int comboId);
    
    // Gestionnaires d'événements
    void OnAddCombo(HWND hComboPage);
    void OnDeleteCombo(HWND hComboPage);
    void OnSaveCombo(HWND hComboPage);
    void OnToggleCombo(HWND hComboPage);
    
    // IDs des contrôles
    enum {
        IDC_COMBO_LIST = 1001,
        IDC_COMBO_NAME = 1002,
        IDC_COMBO_TRIGGER = 1003,
        IDC_ACTION_LIST = 1004,
        IDC_ADD_ACTION = 1005,
        IDC_DELETE_ACTION = 1006,
        IDC_ACTION_KEY = 1007,
        IDC_ACTION_TYPE = 1008,
        IDC_REPEAT_CHECK = 1009,
        IDC_REPEAT_DELAY = 1010,
        IDC_SAVE_COMBO = 1011,
        IDC_DELETE_COMBO_BUTTON = 1012,
        IDC_TOGGLE_ENABLED = 1013
        ,IDC_REPEAT_LOCAL = 1014
    };
}
