// macro_json_storage.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include "macro_json_storage.h"
#include "macro_system.h"

// ============================================================================
// Helper Functions
// ============================================================================

std::wstring MacroJsonStorage::EscapeJsonString(const std::wstring& str)
{
    std::wstring result;
    result.reserve(str.length() + 10);
    
    for (wchar_t c : str)
    {
        switch (c)
        {
        case L'\"': result += L"\\\""; break;
        case L'\\': result += L"\\\\"; break;
        case L'\b': result += L"\\b"; break;
        case L'\f': result += L"\\f"; break;
        case L'\n': result += L"\\n"; break;
        case L'\r': result += L"\\r"; break;
        case L'\t': result += L"\\t"; break;
        default:
            if (c < 32)
            {
                wchar_t buf[8];
                swprintf_s(buf, L"\\u%04x", (int)c);
                result += buf;
            }
            else
            {
                result += c;
            }
        }
    }
    
    return result;
}

std::wstring MacroJsonStorage::UnescapeJsonString(const std::wstring& str)
{
    std::wstring result;
    result.reserve(str.length());
    
    for (size_t i = 0; i < str.length(); ++i)
    {
        if (str[i] == L'\\' && i + 1 < str.length())
        {
            switch (str[i + 1])
            {
            case L'\"': result += L'\"'; i++; break;
            case L'\\': result += L'\\'; i++; break;
            case L'b': result += L'\b'; i++; break;
            case L'f': result += L'\f'; i++; break;
            case L'n': result += L'\n'; i++; break;
            case L'r': result += L'\r'; i++; break;
            case L't': result += L'\t'; i++; break;
            case L'u':
                if (i + 5 < str.length())
                {
                    wchar_t hex[5] = { str[i+2], str[i+3], str[i+4], str[i+5], 0 };
                    result += (wchar_t)wcstol(hex, nullptr, 16);
                    i += 5;
                }
                break;
            default:
                result += str[i];
            }
        }
        else
        {
            result += str[i];
        }
    }
    
    return result;
}

std::wstring MacroJsonStorage::ActionTypeToString(MacroActionType type)
{
    switch (type)
    {
    case MacroActionType::KeyPress: return L"KeyPress";
    case MacroActionType::KeyRelease: return L"KeyRelease";
    case MacroActionType::MouseMove: return L"MouseMove";
    case MacroActionType::MouseLeftDown: return L"MouseLeftDown";
    case MacroActionType::MouseLeftUp: return L"MouseLeftUp";
    case MacroActionType::MouseRightDown: return L"MouseRightDown";
    case MacroActionType::MouseRightUp: return L"MouseRightUp";
    case MacroActionType::MouseMiddleDown: return L"MouseMiddleDown";
    case MacroActionType::MouseMiddleUp: return L"MouseMiddleUp";
    case MacroActionType::MouseWheelUp: return L"MouseWheelUp";
    case MacroActionType::MouseWheelDown: return L"MouseWheelDown";
    case MacroActionType::Delay: return L"Delay";
    default: return L"None";
    }
}

MacroActionType MacroJsonStorage::StringToActionType(const std::wstring& str)
{
    if (str == L"KeyPress") return MacroActionType::KeyPress;
    if (str == L"KeyRelease") return MacroActionType::KeyRelease;
    if (str == L"MouseMove") return MacroActionType::MouseMove;
    if (str == L"MouseLeftDown") return MacroActionType::MouseLeftDown;
    if (str == L"MouseLeftUp") return MacroActionType::MouseLeftUp;
    if (str == L"MouseRightDown") return MacroActionType::MouseRightDown;
    if (str == L"MouseRightUp") return MacroActionType::MouseRightUp;
    if (str == L"MouseMiddleDown") return MacroActionType::MouseMiddleDown;
    if (str == L"MouseMiddleUp") return MacroActionType::MouseMiddleUp;
    if (str == L"MouseWheelUp") return MacroActionType::MouseWheelUp;
    if (str == L"MouseWheelDown") return MacroActionType::MouseWheelDown;
    if (str == L"Delay") return MacroActionType::Delay;
    return MacroActionType::None;
}

// ============================================================================
// Export Functions
// ============================================================================

std::wstring MacroJsonStorage::ExportMacroToJson(const Macro* macro)
{
    if (!macro)
        return L"{}";
    
    std::wostringstream json;
    json << L"{\n";
    json << L"  \"name\": \"" << EscapeJsonString(macro->name) << L"\",\n";
    json << L"  \"isLooping\": " << (macro->isLooping ? L"true" : L"false") << L",\n";
    json << L"  \"playbackSpeed\": " << macro->playbackSpeed << L",\n";
    json << L"  \"blockKeysDuringPlayback\": " << (macro->blockKeysDuringPlayback ? L"true" : L"false") << L",\n";
    json << L"  \"totalDuration\": " << macro->totalDuration << L",\n";
    json << L"  \"actions\": [\n";
    
    for (size_t i = 0; i < macro->actions.size(); ++i)
    {
        const MacroAction& action = macro->actions[i];
        
        json << L"    {\n";
        json << L"      \"type\": \"" << ActionTypeToString(action.type) << L"\",\n";
        json << L"      \"timestamp\": " << action.timestamp << L",\n";
        json << L"      \"hid\": " << action.hid << L",\n";
        json << L"      \"mouseX\": " << action.mousePos.x << L",\n";
        json << L"      \"mouseY\": " << action.mousePos.y << L",\n";
        json << L"      \"delayMs\": " << action.delayMs << L"\n";
        json << L"    }";
        
        if (i < macro->actions.size() - 1)
            json << L",";
        json << L"\n";
    }
    
    json << L"  ]\n";
    json << L"}";
    
    return json.str();
}

bool MacroJsonStorage::SaveToJson(const wchar_t* filePath)
{
    std::wofstream file(filePath);
    if (!file.is_open())
        return false;
    
    file << L"{\n";
    file << L"  \"version\": \"1.0\",\n";
    file << L"  \"macros\": [\n";
    
    std::vector<int> macroIds = MacroSystem::GetAllMacroIds();
    
    for (size_t i = 0; i < macroIds.size(); ++i)
    {
        Macro* macro = MacroSystem::GetMacro(macroIds[i]);
        if (macro)
        {
            std::wstring macroJson = ExportMacroToJson(macro);
            
            // Indent the macro JSON
            std::wistringstream iss(macroJson);
            std::wstring line;
            bool first = true;
            
            while (std::getline(iss, line))
            {
                if (!first)
                    file << L"\n";
                file << L"    " << line;
                first = false;
            }
            
            if (i < macroIds.size() - 1)
                file << L",";
            file << L"\n";
        }
    }
    
    file << L"  ]\n";
    file << L"}\n";
    
    file.close();
    return true;
}

// ============================================================================
// Import Functions (Simplified - full JSON parser would be more complex)
// ============================================================================

bool MacroJsonStorage::ImportMacroFromJson(const std::wstring& jsonStr, Macro& outMacro)
{
    // This is a simplified parser - a full implementation would use a proper JSON library
    // For now, we'll just provide the structure
    
    // TODO: Implement proper JSON parsing
    // For production use, consider using a library like nlohmann/json or RapidJSON
    
    return false;
}

bool MacroJsonStorage::LoadFromJson(const wchar_t* filePath)
{
    // TODO: Implement JSON loading
    // For production use, consider using a library like nlohmann/json or RapidJSON
    
    return false;
}

// ============================================================================
// Directory Management
// ============================================================================

std::wstring MacroJsonStorage::GetMacrosDirectory()
{
    wchar_t path[MAX_PATH];
    
    // Get the executable directory (Release folder)
    if (GetModuleFileNameW(nullptr, path, MAX_PATH) > 0)
    {
        // Remove the executable name to get the directory
        std::wstring exePath = path;
        size_t lastSlash = exePath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos)
        {
            std::wstring macrosDir = exePath.substr(0, lastSlash) + L"\\Macros";
            
            // Create directory if it doesn't exist
            CreateDirectoryW(macrosDir.c_str(), nullptr);
            
            return macrosDir;
        }
    }
    
    // Fallback to current directory
    return L".\\Macros";
}
