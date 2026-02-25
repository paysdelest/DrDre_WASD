#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// ============================================================
// FREE COMBO UI - DrDre_WASD v2.0
// UI for free-trigger combos
// ============================================================

namespace FreeComboUI
{
    // Create page inside the parent tab
    HWND CreatePage(HWND parent, HINSTANCE hInst);

    // Refresh combo list
    void RefreshComboList();

    // Select combo
    void SelectCombo(int id);

    // Handlers (called from WndProc)
    void OnCommand(HWND hWnd, WORD controlId, WORD notifCode);
    void OnTimer();  // Poll capture completion

    // Control IDs
    enum ControlIds {
        ID_COMBO_LIST       = 2001,
        ID_BTN_NEW          = 2002,
        ID_BTN_DELETE       = 2003,
        ID_BTN_SAVE         = 2004,
        ID_EDIT_NAME        = 2005,
        ID_BTN_CAPTURE      = 2006,
        ID_LBL_TRIGGER      = 2007,
        ID_ACTION_LIST      = 2008,
        ID_ACTION_TYPE_CB   = 2009,
        ID_ACTION_KEY_EDIT  = 2010,
        ID_BTN_ADD_ACTION   = 2011,
        ID_BTN_DEL_ACTION   = 2012,
        ID_BTN_UP_ACTION    = 2013,
        ID_BTN_DOWN_ACTION  = 2014,
        ID_CHK_REPEAT       = 2015,
        ID_EDIT_DELAY       = 2016,
        ID_CHK_ENABLED      = 2017,
        ID_SLIDER_DELAY     = 2018,
        ID_LBL_DELAY_VALUE  = 2019,
        ID_BTN_ADD_DELAY    = 2020,
        ID_TIMER_CAPTURE    = 2099,
    };
}
