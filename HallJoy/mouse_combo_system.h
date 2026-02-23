#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <vector>
#include <string>

// Système de combos de souris pour HallJoy
// Permet de détecter des combinaisons de clics et déclencher des actions

// Types d'actions possibles
enum class ComboActionType
{
    None = 0,
    PressKey,           // Appuyer sur une touche
    ReleaseKey,         // Relâcher une touche
    TapKey,             // Appuyer puis relâcher rapidement
    TypeText,           // Taper du texte
    MouseClick,         // Clic de souris
    Delay               // Attendre X millisecondes
};

// Conditions de déclenchement
enum class ComboTriggerType
{
    None = 0,
    RightClickHeld_LeftClick,       // Clic droit maintenu + clic gauche
    LeftClickHeld_RightClick,       // Clic gauche maintenu + clic droit
    MiddleClickHeld_LeftClick,      // Clic milieu maintenu + clic gauche
    MiddleClickHeld_RightClick,     // Clic milieu maintenu + clic droit
    DoubleLeftClick,                // Double clic gauche rapide
    DoubleRightClick,               // Double clic droit rapide
    TripleLeftClick,                // Triple clic gauche rapide
    LeftAndRightTogether,           // Clic gauche ET droit en même temps
    WheelUp_WithRightHeld,          // Molette haut avec clic droit maintenu
    WheelDown_WithRightHeld         // Molette bas avec clic droit maintenu
};

// Action à exécuter
struct ComboAction
{
    ComboActionType type = ComboActionType::None;
    uint16_t keyHid = 0;            // Pour PressKey/ReleaseKey/TapKey
    std::wstring text;              // Pour TypeText
    uint32_t delayMs = 0;           // Pour Delay
    int mouseButton = 0;            // Pour MouseClick (0=left, 1=right, 2=middle)
};

// Définition d'un combo
struct MouseCombo
{
    std::wstring name;              // Nom du combo (pour l'UI)
    ComboTriggerType trigger;       // Condition de déclenchement
    std::vector<ComboAction> actions; // Actions à exécuter
    bool enabled = true;            // Actif ou non
    bool repeatWhileHeld = false;   // Répéter tant que maintenu
    uint32_t repeatDelayMs = 400;   // Délai entre répétitions (ms)
    bool useLocalRepeat = false;    // If true, use repeatDelayMs; otherwise use global setting
    DWORD lastExecutionTime = 0;    // Timestamp de la dernière exécution
};

// Système de gestion des combos
namespace MouseComboSystem
{
    // Initialisation/Arrêt
    void Initialize();
    void Shutdown();
    
    // Gestion des combos
    int CreateCombo(const std::wstring& name, ComboTriggerType trigger);
    bool DeleteCombo(int comboId);
    MouseCombo* GetCombo(int comboId);
    std::vector<int> GetAllComboIds();
    int GetComboCount();
    
    // Actions
    bool AddAction(int comboId, const ComboAction& action);
    bool ClearActions(int comboId);
    
    // Activation/Désactivation
    bool SetComboEnabled(int comboId, bool enabled);
    bool IsComboEnabled(int comboId);
    
    // Répétition
    bool SetComboRepeat(int comboId, bool repeat, uint32_t delayMs = 400);
    
    // Traitement des événements (appelé par le hook)
    void ProcessMouseEvent(UINT msg, WPARAM wParam, LPARAM lParam);
    void ProcessKeyboardEvent(UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Tick pour les répétitions
    void Tick();
    
    // État actuel
    bool IsRightButtonHeld();
    bool IsLeftButtonHeld();
    bool IsMiddleButtonHeld();
    
    // Sauvegarde/Chargement
    bool SaveToFile(const wchar_t* filePath);
    bool LoadFromFile(const wchar_t* filePath);
    
    // Helpers pour créer des actions
    ComboAction MakePressKeyAction(uint16_t hid);
    ComboAction MakeReleaseKeyAction(uint16_t hid);
    ComboAction MakeTapKeyAction(uint16_t hid);
    ComboAction MakeTypeTextAction(const std::wstring& text);
    ComboAction MakeDelayAction(uint32_t ms);
}

// Helpers pour convertir HID en VK et vice-versa
uint16_t VkToHid(WORD vk);
WORD HidToVk(uint16_t hid);
