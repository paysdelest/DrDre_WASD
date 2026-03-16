// keyboard_page_main.cpp
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "keyboard_ui.h"
#include "keyboard_ui_state.h"
#include "keyboard_ui_internal.h"

#include "keyboard_layout.h"
#include "keyboard_render.h"
#include "backend.h"
#include "bindings.h"
#include "profile_ini.h"
#include "remap_panel.h"
#include "keyboard_keysettings_panel.h"

#include "win_util.h"
#include "app_paths.h"
#include "ui_theme.h"
#include "tab_dark.h"

#include "binding_actions.h"
#include "remap_icons.h"
#include "settings.h"
#include "key_settings.h" // NEW
#include "free_combo_ui.h"
#include "free_combo_system.h" // FreeComboSystem::GetInjectedMouseState

#pragma comment(lib, "Comctl32.lib")

// Scaling shortcut
static int S(HWND hwnd, int px) { return WinUtil_ScalePx(hwnd, px); }
static void SetSelectedHid(uint16_t hid);
static void ResizeSubUi(HWND hWnd);
static LRESULT CALLBACK KeyBtnSubclassProc(HWND hBtn, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR dwRefData);
static void EnsureFreeComboPageCreated(HWND hTabParent);
//ajout-->
static constexpr int HOTKEY_EMERGENCY_STOP = 0xE5B;  // id arbitraire
static constexpr int HOTKEY_REMAP_TOGGLE = 0xE5C;  // F1: Ctrl+Alt+R
//ajout<--

struct KeyboardViewMetrics
{
    float scale = 1.0f;
    int offsetX = 0;
    int offsetY = 0;
    int scaledW = 0;
    int scaledH = 0;
};

static RECT  g_mouseViewRect{};
static HWND  g_hBtnRemapToggle = nullptr;  // F1: bouton toggle remap
static bool  g_btnRemapHot = false;    // F1: curseur sur le bouton
static bool  g_btnRemapPressed = false;    // F1: bouton enfoncé
static int KeyboardBottomPx(HWND hWnd); // forward declaration

// Previous mouse button state — only invalidate when something changes
static BYTE  g_mouseStatePrev = 0xFF;

// Mouse view style (vector fallback only): "Synapse" = sharper, more aggressive polygons.
// (Photo/PNG mode keeps using glow overlays.)
static bool g_mouseSynapseMode = true;

enum { MRECT_L = 0, MRECT_R = 1, MRECT_W = 2, MRECT_X1 = 3, MRECT_X2 = 4 };

// Normalized rectangles within the IMAGE DEST RECT (not the whole buffer).
// x,y,w,h in [0..1]. These values are tuned for mouse_transparent_defringed.png.
static float g_mouseRectN[5][4] =
{
    { 0.064f, 0.060f, 0.350f, 0.230f }, // L
    { 0.592f, 0.058f, 0.360f, 0.230f }, // R
    { 0.445f, 0.126f, 0.110f, 0.150f }, // Wheel
    { 0.010f, 0.480f, 0.070f, 0.140f }, // X1(back)
    { 0.004f, 0.330f, 0.070f, 0.140f }, // X2(forward)
};



// Synapse PNG mode: pointed polygons for L/R highlights.
// Points are normalized in IMAGE DEST space (imgDst), not the whole window.
enum { MPOLY_L = 0, MPOLY_R = 1, MPOLY_COUNT = 2, MPOLY_PTS = 6 };

static float g_mousePolyN[MPOLY_COUNT][MPOLY_PTS][2] =
{
    // L polygon (approx. follows the shell seam of the Viper V3 Pro photo)
    { {0.075f,0.080f},{0.430f,0.020f},{0.425f,0.150f},{0.420f,0.460f},{0.120f,0.460f},{0.080f,0.405f} }, // L
    // R polygon
    { {0.580f,0.150f},{0.585f,0.020f},{0.930f,0.075f},{0.920f,0.410f},{0.890f,0.460f},{0.575f,0.460f} }, // R
};

// force first paint
static DWORD g_wheelUpFlash = 0;    // tick when wheel-up was last seen (for flash)
static DWORD g_wheelDownFlash = 0;    // tick when wheel-down was last seen

static BYTE MouseCurrentState()
{
    BYTE s = 0;
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) s |= (1 << 0);
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) s |= (1 << 1);
    if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) s |= (1 << 2);
    if (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) s |= (1 << 3);
    if (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) s |= (1 << 4);
    // OR with macro-injected state so simulated clicks light up the view
    s |= FreeComboSystem::GetInjectedMouseState();
    // Wheel flash: bit 5=up, bit 6=down — active for 120ms after last scroll event
    DWORD now = GetTickCount();
    if (now - g_wheelUpFlash < 120) s |= (1 << 5);
    if (now - g_wheelDownFlash < 120) s |= (1 << 6);
    return s;
}

static void FillBezierShape(HDC hdc, const POINT* pts, int nPts, HBRUSH br, HPEN pn)
{
    BeginPath(hdc);
    MoveToEx(hdc, pts[0].x, pts[0].y, nullptr);
    PolyBezierTo(hdc, pts + 1, nPts - 1);
    CloseFigure(hdc);
    EndPath(hdc);
    SelectObject(hdc, br);
    SelectObject(hdc, pn);
    StrokeAndFillPath(hdc);
}

static void FillPolyShape(HDC hdc, const POINT* pts, int nPts, HBRUSH br, HPEN pn)
{
    if (!pts || nPts < 3) return;
    HGDIOBJ ob = SelectObject(hdc, br);
    HGDIOBJ op = SelectObject(hdc, pn);
    Polygon(hdc, pts, nPts);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
}

// "Synapse" aggressive polygon mouse (used when no PNG is present, or when you want a sharper look).
static void DrawMouseView_VectorSynapse(HWND hWnd, HDC hdc)
{
    RECT wrc{};
    GetClientRect(hWnd, &wrc);

    int topPad = S(hWnd, 8);
    int kbH = KeyboardBottomPx(hWnd) - topPad;
    int mvH = kbH;
    int mvW = (int)(mvH * 0.62f);

    int cx = wrc.right - mvW / 2 - S(hWnd, 10);
    int cy = topPad + mvH / 2;

    g_mouseViewRect = { cx - mvW / 2 - S(hWnd,4), topPad,
                        cx + mvW / 2 + S(hWnd,4), topPad + mvH };

    BYTE ms = g_mouseStatePrev;
    bool bL = (ms & (1 << 0)) != 0;
    bool bR = (ms & (1 << 1)) != 0;
    bool bM = (ms & (1 << 2)) != 0;
    bool bX1 = (ms & (1 << 3)) != 0;
    bool bX2 = (ms & (1 << 4)) != 0;
    bool bWU = (ms & (1 << 5)) != 0;
    bool bWD = (ms & (1 << 6)) != 0;

    // Same palette as the smooth fallback
    COLORREF cBody = RGB(30, 30, 40);
    COLORREF cBrdB = RGB(52, 52, 68);
    COLORREF cIdle = RGB(42, 42, 56);
    COLORREF cBrdI = RGB(60, 62, 80);
    COLORREF cPress = RGB(0, 180, 120);
    COLORREF cBrdP = RGB(0, 220, 150);
    COLORREF cWheel = RGB(24, 24, 34);
    COLORREF cWBrd = RGB(50, 52, 68);
    COLORREF cSep = RGB(48, 48, 64);
    COLORREF cTxt = RGB(120, 125, 148);
    COLORREF cTxtP = RGB(210, 255, 235);
    COLORREF cLine = RGB(55, 58, 76);
    COLORREF cDot = RGB(65, 68, 88);
    COLORREF cDotP = RGB(0, 210, 140);

    float unit = mvH * 0.5f / 100.0f;
    auto px = [&](float u) -> int { return (int)(cx + u * unit); };
    auto py = [&](float u) -> int { return (int)(cy + u * unit); };

    // ── Body (sharper silhouette) ─────────────────────────────────────────
    {
        // A more angular chassis with a defined "nose" and "tail".
        POINT body[] = {
            { px(0),   py(-98) },
            { px(28),  py(-92) },
            { px(46),  py(-70) },
            { px(54),  py(-40) },
            { px(56),  py(-8)  },
            { px(54),  py(34)  },
            { px(46),  py(70)  },
            { px(30),  py(92)  },
            { px(0),   py(98)  },
            { px(-30), py(92)  },
            { px(-46), py(70)  },
            { px(-54), py(34)  },
            { px(-56), py(-8)  },
            { px(-54), py(-40) },
            { px(-46), py(-70) },
            { px(-28), py(-92) },
        };
        HBRUSH br = CreateSolidBrush(cBody);
        HPEN   pn = CreatePen(PS_SOLID, 1, cBrdB);
        FillPolyShape(hdc, body, ARRAYSIZE(body), br, pn);
        DeleteObject(br);
        DeleteObject(pn);
    }

    // ── Left button (sharp inner tip) ─────────────────────────────────────
    {
        POINT lbtn[] = {
            { px(0),   py(-97) },
            { px(-26), py(-95) },
            { px(-48), py(-76) },
            { px(-54), py(-50) },
            { px(-52), py(-26) },
            { px(-18), py(-22) },
            { px(-6),  py(-24) },
            { px(0),   py(-32) }, // pointed tip toward center
        };
        HBRUSH br = CreateSolidBrush(bL ? cPress : cIdle);
        HPEN   pn = CreatePen(PS_SOLID, 1, bL ? cBrdP : cBrdI);
        FillPolyShape(hdc, lbtn, ARRAYSIZE(lbtn), br, pn);
        DeleteObject(br);
        DeleteObject(pn);
    }

    // ── Right button (sharp inner tip) ────────────────────────────────────
    {
        POINT rbtn[] = {
            { px(0),   py(-97) },
            { px(26),  py(-95) },
            { px(48),  py(-76) },
            { px(54),  py(-50) },
            { px(52),  py(-26) },
            { px(18),  py(-22) },
            { px(6),   py(-24) },
            { px(0),   py(-32) }, // pointed tip toward center
        };
        HBRUSH br = CreateSolidBrush(bR ? cPress : cIdle);
        HPEN   pn = CreatePen(PS_SOLID, 1, bR ? cBrdP : cBrdI);
        FillPolyShape(hdc, rbtn, ARRAYSIZE(rbtn), br, pn);
        DeleteObject(br);
        DeleteObject(pn);
    }

    // ── Separator ─────────────────────────────────────────────────────────
    {
        HPEN pn = CreatePen(PS_SOLID, 1, cSep);
        HGDIOBJ op = SelectObject(hdc, pn);
        MoveToEx(hdc, px(0), py(-97), nullptr);
        LineTo(hdc, px(0), py(-22));
        SelectObject(hdc, op);
        DeleteObject(pn);
    }

    // ── Scroll wheel (slightly more angular) ─────────────────────────────
    {
        int wx = px(0), wy = py(-52);
        int ww = S(hWnd, 9), wh = (int)(mvH * 0.20f);
        RECT wr{ wx - ww / 2, wy - wh / 2, wx + ww / 2, wy + wh / 2 };

        HBRUSH br = CreateSolidBrush(bM ? cPress : cWheel);
        HPEN   pn = CreatePen(PS_SOLID, 1, bM ? cBrdP : cWBrd);
        HGDIOBJ ob = SelectObject(hdc, br);
        HGDIOBJ op = SelectObject(hdc, pn);
        Rectangle(hdc, wr.left, wr.top, wr.right, wr.bottom);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(br);
        DeleteObject(pn);

        // Wheel-up arrow (▲) above wheel
        {
            int ax = wx, ay = wr.top - S(hWnd, 2);
            int aw = S(hWnd, 6);
            COLORREF ca = bWU ? cPress : cWBrd;
            HBRUSH ab = CreateSolidBrush(ca);
            HPEN   ap = CreatePen(PS_SOLID, 1, ca);
            POINT tri[] = { {ax, ay - aw}, {ax - aw, ay}, {ax + aw, ay} };
            FillPolyShape(hdc, tri, 3, ab, ap);
            DeleteObject(ab);
            DeleteObject(ap);
        }
        // Wheel-down arrow (▼) below wheel
        {
            int ax = wx, ay = wr.bottom + S(hWnd, 2);
            int aw = S(hWnd, 6);
            COLORREF ca = bWD ? cPress : cWBrd;
            HBRUSH ab = CreateSolidBrush(ca);
            HPEN   ap = CreatePen(PS_SOLID, 1, ca);
            POINT tri[] = { {ax, ay + aw}, {ax - aw, ay}, {ax + aw, ay} };
            FillPolyShape(hdc, tri, 3, ab, ap);
            DeleteObject(ab);
            DeleteObject(ap);
        }
    }

    // ── Side buttons X1 / X2 (sharper trapezoids) ─────────────────────────
    auto DrawSideBtn = [&](float topU, float btmU, bool pressed, const wchar_t* lbl)
        {
            int bx1 = px(-56), bx2 = px(-34);
            int by1 = py(topU), by2 = py(btmU);
            // Trapezoid with a "bite" toward the body for a gaming look
            POINT poly[] = {
                { bx1, by1 },
                { bx2, by1 + S(hWnd, 2) },
                { bx2 - S(hWnd, 4), by2 },
                { bx1, by2 },
            };
            HBRUSH br = CreateSolidBrush(pressed ? cPress : cIdle);
            HPEN   pn = CreatePen(PS_SOLID, 1, pressed ? cBrdP : cBrdI);
            FillPolyShape(hdc, poly, ARRAYSIZE(poly), br, pn);
            DeleteObject(br);
            DeleteObject(pn);

            HFONT hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HFONT oldF = (HFONT)SelectObject(hdc, hf);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, pressed ? cTxtP : cTxt);
            RECT r{ bx1, by1, bx2, by2 };
            DrawTextW(hdc, lbl, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(hdc, oldF);
        };
    // Keep the same logical mapping as your current visual: top = X1, bottom = X2
    DrawSideBtn(-15, 12, bX2, L"X2");
    DrawSideBtn(16, 43, bX1, L"X1");

    // ── Callout labels (same style as smooth fallback) ───────────────────
    HFONT hfSmall = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SetBkMode(hdc, TRANSPARENT);

    auto DrawCallout = [&](float anchorX, float anchorY, bool right, bool pressed, const wchar_t* lbl)
        {
            int ax = px(anchorX), ay = py(anchorY);
            int lineLen = S(hWnd, 18);
            int ex = right ? ax + lineLen : ax - lineLen;

            COLORREF dc = pressed ? cDotP : cDot;
            HBRUSH db = CreateSolidBrush(dc);
            HPEN   dp = CreatePen(PS_SOLID, 1, dc);
            HGDIOBJ ob = SelectObject(hdc, db);
            HGDIOBJ op = SelectObject(hdc, dp);
            Ellipse(hdc, ax - S(hWnd, 2), ay - S(hWnd, 2), ax + S(hWnd, 2), ay + S(hWnd, 2));
            SelectObject(hdc, ob);
            SelectObject(hdc, op);
            DeleteObject(db);
            DeleteObject(dp);

            HPEN lp = CreatePen(PS_SOLID, 1, pressed ? cDotP : cLine);
            HGDIOBJ ol = SelectObject(hdc, lp);
            MoveToEx(hdc, ax, ay, nullptr);
            LineTo(hdc, ex, ay);
            SelectObject(hdc, ol);
            DeleteObject(lp);

            HFONT oldF = (HFONT)SelectObject(hdc, hfSmall);
            SetTextColor(hdc, pressed ? cTxtP : cTxt);
            int tw = S(hWnd, 26);
            RECT tr;
            if (right) tr = { ex + S(hWnd,2), ay - S(hWnd,7), ex + tw, ay + S(hWnd,7) };
            else       tr = { ex - tw,         ay - S(hWnd,7), ex - S(hWnd,2), ay + S(hWnd,7) };
            DrawTextW(hdc, lbl, -1, &tr,
                DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | (right ? DT_LEFT : DT_RIGHT));
            SelectObject(hdc, oldF);
        };

    DrawCallout(-43.0f, -65.0f, false, bL, L"L");
    DrawCallout(-43.0f, -4.0f, false, bX2, L"X2");
    DrawCallout(-43.0f, 28.0f, false, bX1, L"X1");

    DrawCallout(43.0f, -65.0f, true, bR, L"R");
    DrawCallout(8.0f, -75.0f, true, bM, L"M");
    DrawCallout(8.0f, -84.0f, true, bWU, L"▲");
    DrawCallout(8.0f, -42.0f, true, bWD, L"▼");
}

static void DrawMouseView_VectorFallback(HWND hWnd, HDC hdc)
{
    RECT wrc{};
    GetClientRect(hWnd, &wrc);

    int topPad = S(hWnd, 8);
    int kbH = KeyboardBottomPx(hWnd) - topPad;
    int mvH = kbH;
    int mvW = (int)(mvH * 0.62f);

    int cx = wrc.right - mvW / 2 - S(hWnd, 10);
    int cy = topPad + mvH / 2;

    g_mouseViewRect = { cx - mvW / 2 - S(hWnd,4), topPad,
                        cx + mvW / 2 + S(hWnd,4), topPad + mvH };

    BYTE ms = g_mouseStatePrev;
    bool bL = (ms & (1 << 0)) != 0;
    bool bR = (ms & (1 << 1)) != 0;
    bool bM = (ms & (1 << 2)) != 0;
    bool bX1 = (ms & (1 << 3)) != 0;
    bool bX2 = (ms & (1 << 4)) != 0;
    bool bWU = (ms & (1 << 5)) != 0;  // wheel up flash
    bool bWD = (ms & (1 << 6)) != 0;  // wheel down flash

    // ── Palette flat dark (Razer Synapse inspired) ───────────────────────────
    COLORREF cBg = RGB(18, 18, 24);   // window background behind mouse
    COLORREF cBody = RGB(30, 30, 40);   // mouse body fill
    COLORREF cBrdB = RGB(52, 52, 68);   // body border
    COLORREF cIdle = RGB(42, 42, 56);   // button idle fill
    COLORREF cBrdI = RGB(60, 62, 80);   // button idle border
    COLORREF cPress = RGB(0, 180, 120);  // active — Razer green
    COLORREF cBrdP = RGB(0, 220, 150);  // active border
    COLORREF cWheel = RGB(24, 24, 34);   // scroll wheel fill
    COLORREF cWBrd = RGB(50, 52, 68);   // scroll wheel border
    COLORREF cSep = RGB(48, 48, 64);   // separator line
    COLORREF cTxt = RGB(120, 125, 148);// label idle
    COLORREF cTxtP = RGB(210, 255, 235);// label active
    COLORREF cLine = RGB(55, 58, 76);   // callout lines
    COLORREF cDot = RGB(65, 68, 88);   // callout dot idle
    COLORREF cDotP = RGB(0, 210, 140);  // callout dot active

    float unit = mvH * 0.5f / 100.0f;
    auto px = [&](float u) -> int { return (int)(cx + u * unit); };
    auto py = [&](float u) -> int { return (int)(cy + u * unit); };

    // ── Body ─────────────────────────────────────────────────────────────────
    {
        POINT body[] = {
            {px(0),  py(-98)},
            {px(22), py(-98)}, {px(46), py(-75)}, {px(48), py(-45)},
            {px(50), py(-10)}, {px(50), py(30)}, {px(46), py(65)},
            {px(42), py(90)}, {px(22), py(98)}, {px(0), py(98)},
            {px(-22), py(98)}, {px(-42), py(90)}, {px(-46), py(65)},
            {px(-50), py(30)}, {px(-50), py(-10)}, {px(-48), py(-45)},
            {px(-46), py(-75)}, {px(-22), py(-98)}, {px(0), py(-98)},
        };
        HBRUSH br = CreateSolidBrush(cBody);
        HPEN   pn = CreatePen(PS_SOLID, 1, cBrdB);
        FillBezierShape(hdc, body, ARRAYSIZE(body), br, pn);
        DeleteObject(br); DeleteObject(pn);
    }

    // ── Left button ──────────────────────────────────────────────────────────
    {
        POINT lbtn[] = {
            {px(0),  py(-97)},
            {px(-2),  py(-97)}, {px(-22), py(-87)}, {px(-40), py(-64)},
            {px(-43), py(-48)}, {px(-43), py(-22)}, {px(-2), py(-22)},
            {px(-1),  py(-22)}, {px(0), py(-22)}, {px(0), py(-97)},
        };
        HBRUSH br = CreateSolidBrush(bL ? cPress : cIdle);
        HPEN   pn = CreatePen(PS_SOLID, 1, bL ? cBrdP : cBrdI);
        FillBezierShape(hdc, lbtn, ARRAYSIZE(lbtn), br, pn);
        DeleteObject(br); DeleteObject(pn);
    }

    // ── Right button ─────────────────────────────────────────────────────────
    {
        POINT rbtn[] = {
            {px(0),  py(-97)},
            {px(2),  py(-97)}, {px(22), py(-87)}, {px(40), py(-64)},
            {px(43), py(-48)}, {px(43), py(-22)}, {px(2), py(-22)},
            {px(1),  py(-22)}, {px(0), py(-22)}, {px(0), py(-97)},
        };
        HBRUSH br = CreateSolidBrush(bR ? cPress : cIdle);
        HPEN   pn = CreatePen(PS_SOLID, 1, bR ? cBrdP : cBrdI);
        FillBezierShape(hdc, rbtn, ARRAYSIZE(rbtn), br, pn);
        DeleteObject(br); DeleteObject(pn);
    }

    // ── Separator ────────────────────────────────────────────────────────────
    {
        HPEN pn = CreatePen(PS_SOLID, 1, cSep);
        HGDIOBJ op = SelectObject(hdc, pn);
        MoveToEx(hdc, px(0), py(-97), nullptr);
        LineTo(hdc, px(0), py(-22));
        SelectObject(hdc, op); DeleteObject(pn);
    }

    // ── Scroll wheel ─────────────────────────────────────────────────────────
    {
        int wx = px(0), wy = py(-52);
        int ww = S(hWnd, 8), wh = (int)(mvH * 0.20f);
        RECT wr{ wx - ww / 2, wy - wh / 2, wx + ww / 2, wy + wh / 2 };
        // Wheel body
        HBRUSH br = CreateSolidBrush(bM ? cPress : cWheel);
        HPEN   pn = CreatePen(PS_SOLID, 1, bM ? cBrdP : cWBrd);
        HGDIOBJ ob = SelectObject(hdc, br);
        HGDIOBJ op = SelectObject(hdc, pn);
        RoundRect(hdc, wr.left, wr.top, wr.right, wr.bottom, S(hWnd, 3), S(hWnd, 3));
        SelectObject(hdc, ob); SelectObject(hdc, op);
        DeleteObject(br); DeleteObject(pn);

        // Wheel-up arrow (▲) above wheel
        {
            int ax = wx, ay = wr.top - S(hWnd, 2);
            int aw = S(hWnd, 5);
            COLORREF ca = bWU ? cPress : cWBrd;
            HBRUSH ab = CreateSolidBrush(ca);
            HPEN   ap = CreatePen(PS_SOLID, 1, ca);
            POINT tri[] = { {ax, ay - aw}, {ax - aw, ay}, {ax + aw, ay} };
            HGDIOBJ ob2 = SelectObject(hdc, ab);
            HGDIOBJ op2 = SelectObject(hdc, ap);
            Polygon(hdc, tri, 3);
            SelectObject(hdc, ob2); SelectObject(hdc, op2);
            DeleteObject(ab); DeleteObject(ap);
        }
        // Wheel-down arrow (▼) below wheel
        {
            int ax = wx, ay = wr.bottom + S(hWnd, 2);
            int aw = S(hWnd, 5);
            COLORREF ca = bWD ? cPress : cWBrd;
            HBRUSH ab = CreateSolidBrush(ca);
            HPEN   ap = CreatePen(PS_SOLID, 1, ca);
            POINT tri[] = { {ax, ay + aw}, {ax - aw, ay}, {ax + aw, ay} };
            HGDIOBJ ob2 = SelectObject(hdc, ab);
            HGDIOBJ op2 = SelectObject(hdc, ap);
            Polygon(hdc, tri, 3);
            SelectObject(hdc, ob2); SelectObject(hdc, op2);
            DeleteObject(ab); DeleteObject(ap);
        }
    }

    // ── Side buttons X1 / X2 (left flank) ───────────────────────────────────
    auto DrawSideBtn = [&](float topU, float btmU, bool pressed, const wchar_t* lbl)
        {
            int bx1 = px(-52), bx2 = px(-36);
            int by1 = py(topU), by2 = py(btmU);
            HBRUSH br = CreateSolidBrush(pressed ? cPress : cIdle);
            HPEN   pn = CreatePen(PS_SOLID, 1, pressed ? cBrdP : cBrdI);
            HGDIOBJ ob = SelectObject(hdc, br);
            HGDIOBJ op = SelectObject(hdc, pn);
            RoundRect(hdc, bx1, by1, bx2, by2, S(hWnd, 3), S(hWnd, 3));
            SelectObject(hdc, ob); SelectObject(hdc, op);
            DeleteObject(br); DeleteObject(pn);
            HFONT hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HFONT oldF = (HFONT)SelectObject(hdc, hf);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, pressed ? cTxtP : cTxt);
            RECT r{ bx1, by1, bx2, by2 };
            DrawTextW(hdc, lbl, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(hdc, oldF);
        };
    DrawSideBtn(-15, 12, bX2, L"X2");
    DrawSideBtn(16, 43, bX1, L"X1");

    // ── Callout labels style Razer Synapse ───────────────────────────────────
    // Petite ligne horizontale + point d'ancrage + texte à droite ou gauche
    HFONT hfSmall = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SetBkMode(hdc, TRANSPARENT);

    auto DrawCallout = [&](float anchorX, float anchorY,
        bool right, bool pressed, const wchar_t* lbl)
        {
            int ax = px(anchorX), ay = py(anchorY);
            int lineLen = S(hWnd, 18);
            int ex = right ? ax + lineLen : ax - lineLen;

            // Dot at anchor
            COLORREF dc = pressed ? cDotP : cDot;
            HBRUSH db = CreateSolidBrush(dc);
            HPEN   dp = CreatePen(PS_SOLID, 1, dc);
            HGDIOBJ ob = SelectObject(hdc, db);
            HGDIOBJ op = SelectObject(hdc, dp);
            Ellipse(hdc, ax - S(hWnd, 2), ay - S(hWnd, 2), ax + S(hWnd, 2), ay + S(hWnd, 2));
            SelectObject(hdc, ob); SelectObject(hdc, op);
            DeleteObject(db); DeleteObject(dp);

            // Horizontal line
            HPEN lp = CreatePen(PS_SOLID, 1, pressed ? cDotP : cLine);
            HGDIOBJ ol = SelectObject(hdc, lp);
            MoveToEx(hdc, ax, ay, nullptr);
            LineTo(hdc, ex, ay);
            SelectObject(hdc, ol); DeleteObject(lp);

            // Text
            HFONT oldF = (HFONT)SelectObject(hdc, hfSmall);
            SetTextColor(hdc, pressed ? cTxtP : cTxt);
            int tw = S(hWnd, 26);
            RECT tr;
            if (right) tr = { ex + S(hWnd,2), ay - S(hWnd,7), ex + tw, ay + S(hWnd,7) };
            else       tr = { ex - tw,         ay - S(hWnd,7), ex - S(hWnd,2), ay + S(hWnd,7) };
            DrawTextW(hdc, lbl, -1, &tr,
                DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
                (right ? DT_LEFT : DT_RIGHT));
            SelectObject(hdc, oldF);
        };

    // Left side callouts (pointing right toward mouse)
    DrawCallout(-43.0f, -65.0f, false, bL, L"L");
    DrawCallout(-43.0f, -4.0f, false, bX2, L"X2");
    DrawCallout(-43.0f, 28.0f, false, bX1, L"X1");

    // Right side callouts
    DrawCallout(43.0f, -65.0f, true, bR, L"R");
    DrawCallout(8.0f, -75.0f, true, bM, L"M");
    DrawCallout(8.0f, -84.0f, true, bWU, L"▲");  // ▲ wheel up
    DrawCallout(8.0f, -42.0f, true, bWD, L"▼");  // ▼ wheel down
}

// ---------- Photo mouse overlay (optional) + Calibration Mode ----------
static ULONG_PTR g_mouseGdiToken = 0;
static bool g_mouseGdiInit = false;
static Gdiplus::Bitmap* g_mouseBmp = nullptr;

static void MouseEnsureGdiPlus()
{
    if (g_mouseGdiInit) return;
    Gdiplus::GdiplusStartupInput si;
    if (Gdiplus::GdiplusStartup(&g_mouseGdiToken, &si, nullptr) == Gdiplus::Ok)
        g_mouseGdiInit = true;
}

static std::wstring MouseGetExeDir()
{
    wchar_t p[MAX_PATH]{};
    GetModuleFileNameW(nullptr, p, MAX_PATH);
    std::wstring s(p);
    size_t k = s.find_last_of(L"\\/");
    if (k != std::wstring::npos) s.resize(k);
    return s;
}

static std::wstring MouseGetCwd()
{
    wchar_t p[MAX_PATH]{};
    GetCurrentDirectoryW(MAX_PATH, p);
    return std::wstring(p);
}

static void MouseTryLoad(const std::wstring& path)
{
    if (g_mouseBmp) return;
    Gdiplus::Bitmap* b = Gdiplus::Bitmap::FromFile(path.c_str(), FALSE);
    if (b && b->GetLastStatus() == Gdiplus::Ok) { g_mouseBmp = b; return; }
    if (b) delete b;
}

static void MouseLoadBitmapOnce()
{
    if (g_mouseBmp) return;
    MouseEnsureGdiPlus();

    const std::wstring exeDir = MouseGetExeDir();
    const std::wstring cwd = MouseGetCwd();
    const std::wstring names[] =
    {
        L"mouse.png",
        L"mouse_transparent_defringed.png",
        L"mouse_transparent.png",
        L"mouse_razer_viper_v3_pro.png"
    };

    for (const auto& n : names)
    {
        MouseTryLoad(exeDir + L"\\assets\\" + n);
        MouseTryLoad(exeDir + L"\\" + n);
        MouseTryLoad(cwd + L"\\assets\\" + n);
        MouseTryLoad(cwd + L"\\" + n);
        if (g_mouseBmp) break;
    }
}

static void MouseDrawGlowRect(Gdiplus::Graphics& g, const Gdiplus::RectF& r, bool on, const Gdiplus::Color& core)
{
    if (!on) return;
    using namespace Gdiplus;

    auto AddRoundRect = [&](GraphicsPath& p, RectF rr, REAL rad)
        {
            REAL d = rad * 2.0f;
            p.AddArc(rr.X, rr.Y, d, d, 180, 90);
            p.AddArc(rr.X + rr.Width - d, rr.Y, d, d, 270, 90);
            p.AddArc(rr.X + rr.Width - d, rr.Y + rr.Height - d, d, d, 0, 90);
            p.AddArc(rr.X, rr.Y + rr.Height - d, d, d, 90, 90);
            p.CloseFigure();
        };

    // Synapse-like green glow stack
    const Color c1(46, core.GetR(), core.GetG(), core.GetB());
    const Color c2(28, core.GetR(), core.GetG(), core.GetB());
    const Color c3(16, core.GetR(), core.GetG(), core.GetB());

    REAL rad = std::max<REAL>(6.0f, r.Height * 0.18f);
    for (int i = 0; i < 3; ++i)
    {
        RectF e = r;
        e.Inflate(4.0f + i * 3.0f, 4.0f + i * 3.0f);
        GraphicsPath p(FillModeWinding);
        AddRoundRect(p, e, rad + i * 2.0f);
        SolidBrush br(i == 0 ? c1 : (i == 1 ? c2 : c3));
        g.FillPath(&br, &p);
    }

    GraphicsPath p2(FillModeWinding);
    AddRoundRect(p2, r, rad);
    Pen p(Color(120, core.GetR(), core.GetG(), core.GetB()), 2.0f);
    g.DrawPath(&p, &p2);
}


// ---------- Synapse-style sharp polygon overlay (PNG mode) ----------
// We keep wheel + X buttons as rectangular regions (already calibrated).
// L/R are rendered as sharp polygons built from their calibrated rectangles.
static void MouseBuildSynapseLRPath(Gdiplus::GraphicsPath& out, const Gdiplus::RectF& r, bool left)
{
    using namespace Gdiplus;
    out.Reset();

    const REAL x0 = r.X;
    const REAL y0 = r.Y;
    const REAL x1 = r.X + r.Width;
    const REAL y1 = r.Y + r.Height;

    // Tunables (feel free to tweak; rect calibration stays valid)
    const REAL innerTopX = left ? (x0 + r.Width * 0.58f) : (x1 - r.Width * 0.58f);
    const REAL innerBotX = left ? (x0 + r.Width * 0.62f) : (x1 - r.Width * 0.62f);

    const REAL tipX = left ? (x0 - r.Width * 0.10f) : (x1 + r.Width * 0.10f);
    const REAL tipY = y0 + r.Height * 0.18f;

    const REAL outerTopX = left ? (x0 + r.Width * 0.08f) : (x1 - r.Width * 0.08f);
    const REAL outerTopY = y0 + r.Height * 0.03f;

    const REAL outerBotX = left ? (x0 + r.Width * 0.10f) : (x1 - r.Width * 0.10f);
    const REAL outerBotY = y1 - r.Height * 0.08f;

    const REAL innerMidY = y0 + r.Height * 0.55f;

    PointF pts[6]{};
    if (left)
    {
        pts[0] = PointF(innerTopX, y0);        // inner top (near middle split)
        pts[1] = PointF(outerTopX, outerTopY); // outer top
        pts[2] = PointF(tipX, tipY);      // sharp forward tip (aggressive)
        pts[3] = PointF(outerBotX, outerBotY); // outer bottom
        pts[4] = PointF(innerBotX, y1);        // inner bottom
        pts[5] = PointF(innerTopX + r.Width * 0.02f, innerMidY); // inner mid
    }
    else
    {
        pts[0] = PointF(innerTopX, y0);
        pts[1] = PointF(outerTopX, outerTopY);
        pts[2] = PointF(tipX, tipY);
        pts[3] = PointF(outerBotX, outerBotY);
        pts[4] = PointF(innerBotX, y1);
        pts[5] = PointF(innerTopX - r.Width * 0.02f, innerMidY);
    }

    out.AddPolygon(pts, 6);
}

static void MouseDrawGlowPath(Gdiplus::Graphics& g, Gdiplus::GraphicsPath& path, bool on, const Gdiplus::Color& core)
{
    if (!on) return;
    using namespace Gdiplus;

    const Color c0(70, core.GetR(), core.GetG(), core.GetB());
    const Color c1(38, core.GetR(), core.GetG(), core.GetB());
    const Color c2(20, core.GetR(), core.GetG(), core.GetB());

    // Draw a stacked outline glow. Keep corners sharp (Synapse-like).
    Pen p0(c0, 10.0f); p0.SetLineJoin(LineJoinMiter); p0.SetMiterLimit(2.0f);
    Pen p1(c1, 6.0f); p1.SetLineJoin(LineJoinMiter); p1.SetMiterLimit(2.0f);
    Pen p2(c2, 3.0f); p2.SetLineJoin(LineJoinMiter); p2.SetMiterLimit(2.0f);
    g.DrawPath(&p0, &path);
    g.DrawPath(&p1, &path);
    g.DrawPath(&p2, &path);

    // Core border
    Pen p3(Color(160, core.GetR(), core.GetG(), core.GetB()), 2.0f);
    p3.SetLineJoin(LineJoinMiter); p3.SetMiterLimit(2.0f);
    g.DrawPath(&p3, &path);
}

static void MouseBuildPolyPathFromNorm(Gdiplus::GraphicsPath& out, const Gdiplus::RectF& imgDst, int side)
{
    using namespace Gdiplus;
    side = std::clamp(side, 0, MPOLY_COUNT - 1);

    PointF pts[MPOLY_PTS]{};
    for (int i = 0; i < MPOLY_PTS; ++i)
    {
        float nx = g_mousePolyN[side][i][0];
        float ny = g_mousePolyN[side][i][1];
        pts[i] = PointF(imgDst.X + imgDst.Width * nx, imgDst.Y + imgDst.Height * ny);
    }

    out.Reset();
    out.AddPolygon(pts, MPOLY_PTS);
}

static void MouseDrawIdleOutlinePath(Gdiplus::Graphics& g, Gdiplus::GraphicsPath& path, const Gdiplus::Color& c)
{
    using namespace Gdiplus;
    Pen p(c, 1.5f);
    p.SetLineJoin(LineJoinMiter);
    p.SetMiterLimit(2.0f);
    g.DrawPath(&p, &path);
}


void DrawMouseView(HWND hWnd, HDC hdc)
{
    // First compute the mouse view rect (same as original)
    RECT wrc{}; GetClientRect(hWnd, &wrc);
    int topPad = S(hWnd, 8);
    int mvH = S(hWnd, 235);
    int mvW = (int)(mvH * 0.62f);

    int cx = wrc.right - mvW / 2 - S(hWnd, 10);
    g_mouseViewRect = { cx - mvW / 2 - S(hWnd,4), topPad,
                        cx + mvW / 2 + S(hWnd,4), topPad + mvH };

    const int bw = g_mouseViewRect.right - g_mouseViewRect.left;
    const int bh = g_mouseViewRect.bottom - g_mouseViewRect.top;
    if (bw <= 0 || bh <= 0)
        return;

    BYTE ms = g_mouseStatePrev;
    const bool bL = (ms & (1 << 0)) != 0;
    const bool bR = (ms & (1 << 1)) != 0;
    const bool bM = (ms & (1 << 2)) != 0;
    const bool bX1 = (ms & (1 << 3)) != 0; // XBUTTON1 (Back)
    const bool bX2 = (ms & (1 << 4)) != 0; // XBUTTON2 (Forward)
    const bool bWU = (ms & (1 << 5)) != 0; // wheel up flash
    const bool bWD = (ms & (1 << 6)) != 0; // wheel down flash

    MouseLoadBitmapOnce();

    // If no photo available: fallback to your old vector mouse view (so users don't need a PNG).
    if (!g_mouseBmp)
    {
        if (g_mouseSynapseMode)
            DrawMouseView_VectorSynapse(hWnd, hdc);
        else
            DrawMouseView_VectorFallback(hWnd, hdc);
        return;
    }

    using namespace Gdiplus;
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);

    // Panel background already filled by caller buffer; keep clean
    RectF rc((REAL)g_mouseViewRect.left, (REAL)g_mouseViewRect.top, (REAL)bw, (REAL)bh);

    const REAL pad = (REAL)S(hWnd, 6);

    // Fit image into rc with aspect
    const REAL iw = (REAL)g_mouseBmp->GetWidth();
    const REAL ih = (REAL)g_mouseBmp->GetHeight();
    REAL availW = rc.Width - pad * 2.0f;
    REAL availH = rc.Height - pad * 2.0f;
    REAL s = (availW / iw < availH / ih) ? (availW / iw) : (availH / ih);
    REAL dw = iw * s, dh = ih * s;
    REAL dx = rc.X + (rc.Width - dw) * 0.5f;
    REAL dy = rc.Y + (rc.Height - dh) * 0.5f;


    RectF imgDst(dx, dy, dw, dh);
    // Draw mouse image (treat near-white background as transparent)
    {
        ImageAttributes ia;
        ia.SetColorKey(Color(255, 235, 235, 235), Color(255, 255, 255, 255), ColorAdjustTypeBitmap);
        g.DrawImage(g_mouseBmp, imgDst,
            0.0f, 0.0f, iw, ih,
            UnitPixel, &ia);
    }

    // Map normalized rect -> pixel rect in image
    auto NR = [&](int idx)->RectF
        {
            const float nx = g_mouseRectN[idx][0];
            const float ny = g_mouseRectN[idx][1];
            const float nw = g_mouseRectN[idx][2];
            const float nh = g_mouseRectN[idx][3];
            return RectF(imgDst.X + imgDst.Width * nx, imgDst.Y + imgDst.Height * ny,
                imgDst.Width * nw, imgDst.Height * nh);
        };

    const Color green(0, 0, 0, 0); // dummy

    // Glows / outlines
    const Color synGreen(68, 214, 44);

    // X1(back)=lower button, X2(forward)=upper button on Viper V3 Pro photo.
    // MRECT_X1 rect is physically lower, MRECT_X2 is higher — swap booleans to match.
    MouseDrawGlowRect(g, NR(MRECT_X1), bX1, synGreen);  // lower rect = X1
    MouseDrawGlowRect(g, NR(MRECT_X2), bX2, synGreen);  // upper rect = X2
    MouseDrawGlowRect(g, NR(MRECT_W), bM, synGreen);

    // L / R : Synapse sharp polygon mode (calibratable polygons)
    if (g_mouseSynapseMode)
    {
        const Color idleOutline(70, 120, 125, 148); // subtle outline when idle

        GraphicsPath pL(FillModeWinding), pR(FillModeWinding);
        MouseBuildPolyPathFromNorm(pL, imgDst, MPOLY_L);
        MouseBuildPolyPathFromNorm(pR, imgDst, MPOLY_R);

        MouseDrawIdleOutlinePath(g, pL, idleOutline);
        MouseDrawIdleOutlinePath(g, pR, idleOutline);

        MouseDrawGlowPath(g, pL, bL, synGreen);
        MouseDrawGlowPath(g, pR, bR, synGreen);
    }
    else
    {
        // Classic rectangular glow (legacy)
        MouseDrawGlowRect(g, NR(MRECT_L), bL, synGreen);
        MouseDrawGlowRect(g, NR(MRECT_R), bR, synGreen);
    }

    // Wheel up/down arrows (flash)
    if (bWU || bWD)
    {
        Pen p(Color(160, synGreen.GetR(), synGreen.GetG(), synGreen.GetB()), 2.0f);
        RectF rW = NR(MRECT_W);
        PointF up[3] = { PointF(rW.X + rW.Width * 0.5f, rW.Y + rW.Height * 0.05f),
                         PointF(rW.X + rW.Width * 0.3f, rW.Y + rW.Height * 0.18f),
                         PointF(rW.X + rW.Width * 0.7f, rW.Y + rW.Height * 0.18f) };
        PointF dn[3] = { PointF(rW.X + rW.Width * 0.5f, rW.Y + rW.Height * 0.95f),
                         PointF(rW.X + rW.Width * 0.3f, rW.Y + rW.Height * 0.82f),
                         PointF(rW.X + rW.Width * 0.7f, rW.Y + rW.Height * 0.82f) };
        if (bWU) g.DrawPolygon(&p, up, 3);
        if (bWD) g.DrawPolygon(&p, dn, 3);
    }

    // ── L/R centrés sur les boutons (GDI+) ──────────────────────────────────
    {
        using namespace Gdiplus;
        FontFamily ff(L"Segoe UI");
        bool ok = (ff.GetLastStatus() == Ok);
        Font fnt(ok ? L"Segoe UI Light" : L"Segoe UI", 9.0f, FontStyleRegular, UnitPoint);
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);
        RectF rL = NR(MRECT_L), rR = NR(MRECT_R);
        // L
        {
            BYTE a = bL ? 220 : 68;
            Color c(a, bL ? 68 : 105, bL ? 214 : 115, bL ? 44 : 140);
            SolidBrush br(c);
            RectF tr(rL.X + rL.Width * 0.15f, rL.Y + rL.Height * 0.28f,
                rL.Width * 0.58f, rL.Height * 0.44f);
            g.DrawString(L"L", -1, &fnt, tr, &sf, &br);
        }
        // R
        {
            BYTE a = bR ? 220 : 68;
            Color c(a, bR ? 68 : 105, bR ? 214 : 115, bR ? 44 : 140);
            SolidBrush br(c);
            RectF tr(rR.X + rR.Width * 0.27f, rR.Y + rR.Height * 0.28f,
                rR.Width * 0.58f, rR.Height * 0.44f);
            g.DrawString(L"R", -1, &fnt, tr, &sf, &br);
        }
    }

    // ── X1/X2 callouts : ligne fine qui s'estompe + label ────────────────────
    {
        using namespace Gdiplus;
        FontFamily ff(L"Segoe UI");
        bool ok = (ff.GetLastStatus() == Ok);
        Font fntSm(ok ? L"Segoe UI" : L"Arial", 7.0f, FontStyleRegular, UnitPoint);
        StringFormat sfEnd;
        sfEnd.SetAlignment(StringAlignmentFar);
        sfEnd.SetLineAlignment(StringAlignmentCenter);

        // MRECT_X1 = zone basse = X1 (back),  MRECT_X2 = zone haute = X2 (forward)
        RectF rX1 = NR(MRECT_X1);
        RectF rX2 = NR(MRECT_X2);
        float lineLen = imgDst.Width * 0.22f;

        auto DrawXCallout = [&](RectF zone, bool active, const wchar_t* lbl)
            {
                float ax = zone.X;
                float ay = zone.Y + zone.Height * 0.5f;
                Color cA(255, 68, 214, 44);   // vert Razer actif
                Color cI(255, 85, 95, 115);   // gris discret idle
                Color cU = active ? cA : cI;
                BYTE aLine = active ? 175 : 38;
                BYTE aTxt = active ? 205 : 68;

                // Dot d'ancrage
                SolidBrush dotBr(Color(aTxt, cU.GetR(), cU.GetG(), cU.GetB()));
                REAL dr = active ? 2.5f : 1.8f;
                g.FillEllipse(&dotBr, ax - dr, ay - dr, dr * 2.0f, dr * 2.0f);

                // Ligne en 4 segments qui s'estompe vers l'extrémité
                for (int i = 0; i < 4; ++i)
                {
                    float t0 = i / 4.0f, t1 = (i + 1) / 4.0f;
                    BYTE  a0 = (BYTE)(aLine * (1.0f - t0 * 0.72f));
                    Pen sp(Color(a0, cU.GetR(), cU.GetG(), cU.GetB()), 1.0f);
                    g.DrawLine(&sp, ax - lineLen * t0, ay, ax - lineLen * t1, ay);
                }

                // Label aligné à droite au bout
                SolidBrush txtBr(Color(aTxt, cU.GetR(), cU.GetG(), cU.GetB()));
                RectF tr(ax - lineLen - 22.0f, ay - 8.0f, 22.0f, 16.0f);
                g.DrawString(lbl, -1, &fntSm, tr, &sfEnd, &txtBr);
            };

        DrawXCallout(rX1, bX1, L"X1");
        DrawXCallout(rX2, bX2, L"X2");
    }
}


static void ComputeKeyboardViewMetrics(HWND hWnd, KeyboardViewMetrics& out)
{
    const KeyDef* keys = KeyboardLayout_Data();
    int n = KeyboardLayout_Count();

    int maxX = 1;
    int maxBottom = KEYBOARD_KEY_H;
    for (int i = 0; i < n; ++i)
    {
        maxX = std::max(maxX, keys[i].x + std::max(KEYBOARD_KEY_MIN_DIM, keys[i].w));
        int bottom = keys[i].row * KEYBOARD_ROW_PITCH_Y + std::max(KEYBOARD_KEY_MIN_DIM, keys[i].h);
        if (bottom > maxBottom) maxBottom = bottom;
    }

    int modelW = KEYBOARD_MARGIN_X + maxX + KEYBOARD_MARGIN_X;
    int modelH = KEYBOARD_MARGIN_Y + maxBottom;

    int baseW = std::max(1, S(hWnd, modelW));
    int baseH = std::max(1, S(hWnd, modelH));

    RECT rc{};
    GetClientRect(hWnd, &rc);
    int cw = std::max(1, (int)(rc.right - rc.left));
    int ch = std::max(1, (int)(rc.bottom - rc.top));

    int sidePad = S(hWnd, 24);
    int topPad = S(hWnd, 8);
    int bottomPad = S(hWnd, 12);
    int minSubH = S(hWnd, 170);

    int availW = std::max(1, cw - sidePad);
    int availH = std::max(1, ch - topPad - bottomPad - minSubH);

    float sx = (float)availW / (float)baseW;
    float sy = (float)availH / (float)baseH;
    out.scale = std::min(1.0f, std::min(sx, sy));
    out.scale = std::clamp(out.scale, 0.01f, 1.0f);

    out.scaledW = std::max(1, (int)std::lround((double)baseW * out.scale));
    out.scaledH = std::max(1, (int)std::lround((double)baseH * out.scale));

    if (out.scale >= 0.999f)
        out.offsetX = 0; // keep stable position at native size to avoid resize jitter/flicker
    else
        out.offsetX = std::max(0, (cw - out.scaledW) / 2);
    out.offsetY = topPad;
    if (out.offsetY + out.scaledH > ch - bottomPad)
        out.offsetY = std::max(0, (ch - bottomPad) - out.scaledH);
}

static void DestroyKeyboardButtons()
{
    for (HWND b : g_keyButtons)
    {
        if (b && IsWindow(b))
            DestroyWindow(b);
    }
    g_keyButtons.clear();
    g_btnByHid.fill(nullptr);
    g_hids.clear();
}

static int KeyboardBottomPx(HWND hWnd)
{
    KeyboardViewMetrics m{};
    ComputeKeyboardViewMetrics(hWnd, m);
    return m.offsetY + m.scaledH;
}

static void LayoutKeyboardButtons(HWND hWnd)
{
    const KeyDef* keys = KeyboardLayout_Data();
    int n = KeyboardLayout_Count();
    if (!keys || n <= 0 || g_keyButtons.empty()) return;

    KeyboardViewMetrics m{};
    ComputeKeyboardViewMetrics(hWnd, m);

    int cnt = std::min(n, (int)g_keyButtons.size());
    HDWP hdwp = BeginDeferWindowPos(std::max(1, cnt));
    bool anyChanged = false;
    for (int i = 0; i < cnt; ++i)
    {
        HWND b = g_keyButtons[(size_t)i];
        if (!b || !IsWindow(b)) continue;

        const auto& k = keys[i];
        int baseX = S(hWnd, KEYBOARD_MARGIN_X + k.x);
        int baseY = S(hWnd, KEYBOARD_MARGIN_Y + k.row * KEYBOARD_ROW_PITCH_Y);
        int baseW = S(hWnd, std::max(KEYBOARD_KEY_MIN_DIM, k.w));
        int baseH = S(hWnd, std::max(KEYBOARD_KEY_MIN_DIM, k.h));

        int px = m.offsetX + (int)std::lround((double)baseX * m.scale);
        int py = m.offsetY + (int)std::lround((double)baseY * m.scale);
        int pw = std::max(8, (int)std::lround((double)baseW * m.scale));
        int ph = std::max(8, (int)std::lround((double)baseH * m.scale));

        RECT cur{};
        GetWindowRect(b, &cur);
        MapWindowPoints(nullptr, hWnd, (LPPOINT)&cur, 2);
        int cw = cur.right - cur.left;
        int ch = cur.bottom - cur.top;
        if (cur.left == px && cur.top == py && cw == pw && ch == ph)
            continue;

        anyChanged = true;
        if (hdwp)
        {
            hdwp = DeferWindowPos(
                hdwp, b, nullptr, px, py, pw, ph,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOCOPYBITS | SWP_NOREDRAW);
        }
        else
        {
            SetWindowPos(
                b, nullptr, px, py, pw, ph,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOCOPYBITS | SWP_NOREDRAW);
        }
    }

    if (hdwp)
        EndDeferWindowPos(hdwp);

    if (anyChanged)
    {
        RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
}

static void RebuildKeyboardButtons(HWND hWnd)
{
    uint16_t keepSelected = g_selectedHid;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);

    DestroyKeyboardButtons();

    const KeyDef* keys = KeyboardLayout_Data();
    int n = KeyboardLayout_Count();
    for (int i = 0; i < n; ++i)
    {
        const auto& k = keys[i];

        HWND b = CreateWindowW(L"BUTTON", k.label,
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 10, 10,
            hWnd, nullptr, hInst, nullptr);

        SetWindowLongPtrW(b, GWLP_USERDATA, (LONG_PTR)k.hid);
        SetWindowSubclass(b, KeyBtnSubclassProc, 1, (DWORD_PTR)k.hid);
        g_keyButtons.push_back(b);

        if (k.hid != 0 && k.hid < 256)
        {
            g_btnByHid[k.hid] = b;
            g_hids.push_back((uint16_t)k.hid);
        }
    }

    BackendUI_SetTrackedHids(g_hids.data(), (int)g_hids.size());
    LayoutKeyboardButtons(hWnd);

    if (keepSelected >= 256 || g_btnByHid[keepSelected] == nullptr)
        keepSelected = 0;
    SetSelectedHid(keepSelected);
}

static HWND GetBtnForHid(uint16_t hid)
{
    if (hid == 0 || hid >= 256) return nullptr;
    return g_btnByHid[hid];
}

static void InvalidateKeyByHid(uint16_t hid)
{
    if (hid == 0 || hid >= 256) return;
    if (HWND b = g_btnByHid[hid])
        InvalidateRect(b, nullptr, FALSE);
}

static void SetSelectedHid(uint16_t hid)
{
    uint16_t old = g_selectedHid;
    g_selectedHid = hid;

    KeySettingsPanel_SetSelectedHid(hid);

    // NEW: tell renderer which HID is currently being edited/selected (for wow-spin)
    KeyboardRender_NotifySelectedHid(hid);

    if (HWND bOld = GetBtnForHid(old))
        InvalidateRect(bOld, nullptr, FALSE);
    if (HWND bNew = GetBtnForHid(g_selectedHid))
        InvalidateRect(bNew, nullptr, FALSE);
}

static void ShowSubPage(int idx)
{
    g_activeSubTab = idx;

    if (idx == 2)
    {
        EnsureFreeComboPageCreated(g_hSubTab);
        if (g_hSubTab)
        {
            HWND hMain = GetParent(g_hSubTab);
            if (hMain) ResizeSubUi(hMain);
        }
    }

    // NEW: clear selection when leaving Configuration tab
    if (idx != 1 && g_selectedHid != 0)
        SetSelectedHid(0);

    if (g_hPageRemap)  ShowWindow(g_hPageRemap, idx == 0 ? SW_SHOW : SW_HIDE);
    if (g_hPageConfig) ShowWindow(g_hPageConfig, idx == 1 ? SW_SHOW : SW_HIDE);
    if (g_hPageFreeCombo) ShowWindow(g_hPageFreeCombo, idx == 2 ? SW_SHOW : SW_HIDE);
    if (g_hPageTester) ShowWindow(g_hPageTester, idx == 3 ? SW_SHOW : SW_HIDE);
    if (g_hPageGlobal) ShowWindow(g_hPageGlobal, idx == 4 ? SW_SHOW : SW_HIDE);

    if (g_hSubTab) InvalidateRect(g_hSubTab, nullptr, FALSE);
}

static void ResizeSubUi(HWND hWnd)
{
    if (!g_hSubTab) return;

    RECT rc{};
    GetClientRect(hWnd, &rc);

    int kbBottom = KeyboardBottomPx(hWnd);
    int x = S(hWnd, 12);
    int y = kbBottom + S(hWnd, 12);

    int w = (rc.right - rc.left) - S(hWnd, 24);
    int h = (rc.bottom - rc.top) - y - S(hWnd, 12);
    if (w < 10) w = 10;
    if (h < 10) h = 10;

    // F1: Bouton toggle remap — positionné à droite, au-dessus des onglets
    const int btnW = S(hWnd, 130);
    const int btnH = S(hWnd, 26);
    const int btnX = (rc.right - rc.left) - btnW - S(hWnd, 12);
    const int btnY = y + S(hWnd, 2);
    if (g_hBtnRemapToggle)
        SetWindowPos(g_hBtnRemapToggle, HWND_TOP, btnX, btnY, btnW, btnH, SWP_NOZORDER);

    // Réduire légèrement la largeur du tabcontrol pour ne pas déborder sous le bouton
    w = btnX - x - S(hWnd, 8);
    if (w < 10) w = 10;
    SetWindowPos(g_hSubTab, nullptr, x, y, w, h, SWP_NOZORDER);

    RECT tabRc{};
    GetClientRect(g_hSubTab, &tabRc);
    TabCtrl_AdjustRect(g_hSubTab, FALSE, &tabRc);

    int pw = tabRc.right - tabRc.left;
    int ph = tabRc.bottom - tabRc.top;

    if (g_hPageRemap)
        SetWindowPos(g_hPageRemap, nullptr, tabRc.left, tabRc.top, pw, ph, SWP_NOZORDER);

    if (g_hPageConfig)
        SetWindowPos(g_hPageConfig, nullptr, tabRc.left, tabRc.top, pw, ph, SWP_NOZORDER);

    if (g_hPageTester)
        SetWindowPos(g_hPageTester, nullptr, tabRc.left, tabRc.top, pw, ph, SWP_NOZORDER);

    if (g_hPageGlobal)
        SetWindowPos(g_hPageGlobal, nullptr, tabRc.left, tabRc.top, pw, ph, SWP_NOZORDER);

    if (g_hPageFreeCombo)
        SetWindowPos(g_hPageFreeCombo, nullptr, tabRc.left, tabRc.top, pw, ph, SWP_NOZORDER);
}

static void EnsureFreeComboPageCreated(HWND hTabParent)
{
    if (!hTabParent || g_hPageFreeCombo) return;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hTabParent, GWLP_HINSTANCE);
    g_hPageFreeCombo = FreeComboUI::CreatePage(hTabParent, hInst);
    if (g_hPageFreeCombo)
        ShowWindow(g_hPageFreeCombo, SW_HIDE);
}

// ----- Right-click unbind on key button + drag bound icon (subclass) -----

static constexpr UINT_PTR KEYDRAG_TIMER_ID = 9101;
static constexpr UINT_PTR KEYSWAP_TIMER_ID = 9102;
static constexpr UINT_PTR KEYDELETE_TIMER_ID = 9103;

// -----------------------------------------------------------------------------
// Helpers for action type
// -----------------------------------------------------------------------------
static bool IsButtonAction(BindAction a)
{
    switch (a)
    {
    case BindAction::Btn_A: case BindAction::Btn_B: case BindAction::Btn_X: case BindAction::Btn_Y:
    case BindAction::Btn_LB: case BindAction::Btn_RB:
    case BindAction::Btn_Back: case BindAction::Btn_Start: case BindAction::Btn_Guide:
    case BindAction::Btn_LS: case BindAction::Btn_RS:
    case BindAction::Btn_DU: case BindAction::Btn_DD: case BindAction::Btn_DL: case BindAction::Btn_DR:
        return true;
    default:
        return false;
    }
}

static bool ActionToGameButton(BindAction a, GameButton& out)
{
    switch (a)
    {
    case BindAction::Btn_A: out = GameButton::A; return true;
    case BindAction::Btn_B: out = GameButton::B; return true;
    case BindAction::Btn_X: out = GameButton::X; return true;
    case BindAction::Btn_Y: out = GameButton::Y; return true;
    case BindAction::Btn_LB: out = GameButton::LB; return true;
    case BindAction::Btn_RB: out = GameButton::RB; return true;
    case BindAction::Btn_Back:  out = GameButton::Back;  return true;
    case BindAction::Btn_Start: out = GameButton::Start; return true;
    case BindAction::Btn_Guide: out = GameButton::Guide; return true;
    case BindAction::Btn_LS: out = GameButton::LS; return true;
    case BindAction::Btn_RS: out = GameButton::RS; return true;
    case BindAction::Btn_DU: out = GameButton::DpadUp; return true;
    case BindAction::Btn_DD: out = GameButton::DpadDown; return true;
    case BindAction::Btn_DL: out = GameButton::DpadLeft; return true;
    case BindAction::Btn_DR: out = GameButton::DpadRight; return true;
    default: return false;
    }
}

static int FindIconIdxForAction(BindAction a);

struct DisplayedKeyIconEntry
{
    BindAction action{};
    int padIndex = 0;
    int iconIdx = -1;
};

static int CollectDisplayedActionsByHid(uint16_t hid, DisplayedKeyIconEntry out[4])
{
    if (out)
    {
        for (int i = 0; i < 4; ++i) out[i] = DisplayedKeyIconEntry{};
    }
    if (!hid) return 0;

    int pads = std::clamp(Backend_GetVirtualGamepadCount(), 1, 4);
    int count = 0;
    for (int pad = 0; pad < pads; ++pad)
    {
        if (count >= 4) break;
        BindAction act{};
        if (BindingActions_TryGetByHidForPad(pad, hid, act))
        {
            int iconIdx = FindIconIdxForAction(act);
            if (iconIdx < 0) continue;
            if (out)
            {
                out[count].action = act;
                out[count].padIndex = std::clamp(pad, 0, 3);
                out[count].iconIdx = iconIdx;
            }
            ++count;
        }
    }
    return count;
}

static bool FindDisplayedActionByHid(uint16_t hid, BindAction& outAction, int& outPadIndex)
{
    outPadIndex = 0;
    DisplayedKeyIconEntry e[4]{};
    int n = CollectDisplayedActionsByHid(hid, e);
    if (n <= 0) return false;
    outAction = e[0].action;
    outPadIndex = e[0].padIndex;
    return true;
}

static int BuildMiniIconRectsForButton(HWND hBtn, int iconCount, RECT outRects[4])
{
    if (!hBtn || !outRects) return 0;
    iconCount = std::clamp(iconCount, 0, 4);
    if (iconCount <= 0) return 0;

    RECT rc{};
    GetClientRect(hBtn, &rc);
    RECT iconArea = rc;
    InflateRect(&iconArea, -1, -1);

    int iw = iconArea.right - iconArea.left;
    int ih = iconArea.bottom - iconArea.top;
    if (iw <= 10 || ih <= 10) return 0;

    int baseSize = WinUtil_ScalePx(hBtn, (int)Settings_GetBoundKeyIconSizePx());
    baseSize = std::clamp(baseSize, 8, std::min(iw, ih));
    int gap = std::clamp(WinUtil_ScalePx(hBtn, 2), 1, 5);

    for (int i = 0; i < 4; ++i) outRects[i] = RECT{};

    if (iconCount == 1)
    {
        int size = std::clamp(baseSize, 8, std::min(iw, ih));
        int x = (iconArea.left + iconArea.right - size) / 2;
        int y = (iconArea.top + iconArea.bottom - size) / 2;
        outRects[0] = RECT{ x, y, x + size, y + size };
        return 1;
    }

    if (iconCount == 2)
    {
        int cellW = std::max(8, (iw - gap) / 2);
        int size = std::clamp(baseSize, 8, std::min(cellW, ih));
        int y = iconArea.top + (ih - size) / 2;
        int x0 = iconArea.left + (cellW - size) / 2;
        int x1 = iconArea.left + cellW + gap + (cellW - size) / 2;
        outRects[0] = RECT{ x0, y, x0 + size, y + size };
        outRects[1] = RECT{ x1, y, x1 + size, y + size };
        return 2;
    }

    if (iconCount == 3)
    {
        int rowH = std::max(8, (ih - gap) / 2);
        int topCellW = std::max(8, (iw - gap) / 2);
        int size = std::clamp(baseSize, 8, std::min(topCellW, rowH));
        int topY = iconArea.top + (rowH - size) / 2;
        int x0 = iconArea.left + (topCellW - size) / 2;
        int x1 = iconArea.left + topCellW + gap + (topCellW - size) / 2;
        int botY = iconArea.top + rowH + gap + (rowH - size) / 2;
        int x2 = iconArea.left + (iw - size) / 2;
        outRects[0] = RECT{ x0, topY, x0 + size, topY + size };
        outRects[1] = RECT{ x1, topY, x1 + size, topY + size };
        outRects[2] = RECT{ x2, botY, x2 + size, botY + size };
        return 3;
    }

    int cellW = std::max(8, (iw - gap) / 2);
    int cellH = std::max(8, (ih - gap) / 2);
    int size = std::clamp(baseSize, 8, std::min(cellW, cellH));
    for (int i = 0; i < 4; ++i)
    {
        int col = i % 2;
        int row = i / 2;
        int cellX = iconArea.left + col * (cellW + gap);
        int cellY = iconArea.top + row * (cellH + gap);
        int x = cellX + (cellW - size) / 2;
        int y = cellY + (cellH - size) / 2;
        outRects[i] = RECT{ x, y, x + size, y + size };
    }
    return 4;
}

static bool HitTestDisplayedIconOnButton(HWND hBtn, uint16_t hid, POINT ptClient, DisplayedKeyIconEntry& outEntry, RECT& outRect)
{
    outEntry = DisplayedKeyIconEntry{};
    outRect = RECT{};

    DisplayedKeyIconEntry entries[4]{};
    int count = CollectDisplayedActionsByHid(hid, entries);
    if (count <= 0) return false;

    RECT rects[4]{};
    int rectCount = BuildMiniIconRectsForButton(hBtn, count, rects);
    if (rectCount <= 0) return false;

    for (int i = 0; i < rectCount && i < count; ++i)
    {
        if (PtInRect(&rects[i], ptClient))
        {
            outEntry = entries[i];
            outRect = rects[i];
            return true;
        }
    }

    return false;
}

static int GetRemapStyleVariantForPad(int padIndex)
{
    int totalPads = std::clamp(Backend_GetVirtualGamepadCount(), 1, 4);
    padIndex = std::clamp(padIndex, 0, 3);
    if (totalPads <= 1) return 0;
    return std::clamp(Bindings_GetPadStyleVariant(padIndex), 1, 4);
}

// -----------------------------------------------------------------------------
// Swap "fly back" animation (dst icon flies to src)
// -----------------------------------------------------------------------------
struct SwapFlyState
{
    bool running = false;

    HWND hPage = nullptr; // page main window for timer

    uint16_t srcHid = 0;
    uint16_t dstHid = 0;

    BindAction pendingAct{}; // the action that should end up on src
    int pendingPadIndex = 0;
    int iconIdx = -1;

    DWORD startTick = 0;
    DWORD durationMs = 170;

    float x0 = 0, y0 = 0; // start top-left (screen)
    float x1 = 0, y1 = 0; // end top-left (screen)

    // ghost
    HWND hGhost = nullptr;
    int  size = 0;

    HDC     memDC = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    void* bits = nullptr;
};

static SwapFlyState g_swapfly;

static void SwapFly_FreeSurface()
{
    if (g_swapfly.memDC)
    {
        if (g_swapfly.oldBmp) SelectObject(g_swapfly.memDC, g_swapfly.oldBmp);
        g_swapfly.oldBmp = nullptr;
    }
    if (g_swapfly.bmp)
    {
        DeleteObject(g_swapfly.bmp);
        g_swapfly.bmp = nullptr;
    }
    if (g_swapfly.memDC)
    {
        DeleteDC(g_swapfly.memDC);
        g_swapfly.memDC = nullptr;
    }
    g_swapfly.bits = nullptr;
}

static void EnsureGhostClassRegistered(HINSTANCE hInst)
{
    static bool reg = false;
    if (reg) return;

    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = hInst;
    wc.lpszClassName = L"KeyboardBindGhostWindow";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    reg = true;
}

static void SwapFly_EnsureGhostWindow(HINSTANCE hInst, HWND hOwnerTop)
{
    if (g_swapfly.hGhost) return;

    EnsureGhostClassRegistered(hInst);

    g_swapfly.hGhost = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        L"KeyboardBindGhostWindow",
        L"",
        WS_POPUP,
        0, 0, 1, 1,
        hOwnerTop, nullptr, hInst, nullptr);

    if (g_swapfly.hGhost)
        ShowWindow(g_swapfly.hGhost, SW_HIDE);
}

static bool SwapFly_EnsureSurface(HWND hwndForDpi)
{
    if (!g_swapfly.hGhost) return false;

    int want = (int)Settings_GetBoundKeyIconSizePx();
    int size = WinUtil_ScalePx(hwndForDpi, want);
    size = std::clamp(size, 12, 128);

    if (g_swapfly.size == size && g_swapfly.memDC && g_swapfly.bmp && g_swapfly.bits)
        return true;

    g_swapfly.size = size;
    SwapFly_FreeSurface();

    HDC screen = GetDC(nullptr);
    g_swapfly.memDC = CreateCompatibleDC(screen);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = size;
    bi.bmiHeader.biHeight = -size;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    g_swapfly.bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &g_swapfly.bits, nullptr, 0);
    ReleaseDC(nullptr, screen);

    if (!g_swapfly.memDC || !g_swapfly.bmp || !g_swapfly.bits)
    {
        SwapFly_FreeSurface();
        return false;
    }

    g_swapfly.oldBmp = SelectObject(g_swapfly.memDC, g_swapfly.bmp);
    return true;
}

static int FindIconIdxForAction(BindAction a)
{
    int n = RemapIcons_Count();
    for (int i = 0; i < n; ++i)
        if (RemapIcons_Get(i).action == a) return i;
    return -1;
}

static void SwapFly_RenderIcon()
{
    if (!g_swapfly.memDC || !g_swapfly.bits) return;
    int sz = g_swapfly.size;
    if (sz <= 0) return;

    std::memset(g_swapfly.bits, 0, (size_t)sz * (size_t)sz * 4);
    RECT rc{ 0,0,sz,sz };
    int styleVariant = GetRemapStyleVariantForPad(g_swapfly.pendingPadIndex);
    RemapIcons_DrawGlyphAA(g_swapfly.memDC, rc, g_swapfly.iconIdx, true, 0.075f, styleVariant);
}

static void SwapFly_UpdateLayered(int x, int y, BYTE alpha)
{
    if (!g_swapfly.hGhost || !g_swapfly.memDC) return;

    HDC screen = GetDC(nullptr);

    POINT ptPos{ x, y };
    SIZE  sz{ g_swapfly.size, g_swapfly.size };
    POINT ptSrc{ 0, 0 };

    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = alpha;
    bf.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(g_swapfly.hGhost, screen, &ptPos, &sz, g_swapfly.memDC, &ptSrc, 0, &bf, ULW_ALPHA);
    ReleaseDC(nullptr, screen);

    ShowWindow(g_swapfly.hGhost, SW_SHOWNOACTIVATE);
}

static float EaseOutCubic01(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

static void SwapFly_Stop(bool commit)
{
    if (!g_swapfly.running) return;

    KillTimer(g_swapfly.hPage, KEYSWAP_TIMER_ID);

    if (g_swapfly.hGhost)
        ShowWindow(g_swapfly.hGhost, SW_HIDE);

    g_swapfly.running = false;

    if (commit)
    {
        if (g_swapfly.srcHid != 0)
        {
            BindingActions_ApplyForPad(g_swapfly.pendingPadIndex, g_swapfly.pendingAct, g_swapfly.srcHid);
            Profile_SaveIni(AppPaths_BindingsIni().c_str());
        }

        InvalidateKeyByHid(g_swapfly.srcHid);
        InvalidateKeyByHid(g_swapfly.dstHid);
    }

    g_swapfly.srcHid = 0;
    g_swapfly.dstHid = 0;
    g_swapfly.pendingAct = {};
    g_swapfly.pendingPadIndex = 0;
    g_swapfly.iconIdx = -1;
    g_swapfly.hPage = nullptr;
    g_swapfly.startTick = 0;
}

static void SwapFly_Tick()
{
    if (!g_swapfly.running) return;

    DWORD now = GetTickCount();
    DWORD dt = now - g_swapfly.startTick;

    float t = (g_swapfly.durationMs > 0) ? (float)dt / (float)g_swapfly.durationMs : 1.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    float e = EaseOutCubic01(t);

    float x = g_swapfly.x0 + (g_swapfly.x1 - g_swapfly.x0) * e;
    float y = g_swapfly.y0 + (g_swapfly.y1 - g_swapfly.y0) * e;

    SwapFly_UpdateLayered((int)std::lround(x), (int)std::lround(y), 225);

    if (t >= 1.0f - 1e-4f)
        SwapFly_Stop(true);
}

static bool SwapFly_Start(HWND hPage, uint16_t srcHid, uint16_t dstHid, BindAction pendingAct, int pendingPadIndex)
{
    if (!hPage) return false;
    if (!srcHid || !dstHid) return false;

    HWND hSrcBtn = GetBtnForHid(srcHid);
    HWND hDstBtn = GetBtnForHid(dstHid);
    if (!hSrcBtn || !hDstBtn) return false;

    int iconIdx = FindIconIdxForAction(pendingAct);
    if (iconIdx < 0) return false;

    if (g_swapfly.running)
        SwapFly_Stop(true);

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hPage, GWLP_HINSTANCE);
    HWND ownerTop = GetAncestor(hPage, GA_ROOT);

    SwapFly_EnsureGhostWindow(hInst, ownerTop);
    if (!SwapFly_EnsureSurface(hPage))
        return false;

    g_swapfly.hPage = hPage;
    g_swapfly.running = true;
    g_swapfly.srcHid = srcHid;
    g_swapfly.dstHid = dstHid;
    g_swapfly.pendingAct = pendingAct;
    g_swapfly.pendingPadIndex = std::clamp(pendingPadIndex, 0, 3);
    g_swapfly.iconIdx = iconIdx;
    g_swapfly.startTick = GetTickCount();
    g_swapfly.durationMs = 170;

    SwapFly_RenderIcon();

    RECT rcSrc{}, rcDst{};
    GetWindowRect(hSrcBtn, &rcSrc);
    GetWindowRect(hDstBtn, &rcDst);

    int cx0 = (rcDst.left + rcDst.right) / 2;
    int cy0 = (rcDst.top + rcDst.bottom) / 2;

    int cx1 = (rcSrc.left + rcSrc.right) / 2;
    int cy1 = (rcSrc.top + rcSrc.bottom) / 2;

    g_swapfly.x0 = (float)(cx0 - g_swapfly.size / 2);
    g_swapfly.y0 = (float)(cy0 - g_swapfly.size / 2);

    g_swapfly.x1 = (float)(cx1 - g_swapfly.size / 2);
    g_swapfly.y1 = (float)(cy1 - g_swapfly.size / 2);

    SwapFly_UpdateLayered((int)std::lround(g_swapfly.x0), (int)std::lround(g_swapfly.y0), 225);
    SetTimer(hPage, KEYSWAP_TIMER_ID, 15, nullptr);

    InvalidateKeyByHid(srcHid);
    return true;
}

// -----------------------------------------------------------------------------
// Delete shrink animation (RMB unbind): icon shrinks to zero on the key
// -----------------------------------------------------------------------------
struct KeyDeleteShrinkState
{
    bool running = false;
    HWND hPage = nullptr;

    HWND hGhost = nullptr;
    int  size = 0;

    HDC     memDC = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    void* bits = nullptr;

    int iconIdx = -1;
    int padIndex = 0;

    float x = 0.0f; // top-left screen
    float y = 0.0f;

    DWORD startTick = 0;
    DWORD durationMs = 140;
};

static KeyDeleteShrinkState g_kdel;

static void KeyDel_FreeSurface()
{
    if (g_kdel.memDC)
    {
        if (g_kdel.oldBmp) SelectObject(g_kdel.memDC, g_kdel.oldBmp);
        g_kdel.oldBmp = nullptr;
    }
    if (g_kdel.bmp)
    {
        DeleteObject(g_kdel.bmp);
        g_kdel.bmp = nullptr;
    }
    if (g_kdel.memDC)
    {
        DeleteDC(g_kdel.memDC);
        g_kdel.memDC = nullptr;
    }
    g_kdel.bits = nullptr;
    g_kdel.size = 0;
}

static void KeyDel_EnsureGhostWindow(HINSTANCE hInst, HWND hOwnerTop)
{
    if (g_kdel.hGhost) return;

    EnsureGhostClassRegistered(hInst);

    g_kdel.hGhost = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        L"KeyboardBindGhostWindow",
        L"",
        WS_POPUP,
        0, 0, 1, 1,
        hOwnerTop, nullptr, hInst, nullptr);

    if (g_kdel.hGhost)
        ShowWindow(g_kdel.hGhost, SW_HIDE);
}

static bool KeyDel_EnsureSurface(HWND hwndForDpi)
{
    if (!g_kdel.hGhost) return false;

    int want = (int)Settings_GetBoundKeyIconSizePx();
    int sz = WinUtil_ScalePx(hwndForDpi, want);
    sz = std::clamp(sz, 12, 128);

    if (g_kdel.size == sz && g_kdel.memDC && g_kdel.bmp && g_kdel.bits)
        return true;

    g_kdel.size = sz;
    KeyDel_FreeSurface();

    HDC screen = GetDC(nullptr);
    g_kdel.memDC = CreateCompatibleDC(screen);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = sz;
    bi.bmiHeader.biHeight = -sz;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    g_kdel.bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &g_kdel.bits, nullptr, 0);
    ReleaseDC(nullptr, screen);

    if (!g_kdel.memDC || !g_kdel.bmp || !g_kdel.bits)
    {
        KeyDel_FreeSurface();
        return false;
    }

    g_kdel.oldBmp = SelectObject(g_kdel.memDC, g_kdel.bmp);
    return true;
}

static void KeyDel_RenderScaled(float scale01)
{
    if (!g_kdel.memDC || !g_kdel.bits) return;
    int sz = g_kdel.size;
    if (sz <= 0) return;

    scale01 = std::clamp(scale01, 0.0f, 1.0f);

    std::memset(g_kdel.bits, 0, (size_t)sz * (size_t)sz * 4);

    int d = (int)std::lround((float)sz * scale01);

    // IMPORTANT: no 1px leftover
    if (d <= 1) return;

    d = std::clamp(d, 2, sz);

    int x = (sz - d) / 2;
    int y = (sz - d) / 2;
    RECT rc{ x, y, x + d, y + d };

    int styleVariant = GetRemapStyleVariantForPad(g_kdel.padIndex);
    RemapIcons_DrawGlyphAA(g_kdel.memDC, rc, g_kdel.iconIdx, true, 0.075f, styleVariant);
}

static void KeyDel_UpdateLayered(BYTE alpha = 225)
{
    if (!g_kdel.hGhost || !g_kdel.memDC) return;

    HDC screen = GetDC(nullptr);

    POINT ptPos{ (int)std::lround(g_kdel.x), (int)std::lround(g_kdel.y) };
    SIZE  sz{ g_kdel.size, g_kdel.size };
    POINT ptSrc{ 0, 0 };

    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = alpha;
    bf.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(g_kdel.hGhost, screen, &ptPos, &sz, g_kdel.memDC, &ptSrc, 0, &bf, ULW_ALPHA);
    ReleaseDC(nullptr, screen);

    ShowWindow(g_kdel.hGhost, SW_SHOWNOACTIVATE);
}

static void KeyDel_Stop()
{
    if (!g_kdel.running) return;

    if (g_kdel.hPage)
        KillTimer(g_kdel.hPage, KEYDELETE_TIMER_ID);

    if (g_kdel.hGhost)
        ShowWindow(g_kdel.hGhost, SW_HIDE);

    g_kdel.running = false;
    g_kdel.hPage = nullptr;
    g_kdel.iconIdx = -1;
    g_kdel.padIndex = 0;
    g_kdel.startTick = 0;
}

static bool KeyDel_Start(HWND hPage, HWND hBtn, int iconIdx, int padIndex)
{
    if (!hPage || !hBtn) return false;
    if (iconIdx < 0) return false;

    if (g_kdel.running)
        KeyDel_Stop();

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hPage, GWLP_HINSTANCE);
    HWND ownerTop = GetAncestor(hPage, GA_ROOT);

    KeyDel_EnsureGhostWindow(hInst, ownerTop);
    if (!KeyDel_EnsureSurface(hBtn))
        return false;

    RECT rcBtn{};
    GetWindowRect(hBtn, &rcBtn);

    int cx = (rcBtn.left + rcBtn.right) / 2;
    int cy = (rcBtn.top + rcBtn.bottom) / 2;

    g_kdel.hPage = hPage;
    g_kdel.running = true;
    g_kdel.iconIdx = iconIdx;
    g_kdel.padIndex = std::clamp(padIndex, 0, 3);

    g_kdel.x = (float)(cx - g_kdel.size / 2);
    g_kdel.y = (float)(cy - g_kdel.size / 2);

    g_kdel.startTick = GetTickCount();
    g_kdel.durationMs = 140;

    KeyDel_RenderScaled(1.0f);
    KeyDel_UpdateLayered(225);

    SetTimer(hPage, KEYDELETE_TIMER_ID, 15, nullptr);
    return true;
}

static void KeyDel_Tick()
{
    if (!g_kdel.running) return;

    DWORD now = GetTickCount();
    DWORD dt = now - g_kdel.startTick;

    float t = (g_kdel.durationMs > 0) ? (float)dt / (float)g_kdel.durationMs : 1.0f;
    t = std::clamp(t, 0.0f, 1.0f);

    float e = EaseOutCubic01(t);
    float scale = 1.0f - e;

    if (t >= 1.0f - 1e-4f)
        scale = 0.0f;

    KeyDel_RenderScaled(scale);
    KeyDel_UpdateLayered(225);

    if (t >= 1.0f - 1e-4f)
        KeyDel_Stop();
}

// -----------------------------------------------------------------------------
// Dragging state (existing)
// -----------------------------------------------------------------------------
struct KeyIconDragState
{
    bool dragging = false;
    bool growing = false;

    uint16_t srcHid = 0;
    uint16_t hoverHid = 0;
    int srcPadIndex = 0;

    BindAction action{};
    int iconIdx = -1;

    HWND hPage = nullptr;

    HWND hGhost = nullptr;
    int ghostW = 0;
    int ghostH = 0;

    HDC     memDC = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    void* bits = nullptr;

    float gx = 0.0f, gy = 0.0f;
    float tx = 0.0f, ty = 0.0f;
    DWORD lastTick = 0;
    DWORD growStartTick = 0;
    DWORD growDurationMs = 0;
    float growFromScale = 1.0f;

    // NEW: shrink-out animation when dropping to empty (unbind)
    bool shrinking = false;
    DWORD shrinkStartTick = 0;
    DWORD shrinkDurationMs = 140;
};

static KeyIconDragState g_kdrag;

static void KeyDrag_FreeSurface()
{
    if (g_kdrag.memDC)
    {
        if (g_kdrag.oldBmp) SelectObject(g_kdrag.memDC, g_kdrag.oldBmp);
        g_kdrag.oldBmp = nullptr;
    }
    if (g_kdrag.bmp)
    {
        DeleteObject(g_kdrag.bmp);
        g_kdrag.bmp = nullptr;
    }
    if (g_kdrag.memDC)
    {
        DeleteDC(g_kdrag.memDC);
        g_kdrag.memDC = nullptr;
    }
    g_kdrag.bits = nullptr;
}

static bool KeyDrag_EnsureSurface()
{
    if (!g_kdrag.hGhost) return false;
    if (g_kdrag.ghostW <= 0 || g_kdrag.ghostH <= 0) return false;

    KeyDrag_FreeSurface();

    HDC screen = GetDC(nullptr);
    g_kdrag.memDC = CreateCompatibleDC(screen);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = g_kdrag.ghostW;
    bi.bmiHeader.biHeight = -g_kdrag.ghostH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    g_kdrag.bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &g_kdrag.bits, nullptr, 0);
    ReleaseDC(nullptr, screen);

    if (!g_kdrag.memDC || !g_kdrag.bmp || !g_kdrag.bits)
    {
        KeyDrag_FreeSurface();
        return false;
    }

    g_kdrag.oldBmp = SelectObject(g_kdrag.memDC, g_kdrag.bmp);
    return true;
}

static void KeyDrag_EnsureGhostWindow(HINSTANCE hInst, HWND hOwnerTop)
{
    if (g_kdrag.hGhost) return;

    EnsureGhostClassRegistered(hInst);

    g_kdrag.hGhost = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        L"KeyboardBindGhostWindow",
        L"",
        WS_POPUP,
        0, 0, 1, 1,
        hOwnerTop, nullptr, hInst, nullptr);

    if (g_kdrag.hGhost)
        ShowWindow(g_kdrag.hGhost, SW_HIDE);
}

static void KeyDrag_UpdateLayered(int x, int y)
{
    if (!g_kdrag.hGhost || !g_kdrag.memDC) return;

    BYTE alpha = 255;

    HDC screen = GetDC(nullptr);

    POINT ptPos{ x, y };
    SIZE  sz{ g_kdrag.ghostW, g_kdrag.ghostH };
    POINT ptSrc{ 0, 0 };

    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = alpha;
    bf.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(g_kdrag.hGhost, screen, &ptPos, &sz, g_kdrag.memDC, &ptSrc, 0, &bf, ULW_ALPHA);
    ReleaseDC(nullptr, screen);

    ShowWindow(g_kdrag.hGhost, SW_SHOWNOACTIVATE);
}

static void KeyDrag_Hide()
{
    if (g_kdrag.hGhost)
        ShowWindow(g_kdrag.hGhost, SW_HIDE);
}

static void KeyDrag_Stop()
{
    if (!g_kdrag.dragging && !g_kdrag.shrinking) return;

    uint16_t oldSrc = g_kdrag.srcHid;

    g_kdrag.dragging = false;
    g_kdrag.growing = false;
    g_kdrag.shrinking = false;

    if (g_kdrag.hPage)
    {
        KillTimer(g_kdrag.hPage, KEYDRAG_TIMER_ID);
        if (GetCapture() == g_kdrag.hPage)
            ReleaseCapture();
    }

    KeyboardUI_SetDragHoverHid(0);
    KeyDrag_Hide();

    g_kdrag.srcHid = 0;
    g_kdrag.hoverHid = 0;
    g_kdrag.srcPadIndex = 0;
    g_kdrag.action = {};
    g_kdrag.iconIdx = -1;
    g_kdrag.lastTick = 0;
    g_kdrag.growStartTick = 0;
    g_kdrag.growDurationMs = 0;
    g_kdrag.growFromScale = 1.0f;
    g_kdrag.hPage = nullptr;

    InvalidateKeyByHid(oldSrc);
}

static bool KeyDrag_TryPickTargetKey(HWND hPage, POINT ptScreen, uint16_t& outHid, RECT& outRc)
{
    outHid = 0;
    outRc = RECT{};

    HWND w = WindowFromPoint(ptScreen);
    if (w)
    {
        for (HWND cur = w; cur; cur = GetParent(cur))
        {
            if (GetParent(cur) == hPage)
            {
                uint16_t hid = (uint16_t)GetWindowLongPtrW(cur, GWLP_USERDATA);
                if (hid != 0 && KeyboardUI_HasHid(hid))
                {
                    GetWindowRect(cur, &outRc);
                    outHid = hid;
                    return true;
                }
                break;
            }
            if (cur == hPage) break;
        }
    }

    int thr = S(hPage, 42);
    int thr2 = thr * thr;

    int best = INT_MAX;
    uint16_t bestHid = 0;
    RECT bestRc{};

    for (uint16_t hid : g_hids)
    {
        HWND btn = g_btnByHid[hid];
        if (!btn || !IsWindowVisible(btn)) continue;

        RECT rc{};
        GetWindowRect(btn, &rc);

        int dx = 0;
        if (ptScreen.x < rc.left) dx = rc.left - ptScreen.x;
        else if (ptScreen.x > rc.right) dx = ptScreen.x - rc.right;

        int dy = 0;
        if (ptScreen.y < rc.top) dy = rc.top - ptScreen.y;
        else if (ptScreen.y > rc.bottom) dy = ptScreen.y - rc.bottom;

        int d2 = dx * dx + dy * dy;
        if (d2 < best)
        {
            best = d2;
            bestHid = hid;
            bestRc = rc;
        }
    }

    if (bestHid != 0 && best <= thr2)
    {
        outHid = bestHid;
        outRc = bestRc;
        return true;
    }

    return false;
}

static void KeyDrag_RenderGlyphToSurfaceScaled(float scale01)
{
    if (!g_kdrag.memDC || !g_kdrag.bits) return;

    int sz = g_kdrag.ghostW;
    if (sz <= 0 || g_kdrag.ghostH != sz) return;

    scale01 = std::clamp(scale01, 0.0f, 1.0f);

    std::memset(g_kdrag.bits, 0, (size_t)sz * (size_t)sz * 4);

    int d = (int)std::lround((float)sz * scale01);

    // IMPORTANT: avoid 1px artifact
    if (d <= 1) return;

    d = std::clamp(d, 2, sz);

    int x = (sz - d) / 2;
    int y = (sz - d) / 2;
    RECT rc{ x, y, x + d, y + d };

    int styleVariant = GetRemapStyleVariantForPad(g_kdrag.srcPadIndex);
    RemapIcons_DrawGlyphAA(g_kdrag.memDC, rc, g_kdrag.iconIdx, true, 0.135f, styleVariant);
}

static void KeyDrag_BeginShrinkOut()
{
    if (!g_kdrag.hPage) return;
    if (!g_kdrag.hGhost) return;

    g_kdrag.dragging = false;
    g_kdrag.growing = false;
    g_kdrag.shrinking = true;
    g_kdrag.shrinkStartTick = GetTickCount();
    g_kdrag.shrinkDurationMs = 140;

    g_kdrag.hoverHid = 0;
    KeyboardUI_SetDragHoverHid(0);

    // release capture so UI returns to normal
    if (GetCapture() == g_kdrag.hPage)
        ReleaseCapture();

    // keep timer running (reuse same timer id)
    UINT tickMs = std::clamp(Settings_GetUIRefreshMs(), 1u, 50u);
    SetTimer(g_kdrag.hPage, KEYDRAG_TIMER_ID, tickMs, nullptr);
}

static void KeyDrag_ShrinkTick()
{
    if (!g_kdrag.shrinking || !g_kdrag.hPage) return;

    DWORD now = GetTickCount();
    DWORD dt = now - g_kdrag.shrinkStartTick;

    float t = (g_kdrag.shrinkDurationMs > 0) ? (float)dt / (float)g_kdrag.shrinkDurationMs : 1.0f;
    t = std::clamp(t, 0.0f, 1.0f);

    float e = EaseOutCubic01(t);
    float scale = 1.0f - e;

    if (t >= 1.0f - 1e-4f)
        scale = 0.0f;

    KeyDrag_RenderGlyphToSurfaceScaled(scale);
    KeyDrag_UpdateLayered((int)lroundf(g_kdrag.gx), (int)lroundf(g_kdrag.gy));

    if (t >= 1.0f - 1e-4f)
    {
        g_kdrag.shrinking = false;
        KeyDrag_Hide();

        KillTimer(g_kdrag.hPage, KEYDRAG_TIMER_ID);

        // fully reset state
        g_kdrag.srcHid = 0;
        g_kdrag.hoverHid = 0;
        g_kdrag.srcPadIndex = 0;
        g_kdrag.action = {};
        g_kdrag.iconIdx = -1;
        g_kdrag.lastTick = 0;
        g_kdrag.growing = false;
        g_kdrag.growStartTick = 0;
        g_kdrag.growDurationMs = 0;
        g_kdrag.growFromScale = 1.0f;
        g_kdrag.hPage = nullptr;
    }
}

static void KeyDrag_Tick()
{
    if (!g_kdrag.hPage) return;

    if (g_kdrag.shrinking)
    {
        KeyDrag_ShrinkTick();
        return;
    }

    if (!g_kdrag.dragging) return;

    POINT pt{};
    GetCursorPos(&pt);

    uint16_t hid = 0;
    RECT rcKey{};
    KeyDrag_TryPickTargetKey(g_kdrag.hPage, pt, hid, rcKey);

    g_kdrag.hoverHid = hid;
    KeyboardUI_SetDragHoverHid(hid);

    if (hid != 0)
    {
        int cx = (rcKey.left + rcKey.right) / 2;
        int cy = (rcKey.top + rcKey.bottom) / 2;
        g_kdrag.tx = (float)(cx - g_kdrag.ghostW / 2);
        g_kdrag.ty = (float)(cy - g_kdrag.ghostH / 2);
    }
    else
    {
        g_kdrag.tx = (float)(pt.x - g_kdrag.ghostW / 2);
        g_kdrag.ty = (float)(pt.y - g_kdrag.ghostH / 2);
    }

    DWORD now = GetTickCount();
    float dt = 0.016f;
    if (g_kdrag.lastTick != 0)
    {
        dt = (float)(now - g_kdrag.lastTick) / 1000.0f;
        dt = std::clamp(dt, 0.001f, 0.050f);
    }
    g_kdrag.lastTick = now;

    const float lambda = (hid != 0) ? 24.0f : 18.0f;
    float a = 1.0f - expf(-lambda * dt);

    g_kdrag.gx += (g_kdrag.tx - g_kdrag.gx) * a;
    g_kdrag.gy += (g_kdrag.ty - g_kdrag.gy) * a;

    float renderScale = 1.0f;
    if (g_kdrag.growing)
    {
        DWORD growDt = now - g_kdrag.growStartTick;
        float gt = (g_kdrag.growDurationMs > 0) ? (float)growDt / (float)g_kdrag.growDurationMs : 1.0f;
        gt = std::clamp(gt, 0.0f, 1.0f);
        float ge = EaseOutCubic01(gt);
        renderScale = g_kdrag.growFromScale + (1.0f - g_kdrag.growFromScale) * ge;
        if (gt >= 1.0f - 1e-4f)
        {
            g_kdrag.growing = false;
            renderScale = 1.0f;
        }
    }

    KeyDrag_RenderGlyphToSurfaceScaled(renderScale);
    KeyDrag_UpdateLayered((int)lroundf(g_kdrag.gx), (int)lroundf(g_kdrag.gy));
}

static bool HitTestGearMarker(HWND hBtn, POINT ptClient)
{
    if (!hBtn) return false;

    RECT rc{};
    GetClientRect(hBtn, &rc);

    RECT inner = rc;
    InflateRect(&inner, -3, -3);

    // MUST match DrawOverrideGearMarkerAA() geometry in keyboard_render.cpp
    int d = WinUtil_ScalePx(hBtn, 11);
    d = std::clamp(d, 7, 24);

    int pad = WinUtil_ScalePx(hBtn, 0);
    pad = std::clamp(pad, 0, 24);

    int x = inner.right - pad - d;
    int y = inner.top + pad;

    if (x < inner.left + 1) x = inner.left + 1;
    if (y < inner.top + 1) y = inner.top + 1;

    RECT r{ x, y, x + d, y + d };

    return (ptClient.x >= r.left && ptClient.x < r.right &&
        ptClient.y >= r.top && ptClient.y < r.bottom);
}

static bool StartKeyDragFromButton(HWND hBtn, uint16_t hid, POINT ptClient)
{
    if (g_activeSubTab != 0) return false;

    if (g_swapfly.running)
        SwapFly_Stop(true);

    if (!hid) return false;
    DisplayedKeyIconEntry picked{};
    RECT pickedRc{};
    if (!HitTestDisplayedIconOnButton(hBtn, hid, ptClient, picked, pickedRc))
        return false;

    BindAction act = picked.action;
    int srcPadIndex = picked.padIndex;
    int iconIdx = picked.iconIdx;
    if (iconIdx < 0)
        return false;

    HWND hPage = GetParent(hBtn);
    if (!hPage) return false;

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hPage, GWLP_HINSTANCE);
    HWND ownerTop = GetAncestor(hPage, GA_ROOT);

    KeyDrag_Stop();

    g_kdrag.dragging = true;
    g_kdrag.growing = false;
    g_kdrag.shrinking = false;
    g_kdrag.srcHid = hid;
    g_kdrag.hoverHid = 0;
    g_kdrag.srcPadIndex = srcPadIndex;
    g_kdrag.action = act;
    g_kdrag.iconIdx = iconIdx;
    g_kdrag.hPage = hPage;

    g_kdrag.ghostW = WinUtil_ScalePx(hBtn, (int)Settings_GetDragIconSizePx());
    g_kdrag.ghostH = g_kdrag.ghostW;

    KeyDrag_EnsureGhostWindow(hInst, ownerTop);
    if (!KeyDrag_EnsureSurface())
        return false;

    POINT iconCenter{
        (pickedRc.left + pickedRc.right) / 2,
        (pickedRc.top + pickedRc.bottom) / 2 };
    ClientToScreen(hBtn, &iconCenter);

    g_kdrag.gx = (float)(iconCenter.x - g_kdrag.ghostW / 2);
    g_kdrag.gy = (float)(iconCenter.y - g_kdrag.ghostH / 2);
    g_kdrag.tx = g_kdrag.gx;
    g_kdrag.ty = g_kdrag.gy;
    g_kdrag.lastTick = 0;
    int miniW = std::max<int>(1, (int)(pickedRc.right - pickedRc.left));
    g_kdrag.growFromScale = std::clamp((float)miniW / (float)std::max<int>(1, g_kdrag.ghostW), 0.20f, 1.0f);
    g_kdrag.growStartTick = GetTickCount();
    g_kdrag.growDurationMs = 120;
    g_kdrag.growing = (g_kdrag.growFromScale < 0.995f);

    SetFocus(hPage);
    SetCapture(hPage);

    UINT tickMs = std::clamp(Settings_GetUIRefreshMs(), 1u, 50u);
    SetTimer(hPage, KEYDRAG_TIMER_ID, tickMs, nullptr);

    KeyboardUI_SetDragHoverHid(0);
    InvalidateRect(hBtn, nullptr, FALSE);
    UpdateWindow(hBtn);

    KeyDrag_RenderGlyphToSurfaceScaled(g_kdrag.growing ? g_kdrag.growFromScale : 1.0f);
    KeyDrag_UpdateLayered((int)lroundf(g_kdrag.gx), (int)lroundf(g_kdrag.gy));

    return true;
}

static LRESULT CALLBACK KeyBtnSubclassProc(HWND hBtn, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR dwRefData)
{
    uint16_t hid = (uint16_t)dwRefData;

    // Premium UX: hand cursor over the gear marker (override indicator)
    if (msg == WM_SETCURSOR)
    {
        if (g_activeSubTab == 1 && hid != 0 && hid < 256)
        {
            if (KeySettings_GetUseUnique(hid))
            {
                POINT pt{};
                GetCursorPos(&pt);
                ScreenToClient(hBtn, &pt);

                // Show hand only when really over the gear marker
                if (HitTestGearMarker(hBtn, pt))
                {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }
            }
        }
        // else fall through
    }

    if (msg == WM_RBUTTONUP)
    {
        if (hid != 0 && g_activeSubTab == 0)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            DisplayedKeyIconEntry picked{};
            RECT pickedRc{};
            bool pickedSpecific = HitTestDisplayedIconOnButton(hBtn, hid, pt, picked, pickedRc);

            // If key has a bound icon, play shrink animation on top of the key.
            BindAction act{};
            int padIndex = 0;
            int iconIdx = -1;
            if (pickedSpecific)
            {
                act = picked.action;
                padIndex = picked.padIndex;
                iconIdx = picked.iconIdx;
            }
            else if (FindDisplayedActionByHid(hid, act, padIndex))
            {
                iconIdx = FindIconIdxForAction(act);
            }

            if (iconIdx >= 0)
            {
                HWND hPage = GetParent(hBtn);
                KeyDel_Start(hPage, hBtn, iconIdx, padIndex);

                // Logical unbind happens immediately (visual is handled by overlay ghost)
                Bindings_ClearHidForPad(padIndex, hid);
                Profile_SaveIni(AppPaths_BindingsIni().c_str());
                InvalidateRect(hBtn, nullptr, FALSE);
            }
        }
        return 0;
    }

    if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK)
    {
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };

        // Remap tab: allow dragging bound icon as before
        if (StartKeyDragFromButton(hBtn, hid, pt))
            return 0;

        // Configuration tab (premium): if Override is enabled for this key,
        // ANY click on the key reliably triggers wow-spin.
        // This removes the "sometimes works" feeling caused by tiny gear hitbox.
        if (g_activeSubTab == 1 && hid != 0 && hid < 256)
        {
            if (KeySettings_GetUseUnique(hid))
            {
                SetSelectedHid(hid);
                KeyboardRender_OnGearClicked(hid);

                // Ensure keyboard shortcuts go to config page (Ctrl+Z/Ctrl+S etc.)
                if (g_hPageConfig) SetFocus(g_hPageConfig);

                InvalidateRect(hBtn, nullptr, FALSE);
                return 0; // swallow click (we already selected + triggered spin)
            }
        }

        return DefSubclassProc(hBtn, msg, wParam, lParam);
    }

    return DefSubclassProc(hBtn, msg, wParam, lParam);
}

static HPEN PenDropHover()
{
    static HPEN p = CreatePen(PS_SOLID, 3, RGB(60, 200, 120));
    return p;
}

static void DrawDropHoverOutline(const DRAWITEMSTRUCT* dis)
{
    RECT rc = dis->rcItem;
    InflateRect(&rc, -2, -2);

    HDC hdc = dis->hDC;
    HGDIOBJ oldPen = SelectObject(hdc, PenDropHover());
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));

    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
}

static LRESULT CALLBACK PageMainProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == TabDark::MsgSelChanged())
    {
        HWND hTab = (HWND)wParam;
        if (hTab && hTab == g_hSubTab)
        {
            int idx = TabCtrl_GetCurSel(g_hSubTab);
            if (idx < 0) idx = 0;
            ShowSubPage(idx);

            if (g_activeSubTab == 3 && g_hPageTester)
                InvalidateRect(g_hPageTester, nullptr, FALSE);
        }
        return 0;
    }

    switch (msg)
    {
    case WM_ERASEBKGND: return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc{};
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, UiTheme::Brush_PanelBg());
        // Draw mouse view with double-buffer to prevent flicker
        if (g_mouseViewRect.right > g_mouseViewRect.left)
        {
            int bw = g_mouseViewRect.right - g_mouseViewRect.left;
            int bh = g_mouseViewRect.bottom - g_mouseViewRect.top;
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, bw, bh);
            HGDIOBJ old = SelectObject(memDC, bmp);
            RECT bufRc{ 0, 0, bw, bh };
            FillRect(memDC, &bufRc, UiTheme::Brush_PanelBg());
            SetViewportOrgEx(memDC, -g_mouseViewRect.left, -g_mouseViewRect.top, nullptr);
            DrawMouseView(hWnd, memDC);
            SetViewportOrgEx(memDC, 0, 0, nullptr);
            BitBlt(hdc, g_mouseViewRect.left, g_mouseViewRect.top, bw, bh, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, old);
            DeleteObject(bmp);
            DeleteDC(memDC);
        }
        else
        {
            DrawMouseView(hWnd, hdc);
        }
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_CREATE:
    {
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);

        SetTimer(hWnd, 9104, 16, nullptr); // mouse view 60fps timer
        // Hotkeys for mouse overlay calibration (works even when focus is on child controls)
        RegisterHotKey(hWnd, 0xC08, MOD_NOREPEAT, VK_F8);  // toggle Synapse vector mode (when no PNG)
        //Ajout-->
        RegisterHotKey(hWnd, HOTKEY_EMERGENCY_STOP, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_BACK);
        RegisterHotKey(hWnd, HOTKEY_REMAP_TOGGLE, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'R');  // F1
        //Ajout<--
        Profile_LoadIni(AppPaths_BindingsIni().c_str());

        // F1: Bouton toggle remap (visible en permanence au-dessus des onglets)
        g_hBtnRemapToggle = CreateWindowExW(
            0, L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 10, 10,  // position définie dans ResizeSubUi
            hWnd, (HMENU)(UINT_PTR)0xF101, hInst, nullptr);

        RebuildKeyboardButtons(hWnd);

        g_hSubTab = CreateWindowW(WC_TABCONTROLW, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
            0, 0, 100, 100,
            hWnd, (HMENU)9001, hInst, nullptr);

        UiTheme::ApplyToControl(g_hSubTab);
        TabDark::Apply(g_hSubTab);

        TCITEMW tie{};
        tie.mask = TCIF_TEXT;

        tie.pszText = (LPWSTR)L"Remap";
        TabCtrl_InsertItem(g_hSubTab, 0, &tie);

        tie.pszText = (LPWSTR)L"Configuration";
        TabCtrl_InsertItem(g_hSubTab, 1, &tie);

        tie.pszText = (LPWSTR)L"Macro Custom";
        TabCtrl_InsertItem(g_hSubTab, 2, &tie);

        tie.pszText = (LPWSTR)L"Gamepad Tester";
        TabCtrl_InsertItem(g_hSubTab, 3, &tie);

        tie.pszText = (LPWSTR)L"Global settings";
        TabCtrl_InsertItem(g_hSubTab, 4, &tie);

        g_hPageRemap = RemapPanel_Create(g_hSubTab, hInst, hWnd);

        static bool cfgReg = false;
        if (!cfgReg)
        {
            WNDCLASSW wc{};
            wc.lpfnWndProc = KeyboardSubpages_ConfigPageProc;
            wc.hInstance = hInst;
            wc.lpszClassName = L"KeyboardSubConfigPage";
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            RegisterClassW(&wc);
            cfgReg = true;
        }
        g_hPageConfig = CreateWindowW(L"KeyboardSubConfigPage", L"",
            WS_CHILD | WS_CLIPCHILDREN, 0, 0, 100, 100, g_hSubTab, nullptr, hInst, nullptr);

        static bool tstReg = false;
        if (!tstReg)
        {
            WNDCLASSW wc{};
            wc.lpfnWndProc = KeyboardSubpages_TesterPageProc;
            wc.hInstance = hInst;
            wc.lpszClassName = L"KeyboardSubTesterPage";
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            RegisterClassW(&wc);
            tstReg = true;
        }
        g_hPageTester = CreateWindowW(L"KeyboardSubTesterPage", L"",
            WS_CHILD | WS_CLIPCHILDREN, 0, 0, 100, 100, g_hSubTab, nullptr, hInst, nullptr);

        static bool glbReg = false;
        if (!glbReg)
        {
            WNDCLASSW wc{};
            wc.lpfnWndProc = KeyboardSubpages_GlobalSettingsPageProc;
            wc.hInstance = hInst;
            wc.lpszClassName = L"KeyboardSubGlobalSettingsPage";
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            RegisterClassW(&wc);
            glbReg = true;
        }
        g_hPageGlobal = CreateWindowW(L"KeyboardSubGlobalSettingsPage", L"",
            WS_CHILD | WS_CLIPCHILDREN, 0, 0, 100, 100, g_hSubTab, nullptr, hInst, nullptr);

        g_hPageFreeCombo = nullptr; // lazy-create on first tab activation

        ResizeSubUi(hWnd);
        TabCtrl_SetCurSel(g_hSubTab, 0);
        ShowSubPage(0);

        for (uint16_t hid2 : g_hids)
            InvalidateRect(g_btnByHid[hid2], nullptr, FALSE);
        return 0;
    }

    case WM_SIZE:
        LayoutKeyboardButtons(hWnd);
        ResizeSubUi(hWnd);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

    case WM_NOTIFY:
        if (g_hSubTab && ((LPNMHDR)lParam)->hwndFrom == g_hSubTab && ((LPNMHDR)lParam)->code == TCN_SELCHANGE)
        {
            int idx = TabCtrl_GetCurSel(g_hSubTab);
            if (idx < 0) idx = 0;
            ShowSubPage(idx);
            if (g_activeSubTab == 3 && g_hPageTester)
                InvalidateRect(g_hPageTester, nullptr, FALSE);
            return 0;
        }
        return 0;

    case WM_TIMER:
        if (wParam == 9104) // MOUSE_VIEW_TIMER_ID
        {
            BYTE cur = MouseCurrentState();
            if (cur != g_mouseStatePrev)
            {
                g_mouseStatePrev = cur;
                if (g_mouseViewRect.right > g_mouseViewRect.left)
                    InvalidateRect(hWnd, &g_mouseViewRect, FALSE);
            }
            return 0;
        }
        if (wParam == KEYDRAG_TIMER_ID)
        {
            KeyDrag_Tick();
            return 0;
        }
        if (wParam == KEYDELETE_TIMER_ID)
        {
            KeyDel_Tick();
            return 0;
        }
        if (wParam == KEYSWAP_TIMER_ID)
        {
            SwapFly_Tick();
            return 0;
        }
        return 0;

    case WM_LBUTTONUP:
        if (g_kdrag.dragging)
        {
            uint16_t src = g_kdrag.srcHid;
            uint16_t dst = g_kdrag.hoverHid;
            int srcPadIndex = std::clamp(g_kdrag.srcPadIndex, 0, 3);
            BindAction srcAct = g_kdrag.action;

            bool copy = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

            BindAction dstAct{};
            bool dstHasAction = (dst != 0) && BindingActions_TryGetByHidForPad(srcPadIndex, dst, dstAct);

            // Drop to empty:
            if (dst == 0)
            {
                if (!copy)
                {
                    // Unbind immediately
                    Bindings_ClearHidForPad(srcPadIndex, src);
                    Profile_SaveIni(AppPaths_BindingsIni().c_str());
                    InvalidateKeyByHid(src);

                    // Visual: shrink dragged ghost to zero (instead of instant hide)
                    KeyDrag_BeginShrinkOut();
                    return 0;
                }

                // copy-mode: behave as before (instant stop)
                KeyDrag_Stop();
                InvalidateKeyByHid(src);
                return 0;
            }

            // Normal drop to a key: stop drag right away.
            KeyDrag_Stop();

            if (dst == src)
            {
                InvalidateKeyByHid(src);
                return 0;
            }

            if (dst != 0)
            {
                if (!copy && dstHasAction && dstAct != srcAct)
                {
                    Bindings_ClearHidForPad(srcPadIndex, src);
                    Bindings_ClearHidForPad(srcPadIndex, dst);

                    BindingActions_ApplyForPad(srcPadIndex, srcAct, dst);
                    InvalidateKeyByHid(dst);

                    if (!SwapFly_Start(hWnd, src, dst, dstAct, srcPadIndex))
                    {
                        BindingActions_ApplyForPad(srcPadIndex, dstAct, src);
                        Profile_SaveIni(AppPaths_BindingsIni().c_str());
                        InvalidateKeyByHid(src);
                        InvalidateKeyByHid(dst);
                    }
                    else
                    {
                        InvalidateKeyByHid(src);
                    }
                    return 0;
                }

                if (!copy && dstHasAction && dstAct == srcAct && IsButtonAction(srcAct))
                {
                    GameButton gb{};
                    if (ActionToGameButton(srcAct, gb))
                    {
                        Bindings_RemoveButtonHidForPad(srcPadIndex, gb, src);
                        Profile_SaveIni(AppPaths_BindingsIni().c_str());
                        InvalidateKeyByHid(src);
                        InvalidateKeyByHid(dst);
                        return 0;
                    }
                }

                if (IsButtonAction(srcAct))
                {
                    GameButton gb{};
                    if (ActionToGameButton(srcAct, gb))
                    {
                        if (!copy)
                            Bindings_RemoveButtonHidForPad(srcPadIndex, gb, src);

                        BindingActions_ApplyForPad(srcPadIndex, srcAct, dst);
                        Profile_SaveIni(AppPaths_BindingsIni().c_str());
                    }
                }
                else
                {
                    BindingActions_ApplyForPad(srcPadIndex, srcAct, dst);
                    Profile_SaveIni(AppPaths_BindingsIni().c_str());
                }

                InvalidateKeyByHid(src);
                InvalidateKeyByHid(dst);
                return 0;
            }

            // unreachable because dst==0 handled above
            return 0;
        }
        return 0;

    case WM_CAPTURECHANGED:
        // IMPORTANT: do not stop if we are shrinking (we release capture intentionally)
        if (g_kdrag.dragging)
            KeyDrag_Stop();
        return 0;

    case WM_KEYDOWN:
    {
        if (wParam == VK_ESCAPE)
        {
            // cancel any drag or shrink
            KeyDrag_Stop();
            if (g_swapfly.running)
                SwapFly_Stop(true);
            if (g_kdel.running)
                KeyDel_Stop();
            return 0;
        }
        break;
    }
    //Ajout-->
    case WM_HOTKEY:
    {
        if ((int)wParam == HOTKEY_EMERGENCY_STOP)
        {
            MessageBeep(MB_ICONHAND);
            FreeComboSystem::EmergencyStop(L"user-hotkey");
            return 0;
        }
        // F1: Toggle remap
        if ((int)wParam == HOTKEY_REMAP_TOGGLE)
        {
            Backend_ToggleRemap();
            if (g_hBtnRemapToggle) InvalidateRect(g_hBtnRemapToggle, nullptr, FALSE);
            return 0;
        }
        break;
    }
    //Ajout<--


    //Ajout-->
    case WM_LBUTTONDOWN:
    {
        if (g_activeSubTab == 1 && !g_kdrag.dragging && !g_kdrag.shrinking)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            HWND child = ChildWindowFromPointEx(hWnd, pt, CWP_SKIPINVISIBLE);

            // Si on a cliqué sur une touche (un de tes buttons du clavier), on ne clear pas.
            bool clickedKeyButton = false;
            if (child && child != hWnd && GetParent(child) == hWnd)
            {
                uint16_t hid = (uint16_t)GetWindowLongPtrW(child, GWLP_USERDATA);
                if (hid != 0 && KeyboardUI_HasHid(hid))
                    clickedKeyButton = true;
            }

            if (!clickedKeyButton)
            {
                SetSelectedHid(0);
                return 0;
            }
        }
        break;
    }
    //Ajout<--


    case WM_COMMAND:
        // F1: clic bouton toggle remap
        if (LOWORD(wParam) == 0xF101)
        {
            Backend_ToggleRemap();
            if (g_hBtnRemapToggle) InvalidateRect(g_hBtnRemapToggle, nullptr, FALSE);
            return 0;
        }
        if (HIWORD(wParam) == BN_CLICKED)
        {
            HWND btn = (HWND)lParam;
            if (btn && GetParent(btn) == hWnd)
            {
                // Selection works only in Configuration tab
                if (g_activeSubTab == 1)
                {
                    uint16_t hid = (uint16_t)GetWindowLongPtrW(btn, GWLP_USERDATA);
                    // Debug: UI button clicked (on-screen)
                    {
                        char dbg[128];
                        _snprintf_s(dbg, sizeof(dbg), _TRUNCATE, "KPM: UI_BTN_CLICK HID=0x%02X\n", (unsigned int)hid);
                        OutputDebugStringA(dbg);
                    }
                    SetSelectedHid(hid);
                }
                return 0;
            }
        }
        return 0;

    case WM_APP_KEYBOARD_LAYOUT_CHANGED:
        // Rebuild visible keyboard immediately when layout preset changes from subpages.
        KeyDrag_Stop();
        if (g_kdel.running) KeyDel_Stop();
        if (g_swapfly.running) SwapFly_Stop(true);
        KeyboardUI_SetDragHoverHid(0);

        RebuildKeyboardButtons(hWnd);
        ResizeSubUi(hWnd);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

    case WM_MOUSEMOVE:
    {
        // F1: détecter hover sur le bouton toggle
        if (g_hBtnRemapToggle) {
            RECT br{}; GetWindowRect(g_hBtnRemapToggle, &br);
            MapWindowPoints(nullptr, hWnd, (LPPOINT)&br, 2);
            POINT mp{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            bool hot = PtInRect(&br, mp) != FALSE;
            if (hot != g_btnRemapHot) {
                g_btnRemapHot = hot;
                InvalidateRect(g_hBtnRemapToggle, nullptr, FALSE);
            }
        }
        break;
    }

    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lParam;
        if (dis && dis->CtlType == ODT_BUTTON)
        {
            // F1: Bouton Remap Toggle — dessin premium
            if (dis->hwndItem == g_hBtnRemapToggle)
            {
                HDC hdc = dis->hDC;
                RECT rc = dis->rcItem;
                bool on = Backend_GetRemapEnabled();
                bool hot = g_btnRemapHot;
                bool down = (dis->itemState & ODS_SELECTED) != 0;

                // Fond
                COLORREF bgBase = UiTheme::Color_ControlBg();
                COLORREF bgHot = RGB(
                    std::min(255, GetRValue(bgBase) + 18),
                    std::min(255, GetGValue(bgBase) + 18),
                    std::min(255, GetBValue(bgBase) + 18));
                COLORREF bg = (hot || down) ? bgHot : bgBase;
                HBRUSH hbr = CreateSolidBrush(bg);
                FillRect(hdc, &rc, hbr);
                DeleteObject(hbr);

                // Bordure arrondie — accent si ON, border si OFF
                COLORREF borderC = on
                    ? UiTheme::Color_Accent()
                    : UiTheme::Color_Border();
                int rad = S(hWnd, 5);
                HPEN hp = CreatePen(PS_SOLID, on ? 2 : 1, borderC);
                HGDIOBJ oldPen = SelectObject(hdc, hp);
                HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, rad * 2, rad * 2);
                SelectObject(hdc, oldPen);
                SelectObject(hdc, oldBr);
                DeleteObject(hp);

                // Pastille colorée indicateur ON/OFF (à gauche du texte)
                int dotR = S(hWnd, 5);
                int dotCx = rc.left + S(hWnd, 14);
                int dotCy = (rc.top + rc.bottom) / 2;
                COLORREF dotC = on ? UiTheme::Color_Accent() : UiTheme::Color_TextMuted();
                HBRUSH dotBr = CreateSolidBrush(dotC);
                HPEN   dotPn = CreatePen(PS_SOLID, 1, dotC);
                SelectObject(hdc, dotBr);
                SelectObject(hdc, dotPn);
                Ellipse(hdc, dotCx - dotR, dotCy - dotR, dotCx + dotR, dotCy + dotR);
                DeleteObject(dotBr); DeleteObject(dotPn);

                // Texte
                const wchar_t* label = on ? L"Remap  ON" : L"Remap  OFF";
                HFONT hf = (HFONT)SendMessageW(hWnd, WM_GETFONT, 0, 0);
                if (!hf) hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
                HGDIOBJ oldF = SelectObject(hdc, hf);
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, on ? UiTheme::Color_Text() : UiTheme::Color_TextMuted());
                RECT rcText = { dotCx + dotR + S(hWnd, 6), rc.top, rc.right - S(hWnd, 4), rc.bottom };
                DrawTextW(hdc, label, -1, &rcText,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                SelectObject(hdc, oldF);
                return TRUE;
            }
            if (GetParent(dis->hwndItem) == hWnd)
            {
                uint16_t hid = (uint16_t)GetWindowLongPtrW(dis->hwndItem, GWLP_USERDATA);
                uint16_t renderHid = hid;

                bool suppressDraggedMiniIcon = false;
                if (g_kdrag.dragging && hid != 0 && hid == g_kdrag.srcHid)
                {
                    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                    bool copyMode = ctrl && IsButtonAction(g_kdrag.action);
                    suppressDraggedMiniIcon = !copyMode;
                }

                KeyboardRender_ClearSuppressedBinding();
                if (suppressDraggedMiniIcon)
                {
                    KeyboardRender_SetSuppressedBinding(
                        g_kdrag.srcHid,
                        g_kdrag.srcPadIndex,
                        g_kdrag.action);
                }

                // Selection highlight only in Configuration tab
                bool sel = (g_activeSubTab == 1 && hid != 0 && hid == g_selectedHid);
                KeyboardRender_DrawKey(dis, renderHid, sel, -1.0f);
                KeyboardRender_ClearSuppressedBinding();

                if (hid != 0 && hid == g_dragHoverHid)
                    DrawDropHoverOutline(dis);

                return TRUE;
            }
        }
        return FALSE;
    }

    case WM_MOUSEWHEEL:
    {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (delta > 0) g_wheelUpFlash = GetTickCount();
        else           g_wheelDownFlash = GetTickCount();
    }
    break;
    case WM_DESTROY:
        UnregisterHotKey(hWnd, 0xC08);

        //Ajout-->
        UnregisterHotKey(hWnd, HOTKEY_EMERGENCY_STOP);
        UnregisterHotKey(hWnd, HOTKEY_REMAP_TOGGLE);  // F1
        //Ajout<--

        KeyDrag_Stop();
        if (g_kdrag.hGhost)
        {
            DestroyWindow(g_kdrag.hGhost);
            g_kdrag.hGhost = nullptr;
        }
        KeyDrag_FreeSurface();

        if (g_kdel.running)
            KeyDel_Stop();
        if (g_kdel.hGhost)
        {
            DestroyWindow(g_kdel.hGhost);
            g_kdel.hGhost = nullptr;
        }
        KeyDel_FreeSurface();

        if (g_swapfly.running)
            SwapFly_Stop(true);

        if (g_swapfly.hGhost)
        {
            DestroyWindow(g_swapfly.hGhost);
            g_swapfly.hGhost = nullptr;
        }
        SwapFly_FreeSurface();

        return 0;

    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

extern "C" HWND KeyboardPageMain_CreatePage(HWND hParent, HINSTANCE hInst)
{
    static bool registered = false;
    if (!registered)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = PageMainProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"PageMainClass";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
        registered = true;
    }

    return CreateWindowExW(0, L"PageMainClass", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 100, 100, hParent, nullptr, hInst, nullptr);
}