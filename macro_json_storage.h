#pragma once
#include "macro_system.h"
#include <string>

// JSON Storage for Macros
// Provides JSON import/export functionality for macros

class MacroJsonStorage
{
public:
    // Save macros to JSON file
    static bool SaveToJson(const wchar_t* filePath);
    
    // Load macros from JSON file
    static bool LoadFromJson(const wchar_t* filePath);
    
    // Export single macro to JSON string
    static std::wstring ExportMacroToJson(const Macro* macro);
    
    // Import single macro from JSON string
    static bool ImportMacroFromJson(const std::wstring& jsonStr, Macro& outMacro);
    
    // Get default macros directory
    static std::wstring GetMacrosDirectory();
    
private:
    static std::wstring EscapeJsonString(const std::wstring& str);
    static std::wstring UnescapeJsonString(const std::wstring& str);
    static std::wstring ActionTypeToString(MacroActionType type);
    static MacroActionType StringToActionType(const std::wstring& str);
};
