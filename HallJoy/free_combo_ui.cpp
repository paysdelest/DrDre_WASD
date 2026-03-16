// free_combo_ui.cpp  ─  DrDre_WASD v2.0  ─  Premium UI rewrite
// ====================================================================
// Visual improvements :
//   • Left owner-draw list: state dot (green/gray), keyboard/mouse icon,
//     subtle alternating backgrounds, selected item in dark blue
//   • Owner-draw action list: color dot by type, numbering,
//     readable text with key name
//   • Cards drawn in WM_PAINT (distinct background + small-caps label)
//     for Trigger, Options, Actions
//   • Owner-draw buttons: Save=blue, CAPTURE=green/orange,
//     Delete=red, others=neutral gray, hover + pressed state
//   • Vertical separator between both columns
//   • Double-buffer in WM_PAINT (no flicker)
//   • WM_SIZE: left list stretches to use all available space
//   • Trigger label colored orange during capture
// ====================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "free_combo_ui.h"
#include "free_combo_system.h"
#include "ui_theme.h"
#include "win_util.h"
#include "Resource.h"   // IDR_LANTERN_20_PNG / IDR_LANTERN_24_PNG

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "gdiplus.lib")

// ────────────────────────────────────────────────────────────────────
// Win32 shortcut macros
// ────────────────────────────────────────────────────────────────────
#define LB_RESET(h)         SendMessageW((h), LB_RESETCONTENT, 0, 0)
#define LB_ADDRAW(h)        SendMessageW((h), LB_ADDSTRING, 0, (LPARAM)L" ")
#define LB_SEL(h)           (int)SendMessageW((h), LB_GETCURSEL, 0, 0)
#define LB_SETCUR(h,i)      SendMessageW((h), LB_SETCURSEL, (WPARAM)(i), 0)
#define CB_ADD(h,s)         SendMessageW((h), CB_ADDSTRING, 0, (LPARAM)(s))
#define CB_GETSEL(h)        (int)SendMessageW((h), CB_GETCURSEL, 0, 0)
#define CB_SETSEL(h,i)      SendMessageW((h), CB_SETCURSEL, (WPARAM)(i), 0)
#define CHK_SET(h,v)        SendMessageW((h), BM_SETCHECK, (v)?BST_CHECKED:BST_UNCHECKED, 0)
#define CHK_GET(h)          (SendMessageW((h), BM_GETCHECK, 0, 0)==BST_CHECKED)

// ────────────────────────────────────────────────────────────────────
// Local palette (extends UiTheme for new elements)
// ────────────────────────────────────────────────────────────────────
namespace Pal {
    static constexpr COLORREF CardBg = RGB(28, 28, 32);
    static constexpr COLORREF CardBgTrig = RGB(26, 30, 40);
    static constexpr COLORREF CardBorder = RGB(55, 55, 65);
    static constexpr COLORREF CardBorderTrig = RGB(55, 70, 110);
    static constexpr COLORREF ListAlt = RGB(30, 30, 34);
    static constexpr COLORREF ListSel = RGB(35, 65, 115);
    static constexpr COLORREF ListSelBord = RGB(55, 100, 190);
    static constexpr COLORREF ActSel = RGB(30, 50, 80);
    static constexpr COLORREF Sep = RGB(52, 52, 60);
    static constexpr COLORREF DotActive = RGB(75, 200, 110);
    static constexpr COLORREF DotInactive = RGB(75, 75, 88);
    static constexpr COLORREF BtnNorm = RGB(44, 44, 50);
    static constexpr COLORREF BtnHover = RGB(58, 58, 66);
    static constexpr COLORREF BtnPress = RGB(68, 68, 78);
    static constexpr COLORREF BtnBorder = RGB(70, 70, 80);
    static constexpr COLORREF BtnBorderHov = RGB(100, 100, 120);
    static constexpr COLORREF BtnRed = RGB(130, 32, 32);
    static constexpr COLORREF BtnRedHov = RGB(165, 48, 48);
    static constexpr COLORREF BtnRedPres = RGB(155, 38, 38);
    static constexpr COLORREF BtnRedBord = RGB(190, 70, 70);
    static constexpr COLORREF BtnBlue = RGB(42, 98, 185);
    static constexpr COLORREF BtnBlueHov = RGB(55, 118, 215);
    static constexpr COLORREF BtnBluePres = RGB(38, 88, 165);
    static constexpr COLORREF BtnBlueBord = RGB(80, 155, 255);
    static constexpr COLORREF BtnGreen = RGB(33, 90, 52);
    static constexpr COLORREF BtnGreenHov = RGB(44, 112, 68);
    static constexpr COLORREF BtnGreenPres = RGB(28, 78, 44);
    static constexpr COLORREF BtnGreenBord = RGB(65, 165, 95);
    static constexpr COLORREF BtnOrange = RGB(160, 85, 18);
    static constexpr COLORREF BtnOrangeBord = RGB(230, 140, 55);
    // Action type dot colors
    static constexpr COLORREF ActPress = RGB(70, 150, 255);
    static constexpr COLORREF ActRel = RGB(95, 95, 115);
    static constexpr COLORREF ActTap = RGB(72, 195, 115);
    static constexpr COLORREF ActText = RGB(200, 158, 55);
    static constexpr COLORREF ActClick = RGB(170, 75, 200);
    static constexpr COLORREF ActDelay = RGB(110, 110, 125);
    static constexpr COLORREF CAPTUREOrange = RGB(255, 175, 60);
}








// ────────────────────────────────────────────────────────────────────
// Global UI state
// ────────────────────────────────────────────────────────────────────
static HWND g_hPage = nullptr;

// ── Scroll state (right column) ──────────────────────────────────
static int  g_scrollY = 0;   // current scroll offset in px
static bool g_scrollDrag = false;
static int  g_scrollDragStartY = 0;
static int  g_scrollDragStartScrollY = 0;
static int  g_scrollDragThumbHeight = 0;
static int  g_scrollDragMax = 0;

static constexpr int SCROLLBAR_W = 6;   // scrollbar width px (96dpi base)
static constexpr int SCROLLBAR_M = 3;   // scrollbar margin px

static int Sc(HWND h, int px) { return WinUtil_ScalePx(h, px); }

// Content height of the right column (fixed, computed from layout constants)
static int FC_GetContentHeight(HWND hWnd)
{
    // Sum of all rows in the right column (matches WM_SIZE layout)
    int h = Sc(hWnd, 10)          // pad top
        + Sc(hWnd, 16)          // COMBO NAME label
        + Sc(hWnd, 24)          // edit name
        + Sc(hWnd, 12)          // gap
        + Sc(hWnd, 18)          // TRIGGER label
        + Sc(hWnd, 24)          // trigger label ctrl
        + Sc(hWnd, 6)
        + Sc(hWnd, 26)          // capture button
        + Sc(hWnd, 14)
        + Sc(hWnd, 18)          // OPTIONS label
        + Sc(hWnd, 26)          // checkboxes
        + Sc(hWnd, 24)          // Run N times row (edit + cancel)
        + Sc(hWnd, 24)          // Long press row
        + Sc(hWnd, 4)
        + Sc(hWnd, 24)          // delay edit
        + Sc(hWnd, 4)
        + Sc(hWnd, 26)          // slider
        + Sc(hWnd, 12)
        + Sc(hWnd, 18)          // ACTIONS label
        + Sc(hWnd, 100)         // action list
        + Sc(hWnd, 6)           // CB gap
        + Sc(hWnd, 24)          // type CB
        + Sc(hWnd, 4)
        + Sc(hWnd, 16)          // VALUE label
        + Sc(hWnd, 24)          // value edit
        + Sc(hWnd, 6)
        + Sc(hWnd, 26)          // Add/Delay/Delete
        + Sc(hWnd, 4)
        + Sc(hWnd, 26)          // Up/Down
        + Sc(hWnd, 8)
        + Sc(hWnd, 28)          // Save
        + Sc(hWnd, 6)           // gap sous Save
        + Sc(hWnd, 16)          // hint "Emergency Stop — Ctrl+Alt+Backspace"
        + Sc(hWnd, 12);         // bottom margin
    return h;
}

static int FC_GetMaxScroll(HWND hWnd)
{
    RECT rc{}; GetClientRect(hWnd, &rc);
    int viewH = rc.bottom - rc.top;
    int contentH = FC_GetContentHeight(hWnd);
    return std::max(0, contentH - viewH);
}

static RECT FC_GetScrollTrack(HWND hWnd)
{
    RECT rc{}; GetClientRect(hWnd, &rc);
    int w = Sc(hWnd, SCROLLBAR_W);
    int m = Sc(hWnd, SCROLLBAR_M);
    return { rc.right - w - m, m, rc.right - m, rc.bottom - m };
}

static RECT FC_GetScrollThumb(HWND hWnd)
{
    RECT tr = FC_GetScrollTrack(hWnd);
    int trackH = std::max(1, (int)(tr.bottom - tr.top));
    RECT rc{}; GetClientRect(hWnd, &rc);
    int viewH = std::max(1, (int)(rc.bottom - rc.top));
    int contentH = std::max(1, FC_GetContentHeight(hWnd));
    int maxScroll = FC_GetMaxScroll(hWnd);

    int thumbH = (int)((double)trackH * viewH / contentH);
    thumbH = std::clamp(thumbH, Sc(hWnd, 28), trackH);

    int travel = std::max(0, trackH - thumbH);
    int top = tr.top;
    if (travel > 0 && maxScroll > 0)
        top = tr.top + (int)((double)travel * g_scrollY / maxScroll);

    return { tr.left, top, tr.right, top + thumbH };
}

static void FC_OffsetChildren(HWND hWnd, int dy)
{
    if (dy == 0) return;
    // Seuil X : colonne droite commence après la colonne gauche (lw=220 + marges)
    // On ne déplace QUE les contrôles de la colonne droite (rx ≈ Sc(hWnd, 242))
    const int rxMin = Sc(hWnd, 220); // tout contrôle à gauche de cette limite = colonne gauche
    int count = 0;
    for (HWND c = GetWindow(hWnd, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        RECT rc{}; GetWindowRect(c, &rc); MapWindowPoints(nullptr, hWnd, (LPPOINT)&rc, 2);
        if (rc.left >= rxMin) ++count;
    }
    if (!count) return;
    HDWP hdwp = BeginDeferWindowPos(count);
    for (HWND c = GetWindow(hWnd, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        RECT rc{}; GetWindowRect(c, &rc); MapWindowPoints(nullptr, hWnd, (LPPOINT)&rc, 2);
        if (rc.left < rxMin) continue; // colonne gauche : ne pas toucher
        if (hdwp) hdwp = DeferWindowPos(hdwp, c, nullptr, rc.left, rc.top + dy, 0, 0,
            SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
        else SetWindowPos(c, nullptr, rc.left, rc.top + dy, 0, 0,
            SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    if (hdwp) EndDeferWindowPos(hdwp);
}

static void FC_SetScrollY(HWND hWnd, int newY)
{
    int maxScroll = FC_GetMaxScroll(hWnd);
    int target = std::clamp(newY, 0, maxScroll);
    if (target == g_scrollY) {
        InvalidateRect(hWnd, nullptr, FALSE);
        return;
    }
    int dy = g_scrollY - target;  // positive = children move down
    g_scrollY = target;
    FC_OffsetChildren(hWnd, dy);
    RedrawWindow(hWnd, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

static void FC_DrawScrollbar(HWND hWnd, HDC hdc)
{
    if (FC_GetMaxScroll(hWnd) <= 0) return;
    RECT tr = FC_GetScrollTrack(hWnd);
    RECT th = FC_GetScrollThumb(hWnd);
    // Track
    HBRUSH brTrack = CreateSolidBrush(RGB(40, 40, 48));
    FillRect(hdc, &tr, brTrack);
    DeleteObject(brTrack);
    // Thumb
    COLORREF thumbC = g_scrollDrag ? UiTheme::Color_Accent()
        : RGB(GetRValue(UiTheme::Color_Accent()) * 4 / 5,
            GetGValue(UiTheme::Color_Accent()) * 4 / 5,
            GetBValue(UiTheme::Color_Accent()) * 4 / 5);
    HBRUSH brThumb = CreateSolidBrush(thumbC);
    FillRect(hdc, &th, brThumb);
    DeleteObject(brThumb);
}
// Left column
static HWND g_hComboList = nullptr;
static HWND g_hBtnNew = nullptr;
static HWND g_hBtnDelete = nullptr;
// Right column – combo name
static HWND g_hEditName = nullptr;
// Trigger card
static HWND g_hLblTrigger = nullptr;
static HWND g_hBtnCapture = nullptr;
// Options card
static HWND g_hChkEnabled      = nullptr;
static HWND g_hChkRepeat       = nullptr;
static HWND g_hChkCancelRel    = nullptr;  // Cancel on release
static HWND g_hEditRepeatCount = nullptr;  // Run N times (0=inf)
static HWND g_hChkLongPress    = nullptr;  // Long press enable
static HWND g_hEditLongPressMs = nullptr;  // Long press duration (ms)
static HWND g_hEditDelay = nullptr;
static HWND g_hDelaySlider = nullptr;
static HWND g_hDelayValue = nullptr;
// Card Actions
static HWND g_hActionList = nullptr;
static HWND g_hActionTypeCB = nullptr;
static HWND g_hActionKeyEdt = nullptr;
static HWND g_hBtnCaptureMouse = nullptr; // 🖱 live capture button (visible only for Mouse click type)
static HWND g_hBtnAdd = nullptr;
static HWND g_hBtnAddDelay = nullptr;
static HWND g_hBtnDelAct = nullptr;
static HWND g_hBtnUp = nullptr;
static HWND g_hBtnDown = nullptr;
static HWND g_hBtnSave = nullptr;

// ── Whitelist UI ─────────────────────────────────────────────────────────────
static HWND g_hWlModeCB = nullptr;
static HWND g_hWlList   = nullptr;
static HWND g_hWlEdit   = nullptr;
static HWND g_hWlBtnAdd = nullptr;
static HWND g_hWlBtnDel = nullptr;

// Lanterne WATCHMAN PNG (GDI+)
static Gdiplus::Bitmap* g_lanternBmp = nullptr;

// Kimono MY COMBOS PNG (GDI+)
static Gdiplus::Bitmap* g_comboBmp = nullptr;

static bool g_delaySync = false;
static bool g_capturing = false;
static bool  g_capturingMouseAction = false;
static DWORD g_captureGraceUntil   = 0;
static std::vector<int> g_comboIds;
static int  g_selectedId = -1;

// ── Drag & drop — combo list (left column) ───────────────────
static bool  g_comboDrag        = false;
static int   g_comboDragSrcIdx  = -1;   // list index being dragged
static int   g_comboDragHover   = -1;   // current drop target index
static POINT g_comboDragPt      = {};
static int   g_comboDragStartY  = 0;    // Y client (page) au moment du mousedown

// ── Drag & drop — action list (right column) ─────────────────
static bool  g_actionDrag       = false;
static int   g_actionDragSrcIdx = -1;
static int   g_actionDragHover  = -1;
static POINT g_actionDragPt     = {};

// Width reserved for [x] button drawn inside each action row
static constexpr int kActionDelW = 22; // px at 96dpi (scaled at runtime)

// ── Subclass procs for detect clic/drag in listbox ──
static WNDPROC g_origActionListProc = nullptr;
static WNDPROC g_origComboListProc  = nullptr;
static HWND    g_hPageForSubclass   = nullptr; // reference to the parent window -- référence à la fenêtre parente

static LRESULT CALLBACK ActionListSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK ComboListSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ────────────────────────────────────────────────────────────────────
// Action type names + dot colors
// ────────────────────────────────────────────────────────────────────
static const wchar_t* ACTION_NAMES[] = {
    L"Press key", L"Release key", L"Tap key",
    L"Type text", L"Mouse click", L"Wait (ms)",
};
static const COLORREF ACTION_COLORS[] = {
    Pal::ActPress, Pal::ActRel, Pal::ActTap,
    Pal::ActText,  Pal::ActClick, Pal::ActDelay,
};

// ────────────────────────────────────────────────────────────────────
// Font helpers
// ────────────────────────────────────────────────────────────────────
static HFONT MakeFont(HWND h, int ptSize, int weight)
{
    HDC hdc = GetDC(h);
    int lpy = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(h, hdc);
    return CreateFontW(-MulDiv(ptSize, lpy, 72), 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Segoe UI");
}
static HFONT GetFont(HWND h) { static HFONT f = nullptr; if (!f) f = MakeFont(h, 9, FW_NORMAL);   return f; }
static HFONT GetBold(HWND h)  { static HFONT f = nullptr; if (!f) f = MakeFont(h, 9, FW_SEMIBOLD); return f; }
static HFONT GetSmall(HWND h) { static HFONT f = nullptr; if (!f) f = MakeFont(h, 8, FW_NORMAL);   return f; }
static HFONT GetEmoji(HWND h) {
    static HFONT f = nullptr;
    if (!f) {
        int sz = -MulDiv(11, GetDeviceCaps(GetDC(h), LOGPIXELSY), 72);
        f = CreateFontW(sz, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Emoji");
        if (!f) f = GetFont(h);  // fallback
    }
    return f;
}

static void ApplyFont(HWND h, bool bold = false)
{
    HFONT f = bold ? GetBold(h) : GetFont(h);
    if (f) SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE);
}
static void ApplyFontChildren(HWND p)
{
    HFONT f = GetFont(p);
    if (!f) return;
    for (HWND c = GetWindow(p, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        wchar_t cls[32]{};
        GetClassNameW(c, cls, 32);
        if (_wcsicmp(cls, L"ComboBox") == 0) continue;
        SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE);
    }
}

// ────────────────────────────────────────────────────────────────────
// Data helpers
// ────────────────────────────────────────────────────────────────────
static void PersistToDisk()
{
    std::wstring p = WinUtil_BuildPathNearExe(L"free_combos.dat");
    FreeComboSystem::SaveToFile(p.c_str());
}
static int  ClampDelay(int ms) { return ms < 10 ? 10 : ms>2000 ? 2000 : ms; }
static void SetWindowTextInt(HWND h, int v) { wchar_t b[32]; _itow_s(v, b, 32, 10); SetWindowTextW(h, b); }
static int  GetWindowTextInt(HWND h) { wchar_t b[32]; GetWindowTextW(h, b, 32); return _wtoi(b); }
static void SetDelayUi(int ms, bool edit = true)
{
    ms = ClampDelay(ms);
    if (edit && g_hEditDelay)   SetWindowTextInt(g_hEditDelay, ms);
    if (g_hDelaySlider)         SendMessageW(g_hDelaySlider, TBM_SETPOS, TRUE, ms);
    if (g_hDelayValue)          SetWindowTextW(g_hDelayValue, (std::to_wstring(ms) + L" ms").c_str());
}

static bool TriggerIsMouse(const FreeTrigger& t)
{
    return t.keyType == FreeTriggerKeyType::MouseLeft || t.keyType == FreeTriggerKeyType::MouseRight ||
        t.keyType == FreeTriggerKeyType::MouseMiddle || t.keyType == FreeTriggerKeyType::MouseX1 ||
        t.keyType == FreeTriggerKeyType::MouseX2 || t.keyType == FreeTriggerKeyType::WheelUp ||
        t.keyType == FreeTriggerKeyType::WheelDown || t.keyType == FreeTriggerKeyType::MouseDoubleLeft ||
        t.keyType == FreeTriggerKeyType::MouseDoubleRight;
}
static bool TriggersEqual(const FreeTrigger& a, const FreeTrigger& b)
{
    return a.modifier == b.modifier && a.keyType == b.keyType && a.vkCode == b.vkCode &&
        a.holdKeyType == b.holdKeyType && a.holdVkCode == b.holdVkCode;
}
static void DisableOtherCombosWithSameTrigger(int activeId)
{
    FreeCombo* active = FreeComboSystem::GetCombo(activeId);
    if (!active) return;
    for (int id : FreeComboSystem::GetAllIds()) {
        if (id == activeId) continue;
        FreeCombo* o = FreeComboSystem::GetCombo(id);
        if (o && o->enabled && TriggersEqual(o->trigger, active->trigger))
            o->enabled = false;
    }
}
static bool ActivateIfSameTrigger()
{
    if (g_selectedId < 0) return false;
    FreeCombo* sel = FreeComboSystem::GetCombo(g_selectedId);
    if (!sel || !sel->trigger.IsValid()) return false;
    for (int id : FreeComboSystem::GetAllIds()) {
        if (id == g_selectedId) continue;
        FreeCombo* o = FreeComboSystem::GetCombo(id);
        if (o && o->enabled && TriggersEqual(o->trigger, sel->trigger))
        {
            sel->enabled = true; DisableOtherCombosWithSameTrigger(g_selectedId); return true;
        }
    }
    return false;
}

// Action label (for list)
static std::wstring ActionLabel(const ComboAction& a)
{
    auto keyName = [&](int hid) -> std::wstring {
        wchar_t n[64]{}; UINT sc = MapVirtualKeyW(HidToVk(hid), MAPVK_VK_TO_VSC);
        GetKeyNameTextW((LONG)(sc << 16), n, 64);
        return n[0] ? std::wstring(n) : L"?";
        };
    switch (a.type) {
    case ComboActionType::PressKey:   return L"Press      " + keyName(a.keyHid);
    case ComboActionType::ReleaseKey: return L"Release    " + keyName(a.keyHid);
    case ComboActionType::TapKey:     return L"Tap        " + keyName(a.keyHid);
    case ComboActionType::TypeText:   return L"Text       " + a.text;
    case ComboActionType::MouseClick: {
        const wchar_t* b[] = { L"left", L"right", L"middle", L"X1 (thumb)", L"X2 (thumb2)" };
        return std::wstring(L"Click      ") + b[a.mouseButton < 5 ? a.mouseButton : 0];
    }
    case ComboActionType::Delay: return L"Wait       " + std::to_wstring(a.delayMs) + L" ms";
    default: return L"?";
    }
}
static int ActionTypeIdx(ComboActionType t)
{
    switch (t) {
    case ComboActionType::PressKey:   return 0;
    case ComboActionType::ReleaseKey: return 1;
    case ComboActionType::TapKey:     return 2;
    case ComboActionType::TypeText:   return 3;
    case ComboActionType::MouseClick: return 4;
    default:                           return 5;
    }
}

// ────────────────────────────────────────────────────────────────────
// Double-buffer helper
// ────────────────────────────────────────────────────────────────────
struct DblBuf {
    HDC mem = nullptr; HBITMAP bmp = nullptr; HGDIOBJ old = nullptr;
    bool Begin(HDC hdc, int w, int h) {
        mem = CreateCompatibleDC(hdc); if (!mem)return false;
        bmp = CreateCompatibleBitmap(hdc, w, h); old = SelectObject(mem, bmp); return true;
    }
    void End(HDC hdc, int w, int h) {
        BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
    }
};

// ────────────────────────────────────────────────────────────────────
// GDI drawing primitives
// ────────────────────────────────────────────────────────────────────
static void FillRoundRect(HDC hdc, RECT rc, int r, COLORREF fill, COLORREF border)
{
    HBRUSH br = CreateSolidBrush(fill);
    HPEN   pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(hdc, br), op = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, r, r);
    SelectObject(hdc, ob); SelectObject(hdc, op);
    DeleteObject(br); DeleteObject(pen);
}

static void DrawDot(HDC hdc, int cx, int cy, int r, COLORREF c)
{
    HBRUSH br = CreateSolidBrush(c);
    HPEN   pen = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ ob = SelectObject(hdc, br), op = SelectObject(hdc, pen);
    Ellipse(hdc, cx - r, cy - r, cx + r + 1, cy + r + 1);
    SelectObject(hdc, ob); SelectObject(hdc, op);
    DeleteObject(br); DeleteObject(pen);
}

// Stylized keyboard icon
static void DrawKbdIcon(HDC hdc, int x, int y, int sz, COLORREF c)
{
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HBRUSH br = CreateSolidBrush(c);
    HBRUSH hol = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
    HGDIOBJ op = SelectObject(hdc, pen), ob = SelectObject(hdc, hol);
    RoundRect(hdc, x, y + sz / 3, x + sz, y + sz, 2, 2);
    SelectObject(hdc, br);
    int kw = std::max(2, sz / 6), kh = std::max(2, sz / 8);
    int y1 = y + sz / 3 + 3;
    for (int i = 0; i < 4; i++) {
        RECT kr{ x + 3 + i * (kw + 2),y1,x + 3 + i * (kw + 2) + kw,y1 + kh };
        FillRect(hdc, &kr, br);
    }
    int y2 = y1 + kh + 2;
    RECT sp{ x + 4,y2,x + sz - 4,y2 + kh };
    FillRect(hdc, &sp, br);
    SelectObject(hdc, op); SelectObject(hdc, ob);
    DeleteObject(pen); DeleteObject(br);
}

// Stylized mouse icon
static void DrawMouseIcon(HDC hdc, int x, int y, int sz, COLORREF c)
{
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HBRUSH hol = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
    HGDIOBJ op = SelectObject(hdc, pen), ob = SelectObject(hdc, hol);
    int mw = sz * 6 / 10, mx = x + (sz - mw) / 2;
    RoundRect(hdc, mx, y + 1, mx + mw, y + sz - 1, mw / 2, mw / 2);
    MoveToEx(hdc, mx + mw / 2, y + 1, nullptr);
    LineTo(hdc, mx + mw / 2, y + sz * 4 / 10);
    HBRUSH br = CreateSolidBrush(c);
    SelectObject(hdc, br);
    RECT wr{ mx + mw / 2 - 1,y + sz / 6,mx + mw / 2 + 2,y + sz * 4 / 10 - 2 };
    FillRect(hdc, &wr, br);
    SelectObject(hdc, op); SelectObject(hdc, ob);
    DeleteObject(pen); DeleteObject(br);
}

// Draw section label in small caps above a card
static void DrawCardLabel(HDC hdc, HWND hPage, RECT card, const wchar_t* label)
{
    HFONT oldF = (HFONT)SelectObject(hdc, GetSmall(hPage));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, UiTheme::Color_TextMuted());
    RECT lr{ card.left + 8, card.top + 4, card.right, card.top + 16 };
    DrawTextW(hdc, label, -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldF);
}

// Compute bounding RECT around two controls (for cards)
static bool CardRect(HWND hPage, HWND hTop, HWND hBot, int padH, int padV, RECT& out)
{
    if (!hTop || !hBot || !IsWindow(hTop) || !IsWindow(hBot)) return false;
    RECT r1{}, r2{};
    GetWindowRect(hTop, &r1); MapWindowPoints(nullptr, hPage, (LPPOINT)&r1, 2);
    GetWindowRect(hBot, &r2); MapWindowPoints(nullptr, hPage, (LPPOINT)&r2, 2);
    out.left = std::min(r1.left, r2.left) - padH;
    out.top = r1.top - padV - 14; // espace pour le label
    out.right = std::max(r1.right, r2.right) + padH;
    out.bottom = std::max(r1.bottom, r2.bottom) + padV;
    return true;
}

// ────────────────────────────────────────────────────────────────────
// OWNER-DRAW: combo list
// ────────────────────────────────────────────────────────────────────
static void DrawComboListItem(const DRAWITEMSTRUCT* dis)
{
    if (!dis) return;
    int idx = (int)dis->itemID;
    if (idx < 0 || idx >= (int)g_comboIds.size()) return;
    FreeCombo* combo = FreeComboSystem::GetCombo(g_comboIds[(size_t)idx]);
    if (!combo) return;

    HDC  hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool sel = (dis->itemState & ODS_SELECTED) != 0;

    // ── Background ──────────────────────────────────────────────────────
    COLORREF bg = sel ? Pal::ListSel
        : (idx % 2 == 0 ? UiTheme::Color_ControlBg() : Pal::ListAlt);
    HBRUSH brBg = CreateSolidBrush(bg);
    FillRect(hdc, &rc, brBg);
    DeleteObject(brBg);

    // Accented left border when selected
    if (sel) {
        HBRUSH ba = CreateSolidBrush(Pal::ListSelBord);
        RECT bl{ rc.left,rc.top,rc.left + 3,rc.bottom };
        FillRect(hdc, &bl, ba);
        DeleteObject(ba);
    }

    // Separator line
    if (!sel) {
        HPEN sp = CreatePen(PS_SOLID, 1, Pal::Sep);
        HGDIOBJ op = SelectObject(hdc, sp);
        MoveToEx(hdc, rc.left + 8, rc.bottom - 1, nullptr);
        LineTo(hdc, rc.right - 8, rc.bottom - 1);
        SelectObject(hdc, op); DeleteObject(sp);
    }

    // ── State dot (active=green, inactive=gray) ──────────────────
    int cy = (rc.top + rc.bottom) / 2;
    DrawDot(hdc, rc.left + 14, cy, 4, combo->enabled ? Pal::DotActive : Pal::DotInactive);

    // ── Keyboard / mouse icon ─────────────────────────────────────
    int iconSz = 14, iconX = rc.left + 26, iconY = cy - iconSz / 2;
    COLORREF iconC = sel ? RGB(190, 215, 255) : UiTheme::Color_TextMuted();
    if (combo->trigger.IsValid())
        (TriggerIsMouse(combo->trigger) ? DrawMouseIcon : DrawKbdIcon)(hdc, iconX, iconY, iconSz, iconC);

    // ── Name (top half) + Trigger preview (bottom half) ─────────────
    int rowH2 = (rc.bottom - rc.top) / 2;
    HFONT oldF = (HFONT)SelectObject(hdc, GetFont(dis->hwndItem));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc,
        !combo->enabled ? RGB(100, 100, 115) :
        sel ? RGB(235, 245, 255) :
        UiTheme::Color_Text());
    RECT trName{ iconX + iconSz + 7, rc.top, rc.right - 6, rc.top + rowH2 };
    DrawTextW(hdc, combo->name.c_str(), -1, &trName,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    // Trigger string in small greyed font below the name
    SelectObject(hdc, GetSmall(dis->hwndItem));
    SetTextColor(hdc, sel ? RGB(150, 185, 225) : UiTheme::Color_TextMuted());
    {
        std::wstring tstr = combo->trigger.IsValid()
            ? combo->trigger.ToString() : L"(no trigger)";
        RECT trTrig{ iconX + iconSz + 7, rc.top + rowH2, rc.right - 6, rc.bottom - 1 };
        DrawTextW(hdc, tstr.c_str(), -1, &trTrig,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    // Badge "(inactive)" top-right if disabled and not selected
    if (!combo->enabled && !sel) {
        SelectObject(hdc, GetSmall(dis->hwndItem));
        SetTextColor(hdc, Pal::DotInactive);
        RECT br2{ rc.right - 62, rc.top, rc.right - 4, rc.top + rowH2 };
        DrawTextW(hdc, L"(inactive)", -1, &br2, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    SelectObject(hdc, oldF);
    if (dis->itemState & ODS_FOCUS) DrawFocusRect(hdc, &rc);

    // Drag & drop indicator — draw a blue line above the drop target
    if (g_comboDrag && g_comboDragHover == idx) {
        HPEN hp = CreatePen(PS_SOLID, 2, RGB(80, 160, 255));
        HGDIOBJ op2 = SelectObject(hdc, hp);
        MoveToEx(hdc, rc.left + 4, rc.top + 1, nullptr);
        LineTo(hdc, rc.right - 4, rc.top + 1);
        SelectObject(hdc, op2); DeleteObject(hp);
    }
}

// ────────────────────────────────────────────────────────────────────
// OWNER-DRAW: action list
// ────────────────────────────────────────────────────────────────────
static void DrawActionItem(const DRAWITEMSTRUCT* dis)
{
    if (!dis || g_selectedId < 0) return;
    int idx = (int)dis->itemID;
    FreeCombo* combo = FreeComboSystem::GetCombo(g_selectedId);
    if (!combo || idx < 0 || idx >= (int)combo->actions.size()) return;

    const ComboAction& act = combo->actions[(size_t)idx];
    HDC  hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool sel = (dis->itemState & ODS_SELECTED) != 0;

    HBRUSH brBg = CreateSolidBrush(sel ? Pal::ActSel : UiTheme::Color_ControlBg());
    FillRect(hdc, &rc, brBg);
    DeleteObject(brBg);

    // Bottom separator
    if (!sel) {
        HPEN sp = CreatePen(PS_SOLID, 1, Pal::Sep);
        HGDIOBJ op = SelectObject(hdc, sp);
        MoveToEx(hdc, rc.left + 4, rc.bottom - 1, nullptr);
        LineTo(hdc, rc.right - 4, rc.bottom - 1);
        SelectObject(hdc, op); DeleteObject(sp);
    }

    int cy = (rc.top + rc.bottom) / 2;

    // Pastille couleur type
    int typeIdx = ActionTypeIdx(act.type);
    COLORREF dotC = ACTION_COLORS[typeIdx];
    DrawDot(hdc, rc.left + 10, cy, 4, dotC);

    // Step number
    HFONT oldF = (HFONT)SelectObject(hdc, GetSmall(dis->hwndItem));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, UiTheme::Color_TextMuted());
    wchar_t num[6]; swprintf_s(num, L"%d.", idx + 1);
    RECT nr{ rc.left + 20,rc.top,rc.left + 38,rc.bottom };
    DrawTextW(hdc, num, -1, &nr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // Action text (leave room for [x] on the right)
    SelectObject(hdc, GetFont(dis->hwndItem));
    SetTextColor(hdc, sel ? RGB(220, 238, 255) : UiTheme::Color_Text());
    RECT tr{ rc.left + 42, rc.top, rc.right - kActionDelW - 4, rc.bottom };
    std::wstring lbl = ActionLabel(act);
    DrawTextW(hdc, lbl.c_str(), -1, &tr,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    // [x] delete button — red, right-aligned inside the row
    {
        int bx = rc.right - kActionDelW;
        int bw = kActionDelW - 2;
        int bh = rc.bottom - rc.top - 4;
        int by = rc.top + 2;
        COLORREF xBg  = sel ? RGB(180, 50, 50) : RGB(140, 40, 40);
        COLORREF xBrd = sel ? RGB(220, 80, 80) : RGB(180, 60, 60);
        HBRUSH xBr = CreateSolidBrush(xBg);
        HPEN   xPn = CreatePen(PS_SOLID, 1, xBrd);
        HGDIOBJ ob2 = SelectObject(hdc, xBr);
        HGDIOBJ op2 = SelectObject(hdc, xPn);
        RoundRect(hdc, bx, by, bx + bw, by + bh, 3, 3);
        SelectObject(hdc, ob2); SelectObject(hdc, op2);
        DeleteObject(xBr); DeleteObject(xPn);
        HFONT smF  = GetSmall(dis->hwndItem);
        HFONT oldF2 = (HFONT)SelectObject(hdc, smF);
        SetTextColor(hdc, RGB(255, 200, 200));
        SetBkMode(hdc, TRANSPARENT);
        RECT xr{ bx, by, bx + bw, by + bh };
        DrawTextW(hdc, L"x", -1, &xr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, oldF2);
    }

    SelectObject(hdc, oldF);

    // Drag & drop indicator — blue line above drop target
    if (g_actionDrag && g_actionDragHover == idx) {
        HPEN hp = CreatePen(PS_SOLID, 2, RGB(80, 160, 255));
        HGDIOBJ op2 = SelectObject(hdc, hp);
        MoveToEx(hdc, rc.left + 4, rc.top + 1, nullptr);
        LineTo(hdc, rc.right - 4, rc.top + 1);
        SelectObject(hdc, op2); DeleteObject(hp);
    }
}

// ────────────────────────────────────────────────────────────────────
// OWNER-DRAW: premium buttons
// ────────────────────────────────────────────────────────────────────
static void DrawPremiumButton(const DRAWITEMSTRUCT* dis)
{
    if (!dis) return;
    RECT  rc = dis->rcItem;
    HDC   hdc = dis->hDC;
    bool  off = (dis->itemState & ODS_DISABLED) != 0;
    bool  prs = (dis->itemState & ODS_SELECTED) != 0;
    bool  hot = (dis->itemState & ODS_HOTLIGHT) != 0;
    UINT  id = dis->CtlID;

    bool isDel = (id == FreeComboUI::ID_BTN_DELETE || id == FreeComboUI::ID_BTN_DEL_ACTION);
    bool isSave = (id == FreeComboUI::ID_BTN_SAVE);
    bool isCap = (id == FreeComboUI::ID_BTN_CAPTURE);
    bool isCap2 = (id == FreeComboUI::ID_BTN_CAPTURE_MOUSE); // mouse action capture button
    bool isNew = (id == FreeComboUI::ID_BTN_NEW);
    bool isAdd = (id == FreeComboUI::ID_BTN_ADD_ACTION);

    COLORREF bg, border, textC;

    if (off) {
        bg = RGB(28, 28, 32); border = RGB(50, 50, 58); textC = UiTheme::Color_TextMuted();
    }
    else if (isDel) {
        bg = prs ? Pal::BtnRedPres : hot ? Pal::BtnRedHov : Pal::BtnRed;
        border = Pal::BtnRedBord;
        textC = RGB(255, 210, 210);
    }
    else if (isSave) {
        bg = prs ? Pal::BtnBluePres : hot ? Pal::BtnBlueHov : Pal::BtnBlue;
        border = Pal::BtnBlueBord;
        textC = RGB(255, 255, 255);
    }
    else if (isCap) {
        if (g_capturing) {
            bg = prs ? Pal::BtnOrange : Pal::BtnOrange;
            border = Pal::BtnOrangeBord;
            textC = RGB(255, 235, 190);
        }
        else {
            bg = prs ? Pal::BtnGreenPres : hot ? Pal::BtnGreenHov : Pal::BtnGreen;
            border = Pal::BtnGreenBord;
            textC = RGB(195, 255, 215);
        }
    }
    else if (isCap2) {
        // Mouse capture button: orange while capturing, green when idle
        if (g_capturingMouseAction) {
            bg = Pal::BtnOrange; border = Pal::BtnOrangeBord; textC = RGB(255, 235, 190);
        }
        else {
            bg = prs ? Pal::BtnGreenPres : hot ? Pal::BtnGreenHov : Pal::BtnGreen;
            border = Pal::BtnGreenBord; textC = RGB(195, 255, 215);
        }
    }
    else if (isNew || isAdd) {
        bg = prs ? Pal::BtnGreenPres : hot ? Pal::BtnGreenHov : Pal::BtnGreen;
        border = Pal::BtnGreenBord;
        textC = RGB(195, 255, 215);
    }
    else {
        bg = prs ? Pal::BtnPress : hot ? Pal::BtnHover : Pal::BtnNorm;
        border = hot ? Pal::BtnBorderHov : Pal::BtnBorder;
        textC = UiTheme::Color_Text();
    }

    // Page background behind button (avoids artifacts)
    FillRect(hdc, &rc, UiTheme::Brush_PanelBg());
    FillRoundRect(hdc, rc, 5, bg, border);

    // Texte
    wchar_t txt[128]{}; GetWindowTextW(dis->hwndItem, txt, 128);
    HFONT oldF = (HFONT)SelectObject(hdc, (isSave || isCap) ? GetBold(dis->hwndItem) : GetFont(dis->hwndItem));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textC);
    DrawTextW(hdc, txt, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(hdc, oldF);
}

//Ajout --->
static void DrawEmergencyHintUnderSave(HWND hWnd, HDC hdc)
{
    if (!g_hBtnSave || !IsWindow(g_hBtnSave)) return;

    // retrieves the rectangle of the Save combo button (in page coordinates) -- récupère le rect du bouton Save combo (en coords page)
    RECT br{};
    GetWindowRect(g_hBtnSave, &br);
    MapWindowPoints(nullptr, hWnd, (LPPOINT)&br, 2);

    // text area just below -- zone de texte juste en dessous
    RECT r{};
    r.left = br.left;
    r.right = br.right;
    r.top = br.bottom + Sc(hWnd, 6);
    r.bottom = r.top + Sc(hWnd, 16);

    // If you are off-screen, do not draw -- si on est hors écran, ne pas dessiner
    RECT rc{};
    GetClientRect(hWnd, &rc);
    if (r.top >= rc.bottom) return;

    const wchar_t* txt = L"Emergency Stop  \x2014  Ctrl + Alt + Backspace";

    HFONT oldF = (HFONT)SelectObject(hdc, GetSmall(hWnd));
    SetBkMode(hdc, TRANSPARENT);

    // thin shadow -- micro-ombre (très fine)
    RECT rs = r;
    OffsetRect(&rs, Sc(hWnd, 1), Sc(hWnd, 1));
    SetTextColor(hdc, RGB(0, 0, 0));
    DrawTextW(hdc, txt, -1, &rs, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

    // Text: not bright red -> "muted + slight accent" -- texte : pas rouge flashy -> “muted + léger accent”
    // // (You can replace it with UiTheme::Color_TextMuted() if you prefer a completely neutral look) -- (tu peux remplacer par UiTheme::Color_TextMuted() si tu préfères full neutre)
    SetTextColor(hdc, RGB(200, 110, 110));
    //SetTextColor(hdc, RGB(235, 60, 60));
    DrawTextW(hdc, txt, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

    SelectObject(hdc, oldF);
}
//Ajout<--



// ────────────────────────────────────────────────────────────────────
// MAIN PAGE PAINT
// ────────────────────────────────────────────────────────────────────
static void PaintPage(HWND hWnd, HDC hdc)
{
    RECT rc{}; GetClientRect(hWnd, &rc);
    FillRect(hdc, &rc, UiTheme::Brush_PanelBg());

    // ── Vertical separator ──────────────────────────────────────
    if (g_hComboList && IsWindow(g_hComboList)) {
        RECT lr{}; GetWindowRect(g_hComboList, &lr); MapWindowPoints(nullptr, hWnd, (LPPOINT)&lr, 2);
        int sx = lr.right + Sc(hWnd, 8);
        HPEN sp = CreatePen(PS_SOLID, 1, Pal::Sep);
        HGDIOBJ op = SelectObject(hdc, sp);
        MoveToEx(hdc, sx, Sc(hWnd, 8), nullptr);
        LineTo(hdc, sx, rc.bottom - Sc(hWnd, 8));
        SelectObject(hdc, op); DeleteObject(sp);
    //} suppress
    
    //Ajout --->
    // ── Emergency hint (under Save combo) ─────────────────────
    DrawEmergencyHintUnderSave(hWnd, hdc);
    }//Ajout<--

    // ── Label Left column — 🥋 My combos ─────────────────────────────────────
    {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, UiTheme::Color_Text());
        
        const int labelTop = Sc(hWnd, 3);  //  ↕ distance from the top of the window - ↕ distance depuis le haut de la fenêtre
        //  ↑ increase = the entire block descends / ↓ = ascends - ↑ augmenter = tout le bloc descend / ↓ = monte
        // ⚠️ Pass on to WM_SIZE: const int ly = Sc(hWnd,3) + Sc(hWnd,34) + ... - ⚠️ répercuter dans WM_SIZE : const int ly = Sc(hWnd,3) + Sc(hWnd,34) + ...

        const int labelH = Sc(hWnd, 38);   // ↕ height of the ‘My combos’ title area - ↕ hauteur de la zone titre "My combos"
        // ↑ = more vertical space / ↓ = more compact - ↑ = plus d'espace vertical / ↓ = plus compact
        // ⚠️ Pass on to WM_SIZE: Sc(hWnd, 34) in ly -⚠️ répercuter dans WM_SIZE : le Sc(hWnd, 34) dans ly

        const int imgSz = Sc(hWnd, 34);    // icon size in pixels — ↑ = larger / ↓ = smaller - taille icône en pixels — ↑ = plus grande / ↓ = plus petite
        // ⚠️ Also change the name of the png file below to match. - ⚠️ change aussi le nom du fichier png ci-dessous pour correspondre

        const int imgX = Sc(hWnd, 8);      // ← → horizontal position icon - ← → position horizontale icône karateka
        // ↑ increase = move right / ↓ decrease = move left - ↑ augmenter = décale à droite / ↓ diminuer = décale à gauche
        // Sc(hWnd, 0) = stuck to the left edge of the window - collé au bord gauche de la fenêtre

        const int imgY = labelTop + (labelH - imgSz) / 2; // ↕ vertical position icon karateka
        // /1 = bas / /2 = centré / /4 = haut


        // Icône combo PNG (lazy load depuis assets\combo_24.png)
        if (!g_comboBmp) {
            std::wstring path = WinUtil_BuildPathNearExe(L"assets\\combo_34.png");
            Gdiplus::Bitmap* b = Gdiplus::Bitmap::FromFile(path.c_str(), FALSE);
            if (b && b->GetLastStatus() == Gdiplus::Ok) g_comboBmp = b;
            else if (b) delete b;
        }
        if (g_comboBmp) {
            Gdiplus::Graphics g(hdc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
            g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
            g.DrawImage(g_comboBmp, imgX, imgY, imgSz, imgSz);
        }

        HFONT oldF = (HFONT)SelectObject(hdc, GetBold(hWnd)); // title font ‘My combos’ — GetBold = bold / GetFont = normal - police titre "My combos" — GetBold = gras / GetFont = normal
        RECT lr{ imgX + imgSz + Sc(hWnd, 13), // ← left edge title ‘My combos’ ← bord gauche titre "My combos"
            // imgX + imgSz = juste après l'icône karateka
            // + Sc(hWnd, 4) = espace entre icône et titre
            // ↑ augmenter le 4 = titre plus à droite / ↓ = plus à gauche
                        // imgX + imgSz = just after the karateka icon
            // + Sc(hWnd, 4) = space between icon and title
            // ↑ increase the 4 = title further to the right / ↓ = further to the left
labelTop,                    //↕ top edge title - ↕ bord haut titre
Sc(hWnd, 230),              // → Maximum width of the title ‘My combos’ - → largeur max du titre "My combos"
// ↑ increase = title can extend further to the right - ↑ augmenter = titre peut s'étendre plus loin à droite
labelTop + labelH };         // ↕ bottom edge title - ↕ bord bas titre
        DrawTextW(hdc, L"My combos", -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldF);
    }

    // ── Right-click hint (left column) ───────────────────────
    if (g_hBtnNew && IsWindow(g_hBtnNew)) {
        RECT b{}; GetWindowRect(g_hBtnNew, &b); MapWindowPoints(nullptr, hWnd, (LPPOINT)&b, 2);
        HFONT oldF = (HFONT)SelectObject(hdc, GetSmall(hWnd));
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, UiTheme::Color_TextMuted());
        RECT hr{ Sc(hWnd,8), b.top - Sc(hWnd,14), b.left + Sc(hWnd,220), b.top - Sc(hWnd,2) };
        DrawTextW(hdc, L"Right-click a combo to enable/disable", -1, &hr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc, oldF);
    }

    // ── "Combo name" label ──────────────────────────────────────
    if (g_hEditName && IsWindow(g_hEditName)) {
        RECT r{}; GetWindowRect(g_hEditName, &r); MapWindowPoints(nullptr, hWnd, (LPPOINT)&r, 2);
        HFONT oldF = (HFONT)SelectObject(hdc, GetSmall(hWnd));
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, UiTheme::Color_TextMuted());
        RECT lr{ r.left, r.top - Sc(hWnd,14), r.right, r.top - Sc(hWnd,2) };
        DrawTextW(hdc, L"COMBO NAME", -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldF);
    }

    // ── Trigger card ─────────────────────────────────────────
    {
        RECT card{};
        if (CardRect(hWnd, g_hLblTrigger, g_hBtnCapture, Sc(hWnd, 10), Sc(hWnd, 8), card)) {
            FillRoundRect(hdc, card, Sc(hWnd, 7), Pal::CardBgTrig, Pal::CardBorderTrig);
            DrawCardLabel(hdc, hWnd, card, L"TRIGGER");
        }
    }

    // ── OPTIONS card ─────────────────────────────────────────────
    {
        RECT card{};
        if (CardRect(hWnd, g_hChkEnabled, g_hDelayValue, Sc(hWnd, 10), Sc(hWnd, 8), card)) {
            FillRoundRect(hdc, card, Sc(hWnd, 7), Pal::CardBg, Pal::CardBorder);
            DrawCardLabel(hdc, hWnd, card, L"OPTIONS");

        // Field labels inside OPTIONS (drawn here instead of STATIC controls)
        {
            HFONT oldF = (HFONT)SelectObject(hdc, GetSmall(hWnd));
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, UiTheme::Color_TextMuted());

            // "Run N times (0=inf):"
            if (g_hEditRepeatCount && IsWindow(g_hEditRepeatCount)) {
                RECT er{}; GetWindowRect(g_hEditRepeatCount, &er);
                MapWindowPoints(nullptr, hWnd, (LPPOINT)&er, 2);
                RECT lr{ card.left + Sc(hWnd, 10), er.top, er.left - Sc(hWnd, 6), er.bottom };
                DrawTextW(hdc, L"Run N times (0=inf):", -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }

            // "Repeat delay:"
            if (g_hEditDelay && IsWindow(g_hEditDelay)) {
                RECT ed{}; GetWindowRect(g_hEditDelay, &ed);
                MapWindowPoints(nullptr, hWnd, (LPPOINT)&ed, 2);
                RECT lr{ card.left + Sc(hWnd, 10), ed.top, ed.left - Sc(hWnd, 6), ed.bottom };
                DrawTextW(hdc, L"Repeat delay:", -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }

            SelectObject(hdc, oldF);
        }
        }
    }

    // ── ACTIONS card ─────────────────────────────────────────────
    {
        RECT card{};
        if (CardRect(hWnd, g_hActionList, g_hBtnSave, Sc(hWnd, 10), Sc(hWnd, 8), card)) {
            FillRoundRect(hdc, card, Sc(hWnd, 7), Pal::CardBg, Pal::CardBorder);
            DrawCardLabel(hdc, hWnd, card, L"ACTIONS");
        }
    }

    // ── WATCHMAN label — texte bold + lanterne PNG à droite (36px) ────────
    if (g_hWlModeCB && IsWindow(g_hWlModeCB)) {
        RECT wr{};
        GetWindowRect(g_hWlModeCB, &wr);
        MapWindowPoints(nullptr, hWnd, (LPPOINT)&wr, 2);

        // Zone du label : 36px au-dessus du dropdown
        int labelH   = Sc(hWnd, 36);
        int labelTop = wr.top - labelH;
        int labelBot = wr.top;
        int labelL   = wr.left;
        int labelR   = wr.right;

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, UiTheme::Color_Text());

        // Chargement lazy de la lanterne
        if (!g_lanternBmp) {
            std::wstring path = WinUtil_BuildPathNearExe(L"assets\\watchman_lantern_46.png");
            Gdiplus::Bitmap* b = Gdiplus::Bitmap::FromFile(path.c_str(), FALSE);
            if (b && b->GetLastStatus() == Gdiplus::Ok) g_lanternBmp = b;
            else if (b) delete b;
        }

        //int imgSz = Sc(hWnd, 26); // <-- original
        int imgSz = Sc(hWnd, 46);          // lantern icon size in pixels — ↑ = larger / ↓ = smaller - taille icône lanterne en pixels — ↑ = plus grande / ↓ = plus petite

        //int imgX = labelL - Sc(hWnd, 3);   // ← → horizontal position of lantern icon <-- original - ← → position horizontale icône lanterne <-- original 
        // ⚠️ also change the name of the png file above to match    
        // ⚠️ change aussi le nom du fichier png ci-dessus pour correspondre


        int imgX = labelL - Sc(hWnd, 13);   // horizontal position of lantern icon ← → position horizontale icône lanterne

        // labelL = bord gauche du panneau déroulant
        // - Sc(hWnd, 3) = légèrement à GAUCHE du panneau déroulant
        // augmenter le 3 = plus à gauche
        // remplacer - par + = décale à droite
        // + Sc(hWnd, 0) = aligné exactement sur le bord gauche

        // labelL = left edge of drop-down panel
        // - Sc(hWnd, 3) = slightly to the LEFT of the drop-down panel
        // increase the 3 = further to the left
        // replace - with + = shifts to the right
        // + Sc(hWnd, 0) = aligned exactly with the left edge



        //int imgY = labelTop + (labelH - imgSz) / 5 - Sc(hWnd, 1); // <-- original 
        int imgY = labelTop + (labelH - imgSz) / 4 - Sc(hWnd, 1); // ↕ vertical position of lantern icon ↕ position verticale icône lanterne
        // Icône plus basse :↑ augmenter /N → /9, /15..., Icône plus haute: diminuer /N → /4, /2, /1
        // - Sc(hWnd, 1) = micro-ajustement vers le haut, changer ou mettre 0
        // Micro-ajustement bas- Sc(hWnd, valeur négative) ex: - Sc(hWnd, -4)Micro-ajustement haut- Sc(hWnd, valeur positive) ex: - Sc(hWnd, 4)

        // Lower icon:↑ increase /N → /9, /15..., Higher icon: decrease /N → /4, /2, /1
        // - Sc(hWnd, 1) = micro-adjustment upwards, change or set to 0
        // Micro-adjustment down - Sc(hWnd, negative value) e.g.: - Sc(hWnd, -4) Micro-adjustment up - Sc(hWnd, positive value) e.g.: - 


        if (g_lanternBmp) {
            Gdiplus::Graphics g(hdc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
            g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
            g.DrawImage(g_lanternBmp,
                        imgX, imgY,
                        imgSz, imgSz);
        }

        // Texte "WATCHMAN" bold — décalé après la lanterne, centré verticalement
        HFONT oldBold = (HFONT)SelectObject(hdc, GetBold(hWnd));
        RECT tr{ imgX + imgSz + Sc(hWnd, 4), labelTop, labelR, labelBot };
        DrawTextW(hdc, L"WATCHMAN", -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, oldBold);
    }

    // ── "Type" label (drawn here instead of STATIC) ────────────────
    if (g_hActionTypeCB && IsWindow(g_hActionTypeCB)) {
        RECT r{}; GetWindowRect(g_hActionTypeCB, &r); MapWindowPoints(nullptr, hWnd, (LPPOINT)&r, 2);
        HFONT oldF = (HFONT)SelectObject(hdc, GetSmall(hWnd));
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, UiTheme::Color_TextMuted());
        RECT lr{ r.left - Sc(hWnd, 48), r.top, r.left - Sc(hWnd, 6), r.bottom };
        DrawTextW(hdc, L"Type:", -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldF);
    }

// ── "Type / Value" label section actions ─────────────────────
    if (g_hActionKeyEdt && IsWindow(g_hActionKeyEdt)) {
        RECT r{}; GetWindowRect(g_hActionKeyEdt, &r); MapWindowPoints(nullptr, hWnd, (LPPOINT)&r, 2);
        HFONT oldF = (HFONT)SelectObject(hdc, GetSmall(hWnd));
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, UiTheme::Color_TextMuted());
        RECT lr{ r.left, r.top - Sc(hWnd,14), r.right, r.top - Sc(hWnd,2) };
        DrawTextW(hdc, L"VALUE  (key / ms / text)", -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldF);
    }
}

// ────────────────────────────────────────────────────────────────────
// Refresh helpers
// ────────────────────────────────────────────────────────────────────
static void RefreshTriggerLabel()
{
    if (!g_hLblTrigger || g_selectedId < 0) return;
    FreeCombo* c = FreeComboSystem::GetCombo(g_selectedId);
    if (!c) return;
    SetWindowTextW(g_hLblTrigger,
        c->trigger.IsValid() ? c->trigger.ToString().c_str() : L"(not configured)");
}

static void RefreshActionList()
{
    if (!g_hActionList) return;
    LB_RESET(g_hActionList);
    if (g_selectedId < 0) return;
    FreeCombo* c = FreeComboSystem::GetCombo(g_selectedId);
    if (!c) return;
    for (size_t i = 0; i < c->actions.size(); ++i) LB_ADDRAW(g_hActionList);
    // Forcer redessin
    InvalidateRect(g_hActionList, nullptr, TRUE);
}

static void UpdateControlsEnabled()
{
    bool has = (g_selectedId >= 0);
    auto En = [&](HWND h) { if (h && IsWindow(h)) EnableWindow(h, has); };
    En(g_hEditName);    En(g_hLblTrigger);
    En(g_hBtnCapture);  En(g_hChkEnabled); En(g_hChkRepeat);
    if (g_hChkCancelRel)    EnableWindow(g_hChkCancelRel,    has);
    if (g_hEditRepeatCount) EnableWindow(g_hEditRepeatCount, has);
    En(g_hEditDelay);   En(g_hDelaySlider); En(g_hDelayValue);
    if (g_hChkCancelRel)    EnableWindow(g_hChkCancelRel,    has);
    if (g_hEditRepeatCount) EnableWindow(g_hEditRepeatCount, has);
    if (g_hChkLongPress)    EnableWindow(g_hChkLongPress, has);
    if (g_hEditLongPressMs) EnableWindow(g_hEditLongPressMs,
        has && g_hChkLongPress &&
        SendMessageW(g_hChkLongPress, BM_GETCHECK, 0, 0) == BST_CHECKED);
    En(g_hActionList);  En(g_hActionTypeCB); En(g_hActionKeyEdt);
    En(g_hBtnAdd);      En(g_hBtnAddDelay);  En(g_hBtnDelAct);
    En(g_hBtnUp);       En(g_hBtnDown);      En(g_hBtnSave);
    En(g_hBtnDelete);
    // Mouse capture button: only enabled when "Mouse click" type is selected
    if (g_hBtnCaptureMouse && IsWindow(g_hBtnCaptureMouse)) {
        int ti = g_hActionTypeCB ? CB_GETSEL(g_hActionTypeCB) : -1;
        EnableWindow(g_hBtnCaptureMouse, has && ti == 4);
    }
}

static void LoadComboToUI()
{
    if (g_selectedId < 0) { UpdateControlsEnabled(); return; }
    FreeCombo* c = FreeComboSystem::GetCombo(g_selectedId);
    if (!c) { UpdateControlsEnabled(); return; }
    SetWindowTextW(g_hEditName, c->name.c_str());
    CHK_SET(g_hChkRepeat, c->repeatWhileHeld);
    CHK_SET(g_hChkEnabled, c->enabled);
    SetDelayUi((int)c->repeatDelayMs);
    if (g_hChkCancelRel)    CHK_SET(g_hChkCancelRel, c->cancelOnRelease);
    if (g_hEditRepeatCount) SetWindowTextInt(g_hEditRepeatCount, (int)c->repeatCount);
    if (g_hChkLongPress) {
        CHK_SET(g_hChkLongPress, c->longPressEnabled);
        if (g_hEditLongPressMs) {
            SetWindowTextInt(g_hEditLongPressMs, (int)c->longPressMs);
            EnableWindow(g_hEditLongPressMs, c->longPressEnabled);
        }
    }
    RefreshTriggerLabel();
    RefreshActionList();
    UpdateControlsEnabled();
}

// ────────────────────────────────────────────────────────────────────
// WndProc forward declaration
// ────────────────────────────────────────────────────────────────────
static LRESULT CALLBACK FreeComboPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void EnsureClass(HINSTANCE hInst)
{
    static bool s = false; if (s)return;
    WNDCLASSW wc{}; wc.lpfnWndProc = FreeComboPageProc; wc.hInstance = hInst;
    wc.lpszClassName = L"FreeComboPage"; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc); s = true;
}

// ────────────────────────────────────────────────────────────────────
// namespace FreeComboUI  –  public API (end)
// ────────────────────────────────────────────────────────────────────
namespace FreeComboUI
{
    // ----------------------------------------------------------------
    void RefreshComboList()
    {
        if (!g_hComboList) return;
        int wantId = g_selectedId;
        LB_RESET(g_hComboList);
        g_comboIds.clear();
        for (int id : FreeComboSystem::GetAllIds()) {
            if (!FreeComboSystem::GetCombo(id)) continue;
            LB_ADDRAW(g_hComboList);
            g_comboIds.push_back(id);
        }
        if (g_comboIds.empty()) { g_selectedId = -1; UpdateControlsEnabled(); return; }
        int sel = 0;
        for (int i = 0; i < (int)g_comboIds.size(); ++i)
            if (g_comboIds[i] == wantId) { sel = i; break; }
        LB_SETCUR(g_hComboList, sel);
        g_selectedId = g_comboIds[(size_t)sel];
        LoadComboToUI();
        InvalidateRect(g_hComboList, nullptr, FALSE);
    }

    void SelectCombo(int id)
    {
        g_selectedId = id;
        for (int i = 0; i < (int)g_comboIds.size(); ++i)
            if (g_comboIds[i] == id) { LB_SETCUR(g_hComboList, i); break; }
        LoadComboToUI();
    }

    // ----------------------------------------------------------------
    void OnCommand(HWND hWnd, WORD ctlId, WORD notif)
    {
        switch (ctlId) {

        case ID_COMBO_LIST:
            if (notif == LBN_SELCHANGE) {
                int sel = LB_SEL(g_hComboList);
                if (sel >= 0 && sel < (int)g_comboIds.size()) {
                    g_selectedId = g_comboIds[(size_t)sel];
                    LoadComboToUI();
                    if (ActivateIfSameTrigger()) { RefreshComboList(); PersistToDisk(); }
                }
            }
            break;

        case ID_WL_MODE_CB:
            if (notif == CBN_SELCHANGE && g_hWlModeCB) {
                int sel = (int)SendMessageW(g_hWlModeCB, CB_GETCURSEL, 0, 0);
                FreeComboSystem::SetWhitelistMode(sel);
            }
            break;

        case ID_WL_BTN_ADD: {
            if (!g_hWlEdit) break;
            wchar_t buf[128] = {};
            GetWindowTextW(g_hWlEdit, buf, 128);
            std::wstring s(buf);
            while (!s.empty() && (s.front()==L' '||s.front()==L'	')) s.erase(s.begin());
            while (!s.empty() && (s.back()==L' '||s.back()==L'	')) s.pop_back();
            if (!s.empty()) {
                if (s.size() < 4 || _wcsicmp(s.c_str()+s.size()-4, L".exe") != 0)
                    s += L".exe";
                FreeComboSystem::AddToWhitelist(s);
                if (g_hWlList) SendMessageW(g_hWlList, LB_ADDSTRING, 0, (LPARAM)s.c_str());
                SetWindowTextW(g_hWlEdit, L"");
                PersistToDisk();
            }
            break;
        }

        case ID_WL_BTN_DEL: {
            if (!g_hWlList) break;
            int idx = (int)SendMessageW(g_hWlList, LB_GETCURSEL, 0, 0);
            if (idx != LB_ERR) {
                wchar_t buf[128] = {};
                SendMessageW(g_hWlList, LB_GETTEXT, idx, (LPARAM)buf);
                FreeComboSystem::RemoveFromWhitelist(std::wstring(buf));
                SendMessageW(g_hWlList, LB_DELETESTRING, idx, 0);
                PersistToDisk();
            }
            break;
        }

        case ID_BTN_NEW: {
            int newId = FreeComboSystem::CreateCombo(L"New combo");
            RefreshComboList(); SelectCombo(newId);
            if (g_hEditName) { SetFocus(g_hEditName); SendMessageW(g_hEditName, EM_SETSEL, 0, -1); }
            PersistToDisk();
            break;
        }
        case ID_BTN_DELETE:
            if (g_selectedId >= 0) {
                FreeComboSystem::DeleteCombo(g_selectedId);
                g_selectedId = -1; RefreshComboList(); PersistToDisk();
            }
            break;

        case ID_BTN_SAVE: {
            if (g_selectedId < 0) break;
            FreeCombo* c = FreeComboSystem::GetCombo(g_selectedId); if (!c)break;
            wchar_t buf[256]{}; GetWindowTextW(g_hEditName, buf, 256);
            c->name = buf; c->repeatWhileHeld = CHK_GET(g_hChkRepeat);
            c->enabled = CHK_GET(g_hChkEnabled);
            c->repeatDelayMs = (uint32_t)ClampDelay(GetWindowTextInt(g_hEditDelay));
            if (g_hChkCancelRel)    c->cancelOnRelease = CHK_GET(g_hChkCancelRel);
            if (g_hChkLongPress)    c->longPressEnabled = CHK_GET(g_hChkLongPress);
            if (g_hEditLongPressMs) c->longPressMs =
                (uint32_t)std::clamp(GetWindowTextInt(g_hEditLongPressMs), 50, 5000);
            if (g_hEditRepeatCount) {
                int rc = GetWindowTextInt(g_hEditRepeatCount);
                c->repeatCount = (rc >= 0 && rc <= 9999) ? (uint32_t)rc : 0;
            }
            if (c->enabled) DisableOtherCombosWithSameTrigger(g_selectedId);
            SetDelayUi((int)c->repeatDelayMs); RefreshComboList(); PersistToDisk();
            break;
        }
        case ID_BTN_CAPTURE:
            if (!g_capturing) {
                g_capturing = true;
                FreeComboSystem::StartCapture();
                SetWindowTextW(g_hBtnCapture, L"\u23FA  Capturing...");
                EnableWindow(g_hBtnCapture, FALSE);
                SetWindowTextW(g_hLblTrigger, L"Press 1 or 2 keys/buttons...");
                InvalidateRect(g_hPage, nullptr, FALSE);
                SetTimer(g_hPage, ID_TIMER_CAPTURE, 50, nullptr);
            }
            break;

        case ID_BTN_CAPTURE_MOUSE:
            if (!g_capturingMouseAction) {
                g_capturingMouseAction = true;
                SetWindowTextW(g_hBtnCaptureMouse, L"\u23FA ...");
                EnableWindow(g_hBtnCaptureMouse, FALSE);
                SetWindowTextW(g_hActionKeyEdt, L"Click a mouse button...");
                // Disable the Add button during capture to avoid accidental adds
                EnableWindow(g_hBtnAdd, FALSE);
                g_captureGraceUntil = GetTickCount() + 250; // 250ms grace — ignore the activating click
                SetTimer(g_hPage, ID_TIMER_MOUSE_CAPTURE, 16, nullptr); // poll at ~60fps
            }
            break;

        case ID_BTN_ADD_ACTION: {
            if (g_selectedId < 0) break;
            int ti = CB_GETSEL(g_hActionTypeCB); if (ti < 0)break;
            ComboAction action{};
            // Map combobox index → ComboActionType using the same order as ActionTypeIdx()
            // This is safe regardless of enum underlying values
            static const ComboActionType kTypeMap[] = {
                ComboActionType::PressKey,   // 0
                ComboActionType::ReleaseKey, // 1
                ComboActionType::TapKey,     // 2
                ComboActionType::TypeText,   // 3
                ComboActionType::MouseClick, // 4
                ComboActionType::Delay,      // 5
            };
            action.type = (ti >= 0 && ti < 6) ? kTypeMap[ti] : ComboActionType::TapKey;
            wchar_t kbuf[256]{}; GetWindowTextW(g_hActionKeyEdt, kbuf, 256);

            if (action.type == ComboActionType::PressKey ||
                action.type == ComboActionType::ReleaseKey ||
                action.type == ComboActionType::TapKey)
            {
                WORD vk = 0;
                if (wcslen(kbuf) == 1) {
                    wchar_t c2 = towupper(kbuf[0]);
                    if ((c2 >= L'A' && c2 <= L'Z') || (c2 >= L'0' && c2 <= L'9'))vk = (WORD)c2;
                }
                else {
                    struct { const wchar_t* s; WORD vk; }map[] = {
                        {L"F1",VK_F1},{L"F2",VK_F2},{L"F3",VK_F3},{L"F4",VK_F4},
                        {L"F5",VK_F5},{L"F6",VK_F6},{L"F7",VK_F7},{L"F8",VK_F8},
                        {L"F9",VK_F9},{L"F10",VK_F10},{L"F11",VK_F11},{L"F12",VK_F12},
                        {L"Space",VK_SPACE},{L"Enter",VK_RETURN},{L"Tab",VK_TAB},
                        {L"Esc",VK_ESCAPE},{L"Delete",VK_DELETE},
                        {L"Shift",VK_SHIFT},{L"Ctrl",VK_CONTROL},{L"Control",VK_CONTROL},
                        {L"Alt",VK_MENU},{L"Win",VK_LWIN},{L"Windows",VK_LWIN},
                        {L"Up",VK_UP},{L"Down",VK_DOWN},{L"Left",VK_LEFT},{L"Right",VK_RIGHT},
                    };
                    for (auto& e : map) if (_wcsicmp(kbuf, e.s) == 0) { vk = e.vk; break; }
                }
                action.keyHid = VkToHid(vk);
            }
            else if (action.type == ComboActionType::Delay)    action.delayMs = _wtoi(kbuf);
            else if (action.type == ComboActionType::TypeText)  action.text = kbuf;
            else if (action.type == ComboActionType::MouseClick) {
                if (_wcsicmp(kbuf, L"right") == 0 || _wcsicmp(kbuf, L"right click") == 0) action.mouseButton = 1;
                else if (_wcsicmp(kbuf, L"middle") == 0 || _wcsicmp(kbuf, L"middle click") == 0) action.mouseButton = 2;
                else if (_wcsicmp(kbuf, L"x1") == 0 || _wcsicmp(kbuf, L"x1 (thumb)") == 0) action.mouseButton = 3;
                else if (_wcsicmp(kbuf, L"x2") == 0 || _wcsicmp(kbuf, L"x2 (thumb2)") == 0) action.mouseButton = 4;
                // else: left click = 0 (default)
            }
            FreeComboSystem::AddAction(g_selectedId, action);
            RefreshActionList();
            { int n = (int)SendMessageW(g_hActionList, LB_GETCOUNT, 0, 0); if (n > 0)LB_SETCUR(g_hActionList, n - 1); }
            PersistToDisk();
            break;
        }
        case ID_BTN_ADD_DELAY: {
            if (g_selectedId < 0) break;
            ComboAction a{}; a.type = ComboActionType::Delay;
            a.delayMs = (uint32_t)ClampDelay((int)SendMessageW(g_hDelaySlider, TBM_GETPOS, 0, 0));
            FreeComboSystem::AddAction(g_selectedId, a);
            RefreshActionList(); PersistToDisk();
            break;
        }
        case ID_BTN_DEL_ACTION: {
            if (g_selectedId < 0) break;
            int sel = LB_SEL(g_hActionList); if (sel < 0)break;
            FreeComboSystem::RemoveAction(g_selectedId, sel);
            RefreshActionList(); PersistToDisk();
            break;
        }
        case ID_BTN_UP_ACTION: {
            if (g_selectedId < 0) break;
            int sel = LB_SEL(g_hActionList);
            FreeComboSystem::MoveActionUp(g_selectedId, sel);
            RefreshActionList(); if (sel > 0)LB_SETCUR(g_hActionList, sel - 1);
            PersistToDisk(); break;
        }
        case ID_BTN_DOWN_ACTION: {
            if (g_selectedId < 0) break;
            int sel = LB_SEL(g_hActionList);
            FreeComboSystem::MoveActionDown(g_selectedId, sel);
            RefreshActionList(); LB_SETCUR(g_hActionList, sel + 1);
            PersistToDisk(); break;
        }

        case ID_CHK_CANCEL_RELEASE:
            if (g_selectedId >= 0 && notif == BN_CLICKED)
                if (FreeCombo* c = FreeComboSystem::GetCombo(g_selectedId)) {
                    c->cancelOnRelease = CHK_GET(g_hChkCancelRel); PersistToDisk();
                }
            break;
        case ID_EDIT_REPEAT_COUNT:
            if (g_selectedId >= 0 && notif == EN_KILLFOCUS)
                if (FreeCombo* c = FreeComboSystem::GetCombo(g_selectedId)) {
                    int rc = GetWindowTextInt(g_hEditRepeatCount);
                    c->repeatCount = (rc >= 0 && rc <= 9999) ? (uint32_t)rc : 0;
                    PersistToDisk();
                }
            break;
        case ID_CHK_LONG_PRESS:
            if (notif == BN_CLICKED) {
                bool on = g_hChkLongPress &&
                    SendMessageW(g_hChkLongPress, BM_GETCHECK, 0, 0) == BST_CHECKED;
                if (g_hEditLongPressMs) EnableWindow(g_hEditLongPressMs, on);
                if (g_selectedId >= 0)
                    if (FreeCombo* c = FreeComboSystem::GetCombo(g_selectedId)) {
                        c->longPressEnabled = on; PersistToDisk();
                    }
            }
            break;
        case ID_EDIT_LONG_PRESS_MS:
            if (notif == EN_KILLFOCUS && g_selectedId >= 0)
                if (FreeCombo* c = FreeComboSystem::GetCombo(g_selectedId)) {
                    int ms = GetWindowTextInt(g_hEditLongPressMs);
                    c->longPressMs = (uint32_t)std::clamp(ms, 50, 5000);
                    PersistToDisk();
                }
            break;
        } // switch
    }

    // ----------------------------------------------------------------
    void OnTimer()
    {
        if (!FreeComboSystem::IsCapturing() && FreeComboSystem::IsCaptureComplete()) {
            KillTimer(g_hPage, ID_TIMER_CAPTURE);
            g_capturing = false;
            if (g_selectedId >= 0) {
                FreeTrigger t = FreeComboSystem::GetCapturedTrigger();
                FreeComboSystem::SetTrigger(g_selectedId, t);
                if (FreeCombo* c = FreeComboSystem::GetCombo(g_selectedId))
                    if (c->enabled) DisableOtherCombosWithSameTrigger(g_selectedId);
                RefreshTriggerLabel();
                RefreshComboList();
                PersistToDisk();
            }
            SetWindowTextW(g_hBtnCapture, L"\u25CF  Capture trigger");
            EnableWindow(g_hBtnCapture, TRUE);
            InvalidateRect(g_hPage, nullptr, FALSE);
            return;
        }
        if (FreeComboSystem::IsCapturing() && FreeComboSystem::IsCaptureWaitingSecondInput()) {
            FreeTrigger first = FreeComboSystem::GetCaptureFirstInput();
            SetWindowTextW(g_hLblTrigger,
                (L"1st: " + first.ToString() + L"   ->   press 2nd...").c_str());
        }
    }

    // ================================================================
    // CreatePage
    // ================================================================
    HWND CreatePage(HWND parent, HINSTANCE hInst)
    {
        EnsureClass(hInst);
        g_hPage = CreateWindowExW(0, L"FreeComboPage", L"",
            WS_CHILD | WS_CLIPCHILDREN, 0, 0, 0, 0, parent, nullptr, hInst, nullptr);

        // ── Layout constants (base 96dpi coordinates) ──
        //    Uses raw pixel values; WM_SIZE recalculates everything DPI-aware.
        const int pad = 10;
        const int lw = 220;  // left column width
        const int lx = pad;
        // Keep the left list aligned with the painted "My combos" header block.
        const int myCombosLabelTop = 3;
        const int myCombosLabelH = 38;
        const int myCombosGap = 6;
        const int ly = myCombosLabelTop + myCombosLabelH + myCombosGap;
        const int listH = 252;  // initial list height
        const int btnH = 26;
        const int rx = lw + pad * 2 + 2;   // right column start
        const int rw = 352;              // right column width
        const int rowH = 24;
        const int spad = 8;    // inner section padding

        // ── Left column ────────────────────────────────────────
        g_hComboList = CreateWindowExW(0, L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | WS_VSCROLL,
            lx, ly, lw, listH, g_hPage, (HMENU)ID_COMBO_LIST, hInst, nullptr);
        SendMessageW(g_hComboList, LB_SETITEMHEIGHT, 0, 42);  // taller for trigger preview
        // Subclass pour intercepter WM_LBUTTONDOWN (drag & drop)
        g_hPageForSubclass = g_hPage;
        g_origComboListProc = (WNDPROC)SetWindowLongPtrW(g_hComboList, GWLP_WNDPROC,
            (LONG_PTR)ComboListSubclassProc);

        int btnY = ly + listH + 6;
        int bw2 = (lw - 6) / 2;
        g_hBtnNew = CreateWindowExW(0, L"BUTTON", L"+ New",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            lx, btnY, bw2, btnH, g_hPage, (HMENU)ID_BTN_NEW, hInst, nullptr);
        g_hBtnDelete = CreateWindowExW(0, L"BUTTON", L"\u2212 Delete",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            lx + bw2 + 6, btnY, bw2, btnH, g_hPage, (HMENU)ID_BTN_DELETE, hInst, nullptr);

        // ── Whitelist panel (colonne gauche, sous New/Delete) ────────────────────
        {
            int wy = btnY + btnH + 12;

            g_hWlModeCB = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS,
                lx, wy, lw, 200, g_hPage, (HMENU)ID_WL_MODE_CB, hInst, nullptr);
            SendMessageW(g_hWlModeCB, CB_ADDSTRING, 0, (LPARAM)L"OFF  -  Inject everywhere");
            SendMessageW(g_hWlModeCB, CB_ADDSTRING, 0, (LPARAM)L"WHITELIST  -  Allowed apps only");
            SendMessageW(g_hWlModeCB, CB_SETCURSEL, 0, 0);
            UiTheme::ApplyToControl(g_hWlModeCB);
            if (HFONT f = GetFont(g_hPage))
                SendMessageW(g_hWlModeCB, WM_SETFONT, (WPARAM)f, FALSE);
            wy += rowH + 4;

            g_hWlList = CreateWindowExW(0, L"LISTBOX", L"",
                WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL,
                lx, wy, lw, 60, g_hPage, (HMENU)ID_WL_LIST, hInst, nullptr);
            UiTheme::ApplyToControl(g_hWlList);
            // Repeupler la ListBox depuis la whitelist chargée depuis le .dat
            {
                auto wl = FreeComboSystem::GetWhitelist();
                for (const auto& entry : wl)
                    SendMessageW(g_hWlList, LB_ADDSTRING, 0, (LPARAM)entry.c_str());
            }
            wy += 64;

            int ebw = lw - 54;
            g_hWlEdit = CreateWindowExW(0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                lx, wy, ebw, rowH, g_hPage, (HMENU)ID_WL_EDIT, hInst, nullptr);
            UiTheme::ApplyToControl(g_hWlEdit);

            g_hWlBtnAdd = CreateWindowExW(0, L"BUTTON", L"+",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                lx + ebw + 2, wy, 24, rowH, g_hPage, (HMENU)ID_WL_BTN_ADD, hInst, nullptr);
            g_hWlBtnDel = CreateWindowExW(0, L"BUTTON", L"-",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                lx + ebw + 28, wy, 24, rowH, g_hPage, (HMENU)ID_WL_BTN_DEL, hInst, nullptr);
        }

        // ── Right column ────────────────────────────────────────
        int ry = pad;

        // Nom
        ry += 16; // space for the "COMBO NAME" label (drawn in WM_PAINT)
        g_hEditName = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            rx, ry, rw, rowH, g_hPage, (HMENU)ID_EDIT_NAME, hInst, nullptr);
        ry += rowH + 12;

        // ─ Trigger card ─
        ry += 18; // card label (drawn in WM_PAINT)

        g_hLblTrigger = CreateWindowExW(0, L"STATIC", L"(not configured)",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            rx, ry, rw, rowH, g_hPage, (HMENU)ID_LBL_TRIGGER, hInst, nullptr);
        ry += rowH + 6;

        g_hBtnCapture = CreateWindowExW(0, L"BUTTON", L"\u25CF  Capture trigger",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rx, ry, 204, btnH, g_hPage, (HMENU)ID_BTN_CAPTURE, hInst, nullptr);
        ry += btnH + 14;

        // ─ Options card ─
        ry += 18;

        // ── OPTIONS — 4 separated lines (28px each)  -- lignes bien séparées (28px chacune) ──────────────
        // Ligne 1 : [Combo enabled]        [Repeat while held]
        g_hChkEnabled = CreateWindowExW(0, L"BUTTON", L"Combo enabled",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            rx, ry, 148, 20, g_hPage, (HMENU)ID_CHK_ENABLED, hInst, nullptr);
        CHK_SET(g_hChkEnabled, true);
        g_hChkRepeat = CreateWindowExW(0, L"BUTTON", L"Repeat while held",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            rx + 156, ry, 196, 20, g_hPage, (HMENU)ID_CHK_REPEAT, hInst, nullptr);
        ry += 28;

        // Ligne 2 : [Run N times (0=inf): label] [edit 44px]   [Cancel on release]
        {
            // (Label drawn in WM_PAINT) Run N times (0=inf):
            g_hEditRepeatCount = CreateWindowExW(0, L"EDIT", L"0",
                WS_CHILD | WS_VISIBLE | ES_NUMBER | WS_TABSTOP,
                rx + 156, ry, 44, rowH, g_hPage, (HMENU)ID_EDIT_REPEAT_COUNT, hInst, nullptr);
            g_hChkCancelRel = CreateWindowExW(0, L"BUTTON", L"Cancel on release",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                rx + 210, ry, 180, 20, g_hPage, (HMENU)ID_CHK_CANCEL_RELEASE, hInst, nullptr);
        }
        ry += 28;

        // Long press row
        {
            g_hChkLongPress = CreateWindowExW(0, L"BUTTON", L"Hold delay (ms):",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                rx, ry, 140, 20, g_hPage, (HMENU)ID_CHK_LONG_PRESS, hInst, nullptr);
            g_hEditLongPressMs = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"300",
                WS_CHILD | WS_VISIBLE | ES_NUMBER,
                rx + 144, ry, 52, rowH, g_hPage, (HMENU)ID_EDIT_LONG_PRESS_MS, hInst, nullptr);
            EnableWindow(g_hEditLongPressMs, FALSE); // grisé par défaut
        }

        // Ligne 3 : [Repeat delay: label] [edit 52px ms]
        {
            // (Label drawn in WM_PAINT) Repeat delay:
g_hEditDelay = CreateWindowExW(0, L"EDIT", L"400", WS_CHILD | WS_VISIBLE | ES_NUMBER,
                rx + 146, ry, 52, rowH, g_hPage, (HMENU)ID_EDIT_DELAY, hInst, nullptr);
        }
        ry += 28;

        // Ligne 4 : [slider] [400 ms label]
        g_hDelaySlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            rx, ry, rw - 62, 22, g_hPage, (HMENU)ID_SLIDER_DELAY, hInst, nullptr);
        SendMessageW(g_hDelaySlider, TBM_SETRANGE, FALSE, MAKELONG(10, 2000));
        SendMessageW(g_hDelaySlider, TBM_SETPAGESIZE, 0, 50);
        g_hDelayValue = CreateWindowExW(0, L"STATIC", L"400 ms", WS_CHILD | WS_VISIBLE | SS_RIGHT,
            rx + rw - 58, ry, 58, 22, g_hPage, (HMENU)ID_LBL_DELAY_VALUE, hInst, nullptr);
        ry += 30 + 12;

        // ─ Actions card ─
        ry += 18;

        g_hActionList = CreateWindowExW(0, L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | WS_VSCROLL,
            rx, ry, rw, 100, g_hPage, (HMENU)ID_ACTION_LIST, hInst, nullptr);
        SendMessageW(g_hActionList, LB_SETITEMHEIGHT, 0, 24);
        // Subclass for intercept WM_LBUTTONDOWN (drag & drop + clic [x])
        g_origActionListProc = (WNDPROC)SetWindowLongPtrW(g_hActionList, GWLP_WNDPROC,
            (LONG_PTR)ActionListSubclassProc);
        ry += 106;

        // Action type (label drawn in WM_PAINT)
g_hActionTypeCB = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            rx + 48, ry, rw - 48, 300, g_hPage, (HMENU)ID_ACTION_TYPE_CB, hInst, nullptr);
        // Items added AFTER ApplyFontChildren below — WM_SETFONT on combobox clears items on some Win32 builds
        ry += rowH + 4;

        // Label "VALUE" (WM_PAINT) + value edit + mouse capture button
        // The mouse capture button (🖱) appears to the right of the value field,
        // but is only shown when "Mouse click" action type is selected.
        ry += 16;
        const int kCapW = 30; // width of the 🖱 capture button
        g_hActionKeyEdt = CreateWindowExW(0, L"EDIT", L"P", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            rx, ry, rw - kCapW - 4, rowH, g_hPage, (HMENU)ID_ACTION_KEY_EDIT, hInst, nullptr);
        // Must create with WS_VISIBLE so BS_OWNERDRAW registers WM_DRAWITEM correctly.
        // We hide it immediately after — it shows only when "Mouse click" type is selected.
        g_hBtnCaptureMouse = CreateWindowExW(0, L"BUTTON", L"\U0001F5B1",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rx + rw - kCapW, ry, kCapW, rowH, g_hPage, (HMENU)ID_BTN_CAPTURE_MOUSE, hInst, nullptr);
        ry += rowH + 6;

        // Buttons Add / Delay / Del (3 equal columns)
        const int w3 = (rw - 8) / 3;
        g_hBtnAdd = CreateWindowExW(0, L"BUTTON", L"\uFF0B  Add", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rx, ry, w3, btnH, g_hPage, (HMENU)ID_BTN_ADD_ACTION, hInst, nullptr);
        g_hBtnAddDelay = CreateWindowExW(0, L"BUTTON", L"\u23F1  + Delay", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rx + w3 + 4, ry, w3, btnH, g_hPage, (HMENU)ID_BTN_ADD_DELAY, hInst, nullptr);
        g_hBtnDelAct = CreateWindowExW(0, L"BUTTON", L"\u2212  Delete", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rx + (w3 + 4) * 2, ry, w3, btnH, g_hPage, (HMENU)ID_BTN_DEL_ACTION, hInst, nullptr);
        ry += btnH + 4;

        // Buttons Up / Down (2 columns)
        const int w2 = (rw - 4) / 2;
        g_hBtnUp = CreateWindowExW(0, L"BUTTON", L"\u25B2  Move up", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rx, ry, w2, btnH, g_hPage, (HMENU)ID_BTN_UP_ACTION, hInst, nullptr);
        g_hBtnDown = CreateWindowExW(0, L"BUTTON", L"\u25BC  Move down", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rx + w2 + 4, ry, w2, btnH, g_hPage, (HMENU)ID_BTN_DOWN_ACTION, hInst, nullptr);
        ry += btnH + 8;

        // Save button (full width, blue accent)
        g_hBtnSave = CreateWindowExW(0, L"BUTTON", L"\U0001F4BE   Save combo",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rx, ry, rw, btnH + 2, g_hPage, (HMENU)ID_BTN_SAVE, hInst, nullptr);

        // ── Dark theme + font ───────────────────────────────────
        auto ApplyTheme = [&](HWND h) { if (h) UiTheme::ApplyToControl(h); };
        ApplyTheme(g_hComboList); ApplyTheme(g_hEditName);   ApplyTheme(g_hLblTrigger);
        ApplyTheme(g_hBtnCapture); ApplyTheme(g_hActionList); ApplyTheme(g_hActionTypeCB);
        ApplyTheme(g_hActionKeyEdt); ApplyTheme(g_hBtnCaptureMouse);
        ApplyTheme(g_hChkRepeat); ApplyTheme(g_hChkEnabled);
        if (g_hChkCancelRel)    ApplyTheme(g_hChkCancelRel);
        if (g_hEditRepeatCount) ApplyTheme(g_hEditRepeatCount);
        ApplyTheme(g_hEditDelay); ApplyTheme(g_hDelaySlider); ApplyTheme(g_hDelayValue);

        ApplyFontChildren(g_hPage);
        ApplyFont(g_hBtnSave, true);
        ApplyFont(g_hBtnCapture, false);

        // Apply font to combobox manually (skipped by ApplyFontChildren) then add items
        if (HFONT f = GetFont(g_hPage))
            SendMessageW(g_hActionTypeCB, WM_SETFONT, (WPARAM)f, FALSE);
        for (int i = 0; i < 6; ++i) CB_ADD(g_hActionTypeCB, ACTION_NAMES[i]);
        CB_SETSEL(g_hActionTypeCB, 2);

        if (g_hBtnCaptureMouse) ShowWindow(g_hBtnCaptureMouse, SW_HIDE);
        SetDelayUi(400);
        RefreshComboList();
        UpdateControlsEnabled();


        return g_hPage;
    }

} // namespace FreeComboUI

// ────────────────────────────────────────────────────────────────────
// WndProc
// ────────────────────────────────────────────────────────────────────
static LRESULT CALLBACK FreeComboPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        // ── Rendering ─────────────────────────────────────────────────────
    case WM_ERASEBKGND: return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps{}; HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc{}; GetClientRect(hWnd, &rc);
        DblBuf buf;
        if (buf.Begin(hdc, rc.right, rc.bottom)) {
            FillRect(buf.mem, &rc, UiTheme::Brush_PanelBg());
            PaintPage(hWnd, buf.mem);
            FC_DrawScrollbar(hWnd, buf.mem);
            buf.End(hdc, rc.right, rc.bottom);
        }
        else { PaintPage(hWnd, hdc); FC_DrawScrollbar(hWnd, hdc); }
        EndPaint(hWnd, &ps);
        return 0;
    }

                 // ── Standard control colors ───────────────────────────────
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam; HWND ctl = (HWND)lParam;
        SetBkMode(hdc, TRANSPARENT);
        if (ctl == g_hLblTrigger) {
            SetTextColor(hdc, g_capturing ? Pal::CAPTUREOrange : UiTheme::Color_Text());
            SetBkColor(hdc, Pal::CardBgTrig);
            // Static brush to avoid memory leak
            static HBRUSH brTrig = CreateSolidBrush(Pal::CardBgTrig);
            return (LRESULT)brTrig;
        }
        SetTextColor(hdc, UiTheme::Color_Text());
        SetBkColor(hdc, UiTheme::Color_PanelBg());
        return (LRESULT)UiTheme::Brush_PanelBg();
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, UiTheme::Color_Text());
        SetBkColor(hdc, UiTheme::Color_ControlBg());
        return (LRESULT)UiTheme::Brush_ControlBg();
    }
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, UiTheme::Color_ControlBg());
        return (LRESULT)UiTheme::Brush_ControlBg();
    }

                       // ── Owner-draw ────────────────────────────────────────────────
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lParam;
        if (mis && mis->CtlID == FreeComboUI::ID_WL_MODE_CB)
            mis->itemHeight = Sc(hWnd, 22);
        return TRUE;
    }

    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lParam;
        if (!dis) return FALSE;
        if (dis->hwndItem == g_hComboList) { DrawComboListItem(dis); return TRUE; }
        if (dis->hwndItem == g_hActionList) { DrawActionItem(dis);    return TRUE; }

        // Dropdown whitelist mode — rendu premium identique aux autres contrôles
        if (dis->hwndItem == g_hWlModeCB && dis->itemID != (UINT)-1)
        {
            RECT r = dis->rcItem;
            HDC  dc = dis->hDC;
            bool sel = (dis->itemState & ODS_SELECTED) != 0;

            // Fond
            COLORREF bgC = sel ? Pal::ActSel : UiTheme::Color_ControlBg();
            HBRUSH br = CreateSolidBrush(bgC);
            FillRect(dc, &r, br); DeleteObject(br);

            // Texte
            wchar_t txt[128] = {};
            SendMessageW(g_hWlModeCB, CB_GETLBTEXT, dis->itemID, (LPARAM)txt);
            HFONT oldF = (HFONT)SelectObject(dc, GetFont(hWnd));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, sel ? RGB(220, 238, 255) : UiTheme::Color_Text());
            RECT tr = { r.left + 6, r.top, r.right - 4, r.bottom };
            DrawTextW(dc, txt, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(dc, oldF);
            return TRUE;
        }
        if (dis->CtlType == ODT_BUTTON) { DrawPremiumButton(dis); return TRUE; }
        return FALSE;
    }

    case WM_CONTEXTMENU: {
        HWND src = (HWND)wParam;
        if (src != g_hComboList || !g_hComboList)
            break;

        int idx = -1;
        int sx = (int)(short)LOWORD(lParam);
        int sy = (int)(short)HIWORD(lParam);

        if (sx == -1 && sy == -1) {
            idx = LB_SEL(g_hComboList);
        }
        else {
            POINT pt{ sx, sy };
            ScreenToClient(g_hComboList, &pt);
            LRESULT hit = SendMessageW(g_hComboList, LB_ITEMFROMPOINT, 0, MAKELPARAM(pt.x, pt.y));
            if (HIWORD(hit) == 0)
                idx = LOWORD(hit);
        }

        if (idx >= 0 && idx < (int)g_comboIds.size()) {
            int id = g_comboIds[(size_t)idx];
            if (FreeCombo* c = FreeComboSystem::GetCombo(id)) {
                c->enabled = !c->enabled;
                g_selectedId = id;
                if (c->enabled)
                    DisableOtherCombosWithSameTrigger(id);
                FreeComboUI::RefreshComboList();
                PersistToDisk();
                InvalidateRect(g_hComboList, nullptr, FALSE);
            }
        }
        return 0;
    }

                       // ── Commands ─────────────────────────────────────────────────
    case WM_COMMAND: {
        WORD ctlId = LOWORD(wParam), notif = HIWORD(wParam);

        if (ctlId == FreeComboUI::ID_CHK_REPEAT && notif == BN_CLICKED) {
            if (g_selectedId >= 0)
                if (FreeCombo* c = FreeComboSystem::GetCombo(g_selectedId))
                {
                    c->repeatWhileHeld = CHK_GET(g_hChkRepeat); PersistToDisk();
                }
            return 0;
        }
        if (ctlId == FreeComboUI::ID_CHK_ENABLED && notif == BN_CLICKED) {
            if (g_selectedId >= 0)
                if (FreeCombo* c = FreeComboSystem::GetCombo(g_selectedId)) {
                    c->enabled = CHK_GET(g_hChkEnabled);
                    if (c->enabled) DisableOtherCombosWithSameTrigger(g_selectedId);
                    FreeComboUI::RefreshComboList(); PersistToDisk();
                }
            return 0;
        }
        if (ctlId == FreeComboUI::ID_EDIT_DELAY && notif == EN_CHANGE) {
            if (g_delaySync)return 0; g_delaySync = true;
            SetDelayUi(GetWindowTextInt(g_hEditDelay), false);
            if (g_selectedId >= 0) if (FreeCombo* c = FreeComboSystem::GetCombo(g_selectedId))
                c->repeatDelayMs = (uint32_t)ClampDelay(GetWindowTextInt(g_hEditDelay));
            g_delaySync = false; return 0;
        }
        if (ctlId == FreeComboUI::ID_EDIT_DELAY && notif == EN_KILLFOCUS) {
            if (g_delaySync)return 0; g_delaySync = true;
            SetDelayUi(GetWindowTextInt(g_hEditDelay), true);
            if (g_selectedId >= 0) if (FreeCombo* c = FreeComboSystem::GetCombo(g_selectedId))
            {
                c->repeatDelayMs = (uint32_t)ClampDelay(GetWindowTextInt(g_hEditDelay)); PersistToDisk();
            }
            g_delaySync = false; return 0;
        }
        if (ctlId == FreeComboUI::ID_EDIT_NAME && notif == EN_KILLFOCUS) {
            if (g_selectedId >= 0) if (FreeCombo* c = FreeComboSystem::GetCombo(g_selectedId)) {
                wchar_t buf[256]{}; GetWindowTextW(g_hEditName, buf, 256);
                c->name = buf; FreeComboUI::RefreshComboList(); PersistToDisk();
            }
            return 0;
        }
        // Show/hide the mouse capture button depending on selected action type
        if (ctlId == FreeComboUI::ID_ACTION_TYPE_CB && notif == CBN_SELCHANGE) {
            int ti = CB_GETSEL(g_hActionTypeCB);
            bool isMouseClick = (ti == 4); // index 4 = Mouse click (0=Press,1=Release,2=Tap,3=Text,4=Mouse,5=Wait)
            if (g_hBtnCaptureMouse) {
                ShowWindow(g_hBtnCaptureMouse, isMouseClick ? SW_SHOW : SW_HIDE);
                // Enable/disable to match the new type — without this the button stays greyed
                if (isMouseClick) EnableWindow(g_hBtnCaptureMouse, TRUE);
                // Force repaint of the value row area so the button appears immediately
                InvalidateRect(g_hPage, nullptr, FALSE);
                if (isMouseClick) UpdateWindow(g_hBtnCaptureMouse);
                // Also cancel any in-progress mouse capture if user switched type
                if (!isMouseClick && g_capturingMouseAction) {
                    KillTimer(hWnd, FreeComboUI::ID_TIMER_MOUSE_CAPTURE);
                    g_capturingMouseAction = false;
                    SetWindowTextW(g_hBtnCaptureMouse, L"\U0001F5B1");
                    EnableWindow(g_hBtnCaptureMouse, TRUE);
                    EnableWindow(g_hBtnAdd, TRUE);
                }
            }
            // Clear the value field hint when switching types
            if (g_hActionKeyEdt) {
                if (isMouseClick) {
                    SetWindowTextW(g_hActionKeyEdt, L"left");
                    // Reset grace so any ongoing capture doesn't catch the combobox click
                    g_captureGraceUntil = GetTickCount() + 300;
                }
                else if (ti == 5)   SetWindowTextW(g_hActionKeyEdt, L"100");   // Wait ms
                else if (ti == 3)   SetWindowTextW(g_hActionKeyEdt, L"");      // Type text
                else                SetWindowTextW(g_hActionKeyEdt, L"P");     // Key
            }
            return 0;
        }
        FreeComboUI::OnCommand(hWnd, ctlId, notif);
        return 0;
    }

                   // ── Delay slider ──────────────────────────────────────────────
    case WM_HSCROLL:
        if ((HWND)lParam == g_hDelaySlider) {
            if (g_delaySync)return 0;
            int pos = (int)SendMessageW(g_hDelaySlider, TBM_GETPOS, 0, 0);
            g_delaySync = true; SetDelayUi(pos, true);
            if (g_selectedId >= 0) if (FreeCombo* c = FreeComboSystem::GetCombo(g_selectedId))
                c->repeatDelayMs = (uint32_t)ClampDelay(pos);
            g_delaySync = false;
        }
        return 0;

        // ── Capture timer ─────────────────────────────────────────────
    case WM_TIMER:
        if (wParam == FreeComboUI::ID_TIMER_CAPTURE) { FreeComboUI::OnTimer(); return 0; }
        if (wParam == FreeComboUI::ID_TIMER_MOUSE_CAPTURE) {
            struct { int vk; int btn; const wchar_t* name; } btns[] = {
                { VK_LBUTTON,  0, L"left"        },
                { VK_RBUTTON,  1, L"right"       },
                { VK_MBUTTON,  2, L"middle"      },
                { VK_XBUTTON1, 3, L"X1" },  // XBUTTON1 = bit3 = X1
                { VK_XBUTTON2, 4, L"X2" },  // XBUTTON2 = bit4 = X2
            };

            // Phase 1: grace period — extend as long as any button is still held.
            // This handles both the combobox selection click and the capture button click.
            if (GetTickCount() < g_captureGraceUntil) {
                bool anyHeld = false;
                for (auto& b : btns) if (GetAsyncKeyState(b.vk) & 0x8000) { anyHeld = true; break; }
                if (anyHeld) g_captureGraceUntil = GetTickCount() + 100; // keep extending while held
                return 0;
            }

            // Phase 2: all buttons released — detect first press
            for (auto& b : btns) {
                if (GetAsyncKeyState(b.vk) & 0x8000) {
                    KillTimer(hWnd, FreeComboUI::ID_TIMER_MOUSE_CAPTURE);
                    g_capturingMouseAction = false;
                    SetWindowTextW(g_hActionKeyEdt, b.name);
                    SetWindowTextW(g_hBtnCaptureMouse, L"\U0001F5B1");
                    EnableWindow(g_hBtnCaptureMouse, TRUE);
                    EnableWindow(g_hBtnAdd, TRUE);
                    SetFocus(g_hActionKeyEdt);
                    SendMessageW(g_hActionKeyEdt, EM_SETSEL, 0, -1);
                    break;
                }
            }
            return 0;
        }
        return 0;

        // ── Resize ─────────────────────────────────────────
    case WM_GETMINMAXINFO: {
        // Prevents shrinking below scrollable content + left panel -- Empêche de rétrécir en dessous du contenu scrollable + panel gauche
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        // Hauteur min = contenu colonne droite (hint inclus) + padding haut
        int minH = FC_GetContentHeight(hWnd) + Sc(hWnd, 26); // 26 = ly (tab header)
        if (minH < 300) minH = 300;
        mmi->ptMinTrackSize.y = minH;
        // Largeur min fixe (colonne gauche 220 + colonne droite min ~300 + marges)
        mmi->ptMinTrackSize.x = Sc(hWnd, 560);
        return 0;
    }

    case WM_SIZE: {
        int W = LOWORD(lParam), H = HIWORD(lParam);
        if (W < 50 || H < 50) return 0;

        // ── Shared constants ──────────────────────────────────────────
        const int pad = Sc(hWnd, 10);
        const int lx = Sc(hWnd, 10);
        // Must match the values used in PaintPage() for the "My combos" header,
        // otherwise the list panel climbs into the PNG/title area and visually clips it.
        const int myCombosLabelTop = Sc(hWnd, 3);
        const int myCombosLabelH = Sc(hWnd, 38);
        const int myCombosGap = Sc(hWnd, 6);
        const int ly = myCombosLabelTop + myCombosLabelH + myCombosGap;
        const int lw = Sc(hWnd, 220);
        const int btnH = Sc(hWnd, 26);

        // ── Left column ──────────────────────────────────────────────
        // Fixed height reserved for the INJECTION GUARD panel -- Hauteur fixe réservée pour le panel INJECTION GUARD
        // label(14) + gap(4) + dropdown(26) + gap(4) + liste(60) + gap(4) + edit(22) + bas(8)
        const int wlPanelH = Sc(hWnd, 36 + 4 + 26 + 4 + 60 + 4 + 22 + 8);  // 36 = hauteur label WATCHMAN
        int listH = H - ly - Sc(hWnd, 6) - btnH - Sc(hWnd, 6) - wlPanelH;
        if (listH < 60) listH = 60;

        if (g_hComboList) SetWindowPos(g_hComboList, nullptr, lx, ly, lw, listH, SWP_NOZORDER);
        int bY = ly + listH + Sc(hWnd, 6);
        int bw2 = (lw - Sc(hWnd, 6)) / 2;
        if (g_hBtnNew)    SetWindowPos(g_hBtnNew, nullptr, lx, bY, bw2, btnH, SWP_NOZORDER);
        if (g_hBtnDelete) SetWindowPos(g_hBtnDelete, nullptr, lx + bw2 + Sc(hWnd, 6), bY, bw2, btnH, SWP_NOZORDER);

        // ── Right column — Y recalculated from zero (FIX: no GetWindowRect on hidden window) ──
        const int rowH = Sc(hWnd, 24);
        const int rx = Sc(hWnd, 220 + 10 * 2 + 2);
        int rw = W - rx - Sc(hWnd, 10);
        if (rw < 120) rw = 120;

        // Reset scroll if content fits
        if (FC_GetMaxScroll(hWnd) <= 0 && g_scrollY != 0) {
            FC_OffsetChildren(hWnd, g_scrollY); // move children back to top
            g_scrollY = 0;
        }

        // Scrollbar width reservation
        const int sbW = Sc(hWnd, SCROLLBAR_W) + Sc(hWnd, SCROLLBAR_M) * 2;
        rw = W - rx - Sc(hWnd, 12) - sbW; // Reduce rw by 12px for scrollbar
        if (rw < 120) rw = 120;

        int ry = pad - g_scrollY;  // offset by scroll position

        // Combo name
        ry += Sc(hWnd, 16);
        if (g_hEditName)     SetWindowPos(g_hEditName, nullptr, rx, ry, rw, rowH, SWP_NOZORDER);
        ry += rowH + Sc(hWnd, 12);

        // Trigger card
        ry += Sc(hWnd, 18);
        if (g_hLblTrigger)   SetWindowPos(g_hLblTrigger, nullptr, rx, ry, rw, rowH, SWP_NOZORDER);
        ry += rowH + Sc(hWnd, 6);
        if (g_hBtnCapture)   SetWindowPos(g_hBtnCapture, nullptr, rx, ry, Sc(hWnd, 204), btnH, SWP_NOZORDER);
        ry += btnH + Sc(hWnd, 14);

        // Options card
        ry += Sc(hWnd, 18);
        if (g_hChkEnabled)   SetWindowPos(g_hChkEnabled, nullptr, rx, ry, Sc(hWnd, 148), Sc(hWnd, 20), SWP_NOZORDER);
        if (g_hChkRepeat)    SetWindowPos(g_hChkRepeat, nullptr, rx + Sc(hWnd, 156), ry, Sc(hWnd, 196), Sc(hWnd, 20), SWP_NOZORDER);
        ry += Sc(hWnd, 26);

        // Run N times + Cancel on release (FIX: was not positioned, could end up behind other controls)
        if (g_hEditRepeatCount) SetWindowPos(g_hEditRepeatCount, nullptr, rx + Sc(hWnd, 156), ry, Sc(hWnd, 44), rowH, SWP_NOZORDER);
        if (g_hChkCancelRel)    SetWindowPos(g_hChkCancelRel,    nullptr, rx + Sc(hWnd, 210), ry, Sc(hWnd, 180), Sc(hWnd, 20), SWP_NOZORDER);
        ry += rowH + Sc(hWnd, 4);
        // Long press row
        if (g_hChkLongPress)    SetWindowPos(g_hChkLongPress,    nullptr, rx,                 ry, Sc(hWnd, 140), Sc(hWnd, 20), SWP_NOZORDER);
        if (g_hEditLongPressMs) SetWindowPos(g_hEditLongPressMs, nullptr, rx + Sc(hWnd, 144), ry, Sc(hWnd, 52),  rowH,         SWP_NOZORDER);
        if (g_hEditLongPressMs) EnableWindow(g_hEditLongPressMs,
            g_hChkLongPress && SendMessageW(g_hChkLongPress, BM_GETCHECK, 0, 0) == BST_CHECKED);
        ry += rowH + Sc(hWnd, 4);

        // Repeat delay edit
        if (g_hEditDelay)    SetWindowPos(g_hEditDelay, nullptr, rx + Sc(hWnd, 146), ry, Sc(hWnd, 52), rowH, SWP_NOZORDER);
        ry += rowH + Sc(hWnd, 4);

        // Delay slider + value label
        if (g_hDelaySlider)  SetWindowPos(g_hDelaySlider, nullptr, rx, ry, rw - Sc(hWnd, 62), Sc(hWnd, 22), SWP_NOZORDER);
        if (g_hDelayValue)   SetWindowPos(g_hDelayValue, nullptr, rx + rw - Sc(hWnd, 58), ry, Sc(hWnd, 58), Sc(hWnd, 22), SWP_NOZORDER);
        ry += Sc(hWnd, 26) + Sc(hWnd, 12);
        // Card Actions
        ry += Sc(hWnd, 18);
        if (g_hActionList)   SetWindowPos(g_hActionList, nullptr, rx, ry, rw, Sc(hWnd, 100), SWP_NOZORDER);
        ry += Sc(hWnd, 106);
        if (g_hActionTypeCB) SetWindowPos(g_hActionTypeCB, nullptr, rx + Sc(hWnd, 48), ry, rw - Sc(hWnd, 48), Sc(hWnd, 200), SWP_NOZORDER);
        ry += rowH + Sc(hWnd, 4);

        // Value field + mouse capture button (30px wide on the right)
        ry += Sc(hWnd, 16);
        {
            int capW = Sc(hWnd, 30);
            if (g_hActionKeyEdt)    SetWindowPos(g_hActionKeyEdt, nullptr, rx, ry, rw - capW - Sc(hWnd, 4), rowH, SWP_NOZORDER);
            if (g_hBtnCaptureMouse) SetWindowPos(g_hBtnCaptureMouse, nullptr, rx + rw - capW, ry, capW, rowH, SWP_NOZORDER);
        }
        ry += rowH + Sc(hWnd, 6);

        // 3-column buttons: Add / Delay / Delete
        {
            int w3 = (rw - Sc(hWnd, 8)) / 3;
            int g3 = Sc(hWnd, 4);
            if (g_hBtnAdd)      SetWindowPos(g_hBtnAdd, nullptr, rx, ry, w3, btnH, SWP_NOZORDER);
            if (g_hBtnAddDelay) SetWindowPos(g_hBtnAddDelay, nullptr, rx + w3 + g3, ry, w3, btnH, SWP_NOZORDER);
            if (g_hBtnDelAct)   SetWindowPos(g_hBtnDelAct, nullptr, rx + (w3 + g3) * 2, ry, w3, btnH, SWP_NOZORDER);
        }
        ry += btnH + Sc(hWnd, 4);

        // 2-column buttons: Up / Down
        {
            int w2 = (rw - Sc(hWnd, 4)) / 2;
            if (g_hBtnUp)   SetWindowPos(g_hBtnUp, nullptr, rx, ry, w2, btnH, SWP_NOZORDER);
            if (g_hBtnDown) SetWindowPos(g_hBtnDown, nullptr, rx + w2 + Sc(hWnd, 4), ry, w2, btnH, SWP_NOZORDER);
        }
        ry += btnH + Sc(hWnd, 8);

        // Save button (full width)
        if (g_hBtnSave) SetWindowPos(g_hBtnSave, nullptr, rx, ry, rw, btnH + Sc(hWnd, 2), SWP_NOZORDER);

        // ── Whitelist panel — anchored just below New/Delete / ancré juste sous New/Delete ───────────────────────
        {
            // wy = below the New/Delete buttons + space for the label "INJECTION GUARD" -- wy = sous les boutons New/Delete + espace pour le label "INJECTION GUARD"
            // WATCHMAN_LABEL_H = 36 — changer cette valeur pour ajuster hauteur label + espacement icône
            int wy  = bY + btnH + Sc(hWnd, 4);
            int ebw = lw - Sc(hWnd, 56);
            wy += Sc(hWnd, 36);  // hauteur label WATCHMAN (inclut la lanterne 30px)
            if (g_hWlModeCB) SetWindowPos(g_hWlModeCB, nullptr, lx, wy, lw, Sc(hWnd, 26), SWP_NOZORDER);
            wy += Sc(hWnd, 26) + Sc(hWnd, 4);
            if (g_hWlList)   SetWindowPos(g_hWlList, nullptr, lx, wy, lw, Sc(hWnd, 60), SWP_NOZORDER);
            wy += Sc(hWnd, 60) + Sc(hWnd, 4);
            if (g_hWlEdit)   SetWindowPos(g_hWlEdit, nullptr, lx, wy, ebw, Sc(hWnd, 22), SWP_NOZORDER);
            if (g_hWlBtnAdd) SetWindowPos(g_hWlBtnAdd, nullptr, lx + ebw + Sc(hWnd, 2),  wy, Sc(hWnd, 24), Sc(hWnd, 22), SWP_NOZORDER);
            if (g_hWlBtnDel) SetWindowPos(g_hWlBtnDel, nullptr, lx + ebw + Sc(hWnd, 28), wy, Sc(hWnd, 24), Sc(hWnd, 22), SWP_NOZORDER);
        }

        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        UINT lines = 3;
        SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
        if (lines == 0 || lines == WHEEL_PAGESCROLL) lines = 3;
        int step = std::max(Sc(hWnd, 24), (int)lines * Sc(hWnd, 18));
        FC_SetScrollY(hWnd, g_scrollY - (delta / WHEEL_DELTA) * step);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        if (FC_GetMaxScroll(hWnd) > 0) {
            RECT thumb = FC_GetScrollThumb(hWnd);
            RECT track = FC_GetScrollTrack(hWnd);
            if (PtInRect(&thumb, pt)) {
                g_scrollDrag = true;
                g_scrollDragStartY = pt.y;
                g_scrollDragStartScrollY = g_scrollY;
                g_scrollDragThumbHeight = std::max(1, (int)(thumb.bottom - thumb.top));
                g_scrollDragMax = FC_GetMaxScroll(hWnd);
                SetCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            else if (PtInRect(&track, pt)) {
                RECT clientRc{}; GetClientRect(hWnd, &clientRc);
                int pageH = clientRc.bottom - clientRc.top;
                if (pt.y < thumb.top)
                    FC_SetScrollY(hWnd, g_scrollY - pageH);
                else
                    FC_SetScrollY(hWnd, g_scrollY + pageH);
                return 0;
            }
        }
        break;
    }



    case WM_MOUSEMOVE: {
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        if (g_scrollDrag) {
            RECT track = FC_GetScrollTrack(hWnd);
            int trackH = std::max(1, (int)(track.bottom - track.top));
            int maxScroll = std::max(1, g_scrollDragMax);
            int thumbH = std::max(1, g_scrollDragThumbHeight);
            int travel = std::max(1, trackH - thumbH);
            int dy = pt.y - g_scrollDragStartY;
            double t = (double)dy / (double)travel;
            int target = g_scrollDragStartScrollY + (int)(t * maxScroll);
            FC_SetScrollY(hWnd, target);
            return 0;
        }
        if (g_actionDrag && g_hActionList) {
            POINT lpt = pt;
            MapWindowPoints(hWnd, g_hActionList, &lpt, 1);
            int itemH = (int)SendMessageW(g_hActionList, LB_GETITEMHEIGHT, 0, 0);
            if (itemH <= 0) itemH = 24;
            int idx2 = (int)SendMessageW(g_hActionList, LB_GETTOPINDEX, 0, 0) + lpt.y / itemH;
            int cnt2 = (int)SendMessageW(g_hActionList, LB_GETCOUNT, 0, 0);
            idx2 = std::max(0, std::min(idx2, cnt2 - 1));
            if (idx2 != g_actionDragHover) {
                g_actionDragHover = idx2;
                InvalidateRect(g_hActionList, nullptr, FALSE);
            }
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return 0;
        }
        if (g_comboDrag && g_hComboList) {
            int cnt2 = (int)SendMessageW(g_hComboList, LB_GETCOUNT, 0, 0);
            if (cnt2 <= 0) { SetCursor(LoadCursorW(nullptr, IDC_HAND)); return 0; }

            int itemH = (int)SendMessageW(g_hComboList, LB_GETITEMHEIGHT, 0, 0);
            if (itemH <= 0) itemH = 42;

            // ── Calculer idx2 depuis le déplacement Y depuis le point de départ ──
            // Approche robuste : on part de g_comboDragSrcIdx et on ajoute
            // le nombre d'items parcourus depuis le Y initial du drag.
            // Cela fonctionne indépendamment du scroll de la ListBox.
            int deltaY   = pt.y - g_comboDragStartY;
            int deltaIdx = deltaY / itemH;   // peut être négatif (drag vers le haut)
            int idx2     = std::max(0, std::min(g_comboDragSrcIdx + deltaIdx, cnt2 - 1));

            // ── Auto-scroll de la ListBox pour suivre le curseur ──────
            // Forcer la ListBox à montrer l'item cible
            SendMessageW(g_hComboList, LB_SETTOPINDEX, (WPARAM)std::max(0, idx2 - 1), 0);

            if (idx2 != g_comboDragHover) {
                g_comboDragHover = idx2;
                InvalidateRect(g_hComboList, nullptr, FALSE);
            }
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return 0;
        }
        break;
    }

    case WM_LBUTTONUP: {
        if (g_scrollDrag) {
            g_scrollDrag = false;
            ReleaseCapture();
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (g_actionDrag) {
            int src = g_actionDragSrcIdx;
            int dst = g_actionDragHover;
            g_actionDrag = false;
            g_actionDragSrcIdx = -1;
            g_actionDragHover  = -1;
            ReleaseCapture();
            if (src >= 0 && dst >= 0 && src != dst && g_selectedId >= 0) {
                // Move action from src to dst by successive swaps
                FreeCombo* c = FreeComboSystem::GetCombo(g_selectedId);
                if (c && src < (int)c->actions.size() && dst < (int)c->actions.size()) {
                    int step = (src < dst) ? 1 : -1;
                    for (int i = src; i != dst; i += step) {
                        if (step > 0) FreeComboSystem::MoveActionDown(g_selectedId, i);
                        else          FreeComboSystem::MoveActionUp(g_selectedId, i);
                    }
                    RefreshActionList();
                    LB_SETCUR(g_hActionList, dst);
                    PersistToDisk();
                }
            }
            InvalidateRect(g_hActionList, nullptr, FALSE);
            return 0;
        }
        if (g_comboDrag) {
            int src = g_comboDragSrcIdx;
            int dst = g_comboDragHover;
            g_comboDrag = false;
            g_comboDragSrcIdx = -1;
            g_comboDragHover  = -1;
            ReleaseCapture();
            if (src >= 0 && dst >= 0 && src != dst
                && src < (int)g_comboIds.size() && dst < (int)g_comboIds.size()) {
                // Déplacement direct : src → dst en une seule opération
                // g_comboIds[i] == i car les IDs sont des index séquentiels
                // MoveCombo fait un std::rotate dans g_combos
                FreeComboSystem::MoveCombo(src, dst);
                // Reconstruire g_comboIds depuis le nouvel ordre
                int selId = -1;
                FreeComboUI::RefreshComboList();
                // Sélectionner le combo qui était à src (maintenant à dst)
                if (dst < (int)g_comboIds.size())
                    selId = g_comboIds[(size_t)dst];
                if (selId >= 0) FreeComboUI::SelectCombo(selId);
                PersistToDisk();
            }
            InvalidateRect(g_hComboList, nullptr, FALSE);
            return 0;
        }
        break;
    }

    case WM_CAPTURECHANGED: {
        if (g_scrollDrag) {
            g_scrollDrag = false;
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        if (g_actionDrag) {
            g_actionDrag = false; g_actionDragSrcIdx = -1; g_actionDragHover = -1;
            InvalidateRect(g_hActionList, nullptr, FALSE);
        }
        if (g_comboDrag) {
            g_comboDrag = false; g_comboDragSrcIdx = -1; g_comboDragHover = -1;
            InvalidateRect(g_hComboList, nullptr, FALSE);
        }
        return 0;
    }

    case WM_SETCURSOR: {
        if (g_actionDrag || g_comboDrag) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        if ((HWND)wParam == g_hActionList || (HWND)wParam == g_hComboList) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        if ((HWND)wParam == hWnd && FC_GetMaxScroll(hWnd) > 0) {
            POINT pt{};
            GetCursorPos(&pt); ScreenToClient(hWnd, &pt);
            RECT thumb = FC_GetScrollThumb(hWnd);
            RECT track = FC_GetScrollTrack(hWnd);
            if (PtInRect(&thumb, pt) || PtInRect(&track, pt)) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
        }
        break;
    }

    } // switch
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ════════════════════════════════════════════════════════════════════
// Subclass proc — Action list  (drag & drop + clic [x])
// ════════════════════════════════════════════════════════════════════
static LRESULT CALLBACK ActionListSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_LBUTTONDOWN) {
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        int itemH = (int)SendMessageW(hWnd, LB_GETITEMHEIGHT, 0, 0);
        if (itemH <= 0) itemH = 24;
        int top = (int)SendMessageW(hWnd, LB_GETTOPINDEX, 0, 0);
        int idx = top + pt.y / itemH;
        int cnt = (int)SendMessageW(hWnd, LB_GETCOUNT, 0, 0);
        if (idx >= 0 && idx < cnt && g_selectedId >= 0) {
            RECT itemRc{}; SendMessageW(hWnd, LB_GETITEMRECT, idx, (LPARAM)&itemRc);
            if (pt.x >= itemRc.right - kActionDelW) {
                FreeComboSystem::RemoveAction(g_selectedId, idx);
                RefreshActionList(); PersistToDisk();
                return 0;
            }
            g_actionDrag       = true;
            g_actionDragSrcIdx = idx;
            g_actionDragHover  = idx;
            // Visual selection via the listbox, then we resume the capture -- Sélection visuelle via la listbox, puis on reprend la capture
            CallWindowProcW(g_origActionListProc, hWnd, msg, wParam, lParam);
            SetCapture(g_hPageForSubclass);
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return 0;
        }
    }
    if (msg == WM_MOUSEMOVE && g_actionDrag) {
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        ClientToScreen(hWnd, &pt);
        ScreenToClient(g_hPageForSubclass, &pt);
        SendMessageW(g_hPageForSubclass, WM_MOUSEMOVE, wParam, MAKELPARAM((short)pt.x, (short)pt.y));
        return 0;
    }
    if (msg == WM_LBUTTONUP && g_actionDrag) {
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        ClientToScreen(hWnd, &pt);
        ScreenToClient(g_hPageForSubclass, &pt);
        SendMessageW(g_hPageForSubclass, WM_LBUTTONUP, wParam, MAKELPARAM((short)pt.x, (short)pt.y));
        return 0;
    }
    return CallWindowProcW(g_origActionListProc, hWnd, msg, wParam, lParam);
}

// ════════════════════════════════════════════════════════════════════
// Subclass proc — Combo list  (drag & drop)
// ════════════════════════════════════════════════════════════════════
static LRESULT CALLBACK ComboListSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_LBUTTONDOWN) {
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        int itemH = (int)SendMessageW(hWnd, LB_GETITEMHEIGHT, 0, 0);
        if (itemH <= 0) itemH = 42;
        int top = (int)SendMessageW(hWnd, LB_GETTOPINDEX, 0, 0);
        int idx = top + pt.y / itemH;
        int cnt = (int)SendMessageW(hWnd, LB_GETCOUNT, 0, 0);
        if (idx >= 0 && idx < cnt) {
            // Select the item explicitly BEFORE capturing the mouse -- Sélectionner l'item explicitement AVANT de capturer la souris
            SendMessageW(hWnd, LB_SETCURSEL, (WPARAM)idx, 0);
            // Notify the parent (LBN_SELCHANGE) to trigger LoadComboToUI() -- Notifier le parent (LBN_SELCHANGE) pour que LoadComboToUI() se déclenche
            SendMessageW(GetParent(hWnd), WM_COMMAND,
                MAKEWPARAM(GetDlgCtrlID(hWnd), LBN_SELCHANGE), (LPARAM)hWnd);
            g_comboDrag       = true;
            g_comboDragSrcIdx = idx;
            g_comboDragHover  = idx;
            // Enregistrer la position Y dans les coords de la page
            {
                POINT startPt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
                ClientToScreen(hWnd, &startPt);
                ScreenToClient(g_hPageForSubclass, &startPt);
                g_comboDragStartY = startPt.y;
            }
            SetCapture(g_hPageForSubclass);
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return 0;
        }
    }
    if (msg == WM_MOUSEMOVE && g_comboDrag) {
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        ClientToScreen(hWnd, &pt);
        ScreenToClient(g_hPageForSubclass, &pt);
        SendMessageW(g_hPageForSubclass, WM_MOUSEMOVE, wParam, MAKELPARAM((short)pt.x, (short)pt.y));
        return 0;
    }
    if (msg == WM_LBUTTONUP && g_comboDrag) {
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        ClientToScreen(hWnd, &pt);
        ScreenToClient(g_hPageForSubclass, &pt);
        SendMessageW(g_hPageForSubclass, WM_LBUTTONUP, wParam, MAKELPARAM((short)pt.x, (short)pt.y));
        return 0;
    }
    return CallWindowProcW(g_origComboListProc, hWnd, msg, wParam, lParam);
}
