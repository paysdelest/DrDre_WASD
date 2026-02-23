#pragma once
#include <windows.h>

// Test interface for HallJoy Bind Macro System
// Simple interface to demonstrate and test the functionality

// Window procedure for test dialog
LRESULT CALLBACK HallJoyBindMacroTestProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Function to create test dialog
HWND HallJoyBindMacroTest_Create(HWND hParent, HINSTANCE hInst);

// Test functions
void HallJoyBindMacroTest_CreatePingMacro(HWND hWnd);
void HallJoyBindMacroTest_CreateCompositeMacro(HWND hWnd);
void HallJoyBindMacroTest_ToggleDirectBinding(HWND hWnd);
void HallJoyBindMacroTest_ClearAllMacros(HWND hWnd);
void HallJoyBindMacroTest_RefreshMacroList(HWND hWnd);
