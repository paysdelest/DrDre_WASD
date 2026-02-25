// free_combo_ui.cpp  ─  DrDre_WASD v2.0  ─  Premium UI rewrite
// ====================================================================
// Visual improvements :
//   • Left owner-draw list: state dot (green/gray), keyboard/mouse icon,
//     subtle alternating backgrounds, selected item in dark blue
//   • Owner-draw action list: color dot by type, numbering,
//     readable text with key name
//   • Cards drawn in WM_PAINT (distinct background + small-caps label)
//     for Trigger, Options, Actions
//   • Boutons BS_OWNERDRAW premium : Save=bleu, CAPTURE=vert/orange,
//     Supprimer=rouge, autres=gris neutre, hover + pressed
//   • Vertical separator between both columns
//   • Double-buffer sur WM_PAINT (aucun clignotement)
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
    // Pastilles types d'actions
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
// Left column
static HWND g_hComboList = nullptr;
static HWND g_hBtnNew = nullptr;
static HWND g_hBtnDelete = nullptr;
// Colonne droite – nom
static HWND g_hEditName = nullptr;
// Trigger card
static HWND g_hLblTrigger = nullptr;
static HWND g_hBtnCapture = nullptr;
// Card Options
static HWND g_hChkEnabled = nullptr;
static HWND g_hChkRepeat = nullptr;
static HWND g_hEditDelay = nullptr;
static HWND g_hDelaySlider = nullptr;
static HWND g_hDelayValue = nullptr;
// Card Actions
static HWND g_hActionList = nullptr;
static HWND g_hActionTypeCB = nullptr;
static HWND g_hActionKeyEdt = nullptr;
static HWND g_hBtnAdd = nullptr;
static HWND g_hBtnAddDelay = nullptr;
static HWND g_hBtnDelAct = nullptr;
static HWND g_hBtnUp = nullptr;
static HWND g_hBtnDown = nullptr;
static HWND g_hBtnSave = nullptr;

static bool g_delaySync = false;
static bool g_capturing = false;
static std::vector<int> g_comboIds;
static int  g_selectedId = -1;

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
static HFONT GetBold(HWND h) { static HFONT f = nullptr; if (!f) f = MakeFont(h, 9, FW_SEMIBOLD); return f; }
static HFONT GetSmall(HWND h) { static HFONT f = nullptr; if (!f) f = MakeFont(h, 8, FW_NORMAL);   return f; }

static void ApplyFont(HWND h, bool bold = false)
{
    HFONT f = bold ? GetBold(h) : GetFont(h);
    if (f) SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE);
}
static void ApplyFontChildren(HWND p)
{
    HFONT f = GetFont(p);
    if (!f) return;
    for (HWND c = GetWindow(p, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
        SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE);
}

static int Sc(HWND h, int px) { return WinUtil_ScalePx(h, px); }

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
        const wchar_t* b[] = { L"left",L"right",L"middle" };
        return std::wstring(L"Click      ") + b[a.mouseButton < 3 ? a.mouseButton : 0];
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

    // ── Fond ──────────────────────────────────────────────────────
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

    // ── Icone clavier / souris ─────────────────────────────────────
    int iconSz = 14, iconX = rc.left + 26, iconY = cy - iconSz / 2;
    COLORREF iconC = sel ? RGB(190, 215, 255) : UiTheme::Color_TextMuted();
    if (combo->trigger.IsValid())
        (TriggerIsMouse(combo->trigger) ? DrawMouseIcon : DrawKbdIcon)(hdc, iconX, iconY, iconSz, iconC);

    // ── Nom ────────────────────────────────────────────────────────
    HFONT oldF = (HFONT)SelectObject(hdc, combo->enabled ? GetFont(dis->hwndItem) : GetFont(dis->hwndItem));
    SetBkMode(hdc, TRANSPARENT);
    // Name in white if active+selected, dimmed if inactive
    SetTextColor(hdc,
        !combo->enabled ? RGB(100, 100, 115) :
        sel ? RGB(235, 245, 255) :
        UiTheme::Color_Text());
    RECT tr{ iconX + iconSz + 7, rc.top, rc.right - 6, rc.bottom };
    DrawTextW(hdc, combo->name.c_str(), -1, &tr,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    // Badge "(inactive)" in small font on the right if disabled and not selected
    if (!combo->enabled && !sel) {
        SelectObject(hdc, GetSmall(dis->hwndItem));
        SetTextColor(hdc, Pal::DotInactive);
        RECT br2{ rc.right - 62,rc.top,rc.right - 4,rc.bottom };
        DrawTextW(hdc, L"(inactive)", -1, &br2, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    SelectObject(hdc, oldF);
    if (dis->itemState & ODS_FOCUS) DrawFocusRect(hdc, &rc);
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

    // Texte de l'action
    SelectObject(hdc, GetFont(dis->hwndItem));
    SetTextColor(hdc, sel ? RGB(220, 238, 255) : UiTheme::Color_Text());
    RECT tr{ rc.left + 42,rc.top,rc.right - 6,rc.bottom };
    std::wstring lbl = ActionLabel(act);
    DrawTextW(hdc, lbl.c_str(), -1, &tr,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    SelectObject(hdc, oldF);
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
    }

    // ── Label Left column ─────────────────────────────────────
    {
        HFONT oldF = (HFONT)SelectObject(hdc, GetBold(hWnd));
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, UiTheme::Color_Text());
        RECT lr{ Sc(hWnd,8),Sc(hWnd,8),Sc(hWnd,230),Sc(hWnd,24) };
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

    // ── Label "Nom du combo" ──────────────────────────────────────
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

    // ── Card OPTIONS ─────────────────────────────────────────────
    {
        RECT card{};
        if (CardRect(hWnd, g_hChkEnabled, g_hDelayValue, Sc(hWnd, 10), Sc(hWnd, 8), card)) {
            FillRoundRect(hdc, card, Sc(hWnd, 7), Pal::CardBg, Pal::CardBorder);
            DrawCardLabel(hdc, hWnd, card, L"OPTIONS");
        }
    }

    // ── Card ACTIONS ─────────────────────────────────────────────
    {
        RECT card{};
        if (CardRect(hWnd, g_hActionList, g_hBtnSave, Sc(hWnd, 10), Sc(hWnd, 8), card)) {
            FillRoundRect(hdc, card, Sc(hWnd, 7), Pal::CardBg, Pal::CardBorder);
            DrawCardLabel(hdc, hWnd, card, L"ACTIONS");
        }
    }

    // ── Label "Type / Valeur" section actions ─────────────────────
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
    En(g_hEditDelay);   En(g_hDelaySlider); En(g_hDelayValue);
    En(g_hActionList);  En(g_hActionTypeCB); En(g_hActionKeyEdt);
    En(g_hBtnAdd);      En(g_hBtnAddDelay);  En(g_hBtnDelAct);
    En(g_hBtnUp);       En(g_hBtnDown);      En(g_hBtnSave);
    En(g_hBtnDelete);
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
// namespace FreeComboUI  –  public API
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

        case ID_BTN_ADD_ACTION: {
            if (g_selectedId < 0) break;
            int ti = CB_GETSEL(g_hActionTypeCB); if (ti < 0)break;
            ComboAction action{};
            action.type = (ComboActionType)(ti + 1);
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
                if (_wcsicmp(kbuf, L"right") == 0)   action.mouseButton = 1;
                else if (_wcsicmp(kbuf, L"middle") == 0) action.mouseButton = 2;
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

        // ── Layout constants ("base 96dpi" coordinates) ──
        //    Uses raw integer values; WM_SIZE recalculates everything.
        const int pad = 10;
        const int lw = 220;  // left column width
        const int lx = pad;
        const int ly = 26;   // list start (after the "My combos" label)
        const int listH = 252;  // hauteur initiale de la liste
        const int btnH = 26;
        const int rx = lw + pad * 2 + 2;   // right column start
        const int rw = 352;              // right column width
        const int rowH = 24;
        const int spad = 8;    // padding interne des sections

        // ── Left column ────────────────────────────────────────
        g_hComboList = CreateWindowExW(0, L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | WS_VSCROLL,
            lx, ly, lw, listH, g_hPage, (HMENU)ID_COMBO_LIST, hInst, nullptr);
        SendMessageW(g_hComboList, LB_SETITEMHEIGHT, 0, 28);

        int btnY = ly + listH + 6;
        int bw2 = (lw - 6) / 2;
        g_hBtnNew = CreateWindowExW(0, L"BUTTON", L"+ New",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            lx, btnY, bw2, btnH, g_hPage, (HMENU)ID_BTN_NEW, hInst, nullptr);
        g_hBtnDelete = CreateWindowExW(0, L"BUTTON", L"\u2212 Delete",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            lx + bw2 + 6, btnY, bw2, btnH, g_hPage, (HMENU)ID_BTN_DELETE, hInst, nullptr);

        // ── Colonne droite ────────────────────────────────────────
        int ry = pad;

        // Nom
        ry += 16; // space for the "COMBO NAME" label (drawn in WM_PAINT)
        g_hEditName = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            rx, ry, rw, rowH, g_hPage, (HMENU)ID_EDIT_NAME, hInst, nullptr);
        ry += rowH + 12;

        // ─ Trigger card ─
        ry += 18; // label de card (WM_PAINT)

        g_hLblTrigger = CreateWindowExW(0, L"STATIC", L"(not configured)",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            rx, ry, rw, rowH, g_hPage, (HMENU)ID_LBL_TRIGGER, hInst, nullptr);
        ry += rowH + 6;

        g_hBtnCapture = CreateWindowExW(0, L"BUTTON", L"\u25CF  Capture trigger",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rx, ry, 204, btnH, g_hPage, (HMENU)ID_BTN_CAPTURE, hInst, nullptr);
        ry += btnH + 14;

        // ─ Card Options ─
        ry += 18;

        g_hChkEnabled = CreateWindowExW(0, L"BUTTON", L"Combo enabled",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            rx, ry, 148, 20, g_hPage, (HMENU)ID_CHK_ENABLED, hInst, nullptr);
        CHK_SET(g_hChkEnabled, true);

        g_hChkRepeat = CreateWindowExW(0, L"BUTTON", L"Repeat while held",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            rx + 156, ry, 196, 20, g_hPage, (HMENU)ID_CHK_REPEAT, hInst, nullptr);
        ry += 26;

        // Delay row: static label | edit | slider | value
        HWND lblDel = CreateWindowExW(0, L"STATIC", L"Repeat delay:",
            WS_CHILD | WS_VISIBLE, rx, ry + 3, 142, 18, g_hPage, nullptr, hInst, nullptr);
        (void)lblDel;
        g_hEditDelay = CreateWindowExW(0, L"EDIT", L"400", WS_CHILD | WS_VISIBLE | ES_NUMBER,
            rx + 146, ry, 52, rowH, g_hPage, (HMENU)ID_EDIT_DELAY, hInst, nullptr);
        ry += rowH + 4;

        g_hDelaySlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            rx, ry, rw - 62, 22, g_hPage, (HMENU)ID_SLIDER_DELAY, hInst, nullptr);
        SendMessageW(g_hDelaySlider, TBM_SETRANGE, FALSE, MAKELONG(10, 2000));
        SendMessageW(g_hDelaySlider, TBM_SETPAGESIZE, 0, 50);

        g_hDelayValue = CreateWindowExW(0, L"STATIC", L"400 ms", WS_CHILD | WS_VISIBLE | SS_RIGHT,
            rx + rw - 58, ry, 58, 22, g_hPage, (HMENU)ID_LBL_DELAY_VALUE, hInst, nullptr);
        ry += 26 + 12;

        // ─ Card Actions ─
        ry += 18;

        g_hActionList = CreateWindowExW(0, L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | WS_VSCROLL,
            rx, ry, rw, 100, g_hPage, (HMENU)ID_ACTION_LIST, hInst, nullptr);
        SendMessageW(g_hActionList, LB_SETITEMHEIGHT, 0, 24);
        ry += 106;

        // Action type
        HWND lblType = CreateWindowExW(0, L"STATIC", L"Type:", WS_CHILD | WS_VISIBLE,
            rx, ry + 3, 44, 18, g_hPage, nullptr, hInst, nullptr);
        (void)lblType;
        g_hActionTypeCB = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            rx + 48, ry, rw - 48, 200, g_hPage, (HMENU)ID_ACTION_TYPE_CB, hInst, nullptr);
        for (int i = 0; i < 6; ++i) CB_ADD(g_hActionTypeCB, ACTION_NAMES[i]);
        CB_SETSEL(g_hActionTypeCB, 2);
        ry += rowH + 4;

        // Label "VALEUR" (WM_PAINT) + champ edit
        ry += 16;
        g_hActionKeyEdt = CreateWindowExW(0, L"EDIT", L"P", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            rx, ry, rw, rowH, g_hPage, (HMENU)ID_ACTION_KEY_EDIT, hInst, nullptr);
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
        ApplyTheme(g_hActionKeyEdt); ApplyTheme(g_hChkRepeat); ApplyTheme(g_hChkEnabled);
        ApplyTheme(g_hEditDelay); ApplyTheme(g_hDelaySlider); ApplyTheme(g_hDelayValue);

        ApplyFontChildren(g_hPage);
        ApplyFont(g_hBtnSave, true);
        ApplyFont(g_hBtnCapture, false);

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
            buf.End(hdc, rc.right, rc.bottom);
        }
        else { PaintPage(hWnd, hdc); }
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
    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lParam;
        if (!dis) return FALSE;
        if (dis->hwndItem == g_hComboList) { DrawComboListItem(dis); return TRUE; }
        if (dis->hwndItem == g_hActionList) { DrawActionItem(dis);    return TRUE; }
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

        // ── CAPTURE timer ─────────────────────────────────────────────
    case WM_TIMER:
        if (wParam == FreeComboUI::ID_TIMER_CAPTURE) { FreeComboUI::OnTimer(); return 0; }
        return 0;

        // ── Resize ─────────────────────────────────────────
    case WM_SIZE: {
        int W = LOWORD(lParam), H = HIWORD(lParam);
        if (W < 50 || H < 50) return 0;

        // Left column: list takes full available height
        const int pad = Sc(hWnd, 10);
        const int lx = Sc(hWnd, 10);
        const int ly = Sc(hWnd, 26);
        const int lw = Sc(hWnd, 220);
        const int btnH = Sc(hWnd, 26);

        int listH = H - ly - Sc(hWnd, 6) - btnH - Sc(hWnd, 6);
        if (listH < 60) listH = 60;

        if (g_hComboList) SetWindowPos(g_hComboList, nullptr, lx, ly, lw, listH, SWP_NOZORDER);
        int bY = ly + listH + Sc(hWnd, 6);
        int bw2 = (lw - Sc(hWnd, 6)) / 2;
        if (g_hBtnNew)    SetWindowPos(g_hBtnNew, nullptr, lx, bY, bw2, btnH, SWP_NOZORDER);
        if (g_hBtnDelete) SetWindowPos(g_hBtnDelete, nullptr, lx + bw2 + Sc(hWnd, 6), bY, bw2, btnH, SWP_NOZORDER);

        // Right column: stretch width
        const int rx = Sc(hWnd, 220 + 10 * 2 + 2);
        int rw = W - rx - Sc(hWnd, 10);
        if (rw < 120) rw = 120;

        auto ResizeW = [&](HWND h) {
            if (!h || !IsWindow(h))return;
            RECT r{}; GetWindowRect(h, &r); MapWindowPoints(nullptr, hWnd, (LPPOINT)&r, 2);
            SetWindowPos(h, nullptr, r.left, r.top, rw, r.bottom - r.top, SWP_NOZORDER);
            };
        ResizeW(g_hEditName); ResizeW(g_hLblTrigger);
        ResizeW(g_hActionList); ResizeW(g_hBtnSave);

        if (g_hActionTypeCB) {
            RECT r{}; GetWindowRect(g_hActionTypeCB, &r); MapWindowPoints(nullptr, hWnd, (LPPOINT)&r, 2);
            SetWindowPos(g_hActionTypeCB, nullptr, r.left, r.top, rw - (r.left - rx), r.bottom - r.top, SWP_NOZORDER);
        }
        if (g_hActionKeyEdt) ResizeW(g_hActionKeyEdt);

        if (g_hDelaySlider) {
            RECT r{}; GetWindowRect(g_hDelaySlider, &r); MapWindowPoints(nullptr, hWnd, (LPPOINT)&r, 2);
            SetWindowPos(g_hDelaySlider, nullptr, r.left, r.top, rw - Sc(hWnd, 62), r.bottom - r.top, SWP_NOZORDER);
        }
        if (g_hDelayValue) {
            RECT r{}; GetWindowRect(g_hDelayValue, &r); MapWindowPoints(nullptr, hWnd, (LPPOINT)&r, 2);
            SetWindowPos(g_hDelayValue, nullptr, rx + rw - Sc(hWnd, 58), r.top, Sc(hWnd, 58), r.bottom - r.top, SWP_NOZORDER);
        }

        // 3-column buttons
        auto ReBtn3 = [&](HWND h, int idx) {
            if (!h || !IsWindow(h))return;
            int w3 = (rw - Sc(hWnd, 8)) / 3;
            RECT r{}; GetWindowRect(h, &r); MapWindowPoints(nullptr, hWnd, (LPPOINT)&r, 2);
            SetWindowPos(h, nullptr, rx + idx * (w3 + Sc(hWnd, 4)), r.top, w3, r.bottom - r.top, SWP_NOZORDER);
            };
        ReBtn3(g_hBtnAdd, 0); ReBtn3(g_hBtnAddDelay, 1); ReBtn3(g_hBtnDelAct, 2);

        // 2-column buttons
        auto ReBtn2 = [&](HWND h, int idx) {
            if (!h || !IsWindow(h))return;
            int w2 = (rw - Sc(hWnd, 4)) / 2;
            RECT r{}; GetWindowRect(h, &r); MapWindowPoints(nullptr, hWnd, (LPPOINT)&r, 2);
            SetWindowPos(h, nullptr, rx + idx * (w2 + Sc(hWnd, 4)), r.top, w2, r.bottom - r.top, SWP_NOZORDER);
            };
        ReBtn2(g_hBtnUp, 0); ReBtn2(g_hBtnDown, 1);

        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    } // switch
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}



