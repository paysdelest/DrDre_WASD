#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>

#include "app.h"
#include "win_util.h"
#include "Resource.h"
#include "Logger.h"

#pragma comment(lib, "gdiplus.lib")

static HMODULE g_wootingWrapperModule = nullptr;

static bool FileExistsNoDir(const std::wstring& path)
{
    DWORD a = GetFileAttributesW(path.c_str());
    return (a != INVALID_FILE_ATTRIBUTES) && ((a & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

static bool WriteBufferToFile(const std::wstring& path, const void* data, DWORD size)
{
    Logger::Info("IO", "Ecriture fichier");

    HANDLE h = CreateFileW(path.c_str(),
        GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (h == INVALID_HANDLE_VALUE) {
        Logger::Error("IO", "CreateFileW echoue !");
        return false;
    }

    const BYTE* p = (const BYTE*)data;
    DWORD total = 0;
    while (total < size)
    {
        DWORD n = 0;
        DWORD chunk = size - total;
        if (!WriteFile(h, p + total, chunk, &n, nullptr) || n == 0)
        {
            Logger::Error("IO", "WriteFile echoue en cours d'ecriture");
            CloseHandle(h);
            return false;
        }
        total += n;
    }

    FlushFileBuffers(h);
    CloseHandle(h);
    Logger::Info("IO", "Ecriture OK");
    return true;
}

static bool ExtractResourceToFile(HINSTANCE hInst, int resId, const std::wstring& dstPath)
{
    Logger::Info("RESOURCE", "Extraction ressource ID=" + std::to_string(resId));

    HRSRC hRes = FindResourceW(hInst, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hRes) {
        Logger::Error("RESOURCE", "FindResourceW echoue !");
        return false;
    }

    DWORD sz = SizeofResource(hInst, hRes);
    if (sz == 0) {
        Logger::Error("RESOURCE", "SizeofResource == 0 !");
        return false;
    }

    HGLOBAL hData = LoadResource(hInst, hRes);
    if (!hData) {
        Logger::Error("RESOURCE", "LoadResource echoue !");
        return false;
    }

    const void* p = LockResource(hData);
    if (!p) {
        Logger::Error("RESOURCE", "LockResource echoue !");
        return false;
    }

    std::wstring tmp = dstPath + L".tmp";
    DeleteFileW(tmp.c_str());

    if (!WriteBufferToFile(tmp, p, sz))
    {
        Logger::Error("RESOURCE", "WriteBufferToFile echoue !");
        DeleteFileW(tmp.c_str());
        return false;
    }

    if (!MoveFileExW(tmp.c_str(), dstPath.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        Logger::Error("RESOURCE", "MoveFileExW echoue !");
        DeleteFileW(tmp.c_str());
        return false;
    }

    Logger::Info("RESOURCE", "Extraction OK");
    return true;
}

static bool EnsureWootingWrapperReady(HINSTANCE hInst)
{
    const std::wstring dllPath = WinUtil_BuildPathNearExe(L"wooting_analog_wrapper.dll");
    Logger::Info("WOOTING", "Recherche DLL wrapper...");

    if (!FileExistsNoDir(dllPath))
    {
        Logger::Warn("WOOTING", "DLL introuvable, extraction depuis les ressources...");

        if (!ExtractResourceToFile(hInst, IDR_WOOTING_WRAPPER, dllPath))
        {
            Logger::Critical("WOOTING", "Echec extraction DLL depuis les ressources !");
            return false;
        }
        Logger::Info("WOOTING", "DLL extraite avec succes");
    }
    else
    {
        Logger::Info("WOOTING", "DLL trouvee sur disque");
    }

    g_wootingWrapperModule = LoadLibraryW(dllPath.c_str());
    if (!g_wootingWrapperModule)
    {
        DWORD error = GetLastError();
        Logger::Critical("WOOTING", "LoadLibraryW echoue ! Code erreur : " + std::to_string(error));
        return false;
    }

    Logger::Info("WOOTING", "LoadLibraryW OK - DLL chargee en memoire");
    return true;
}

static void InitDpiAwareness()
{
    Logger::Info("DPI", "Initialisation DPI awareness...");

    HMODULE u32 = GetModuleHandleW(L"user32.dll");

    using SetCtxFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    auto setCtx = (SetCtxFn)GetProcAddress(u32, "SetProcessDpiAwarenessContext");
    if (setCtx)
    {
        setCtx(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
        Logger::Info("DPI", "SetProcessDpiAwarenessContext OK");
        return;
    }

    using SetAwareFn = BOOL(WINAPI*)();
    auto setAware = (SetAwareFn)GetProcAddress(u32, "SetProcessDPIAware");
    if (setAware) {
        setAware();
        Logger::Info("DPI", "SetProcessDPIAware OK (fallback)");
    }
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow)
{
    // Lire le settings.ini AVANT d'initialiser le logger
    std::wstring iniPath = WinUtil_BuildPathNearExe(L"settings.ini");
    int loggingEnabled = GetPrivateProfileIntW(L"Main", L"Logging", 0, iniPath.c_str());
    Logger::SetEnabled(loggingEnabled != 0);
    Logger::Init("HallJoy_log.txt");
    Logger::Info("MAIN", "=== wWinMain demarre ===");

    Logger::Info("MAIN", "Avant EnsureWootingWrapperReady");
    if (!EnsureWootingWrapperReady(hInst))
    {
        Logger::Critical("MAIN", "EnsureWootingWrapperReady echoue - arret");
        MessageBoxW(nullptr,
            L"Failed to prepare wooting_analog_wrapper.dll near the executable.",
            L"HallJoy", MB_ICONERROR | MB_OK);
        Logger::Close();
        return 1;
    }
    Logger::Info("MAIN", "EnsureWootingWrapperReady OK");

    Logger::Info("MAIN", "Avant InitDpiAwareness");
    InitDpiAwareness();
    Logger::Info("MAIN", "InitDpiAwareness OK");

    Logger::Info("MAIN", "Avant GdiplusStartup");
    Gdiplus::GdiplusStartupInput gdiInput;
    ULONG_PTR gdiToken = 0;
    Gdiplus::Status gdiStatus = Gdiplus::GdiplusStartup(&gdiToken, &gdiInput, nullptr);
    if (gdiStatus != Gdiplus::Ok) {
        Logger::Error("MAIN", "GdiplusStartup echoue ! Code : " + std::to_string(gdiStatus));
    } else {
        Logger::Info("MAIN", "GdiplusStartup OK");
    }

    Logger::Info("MAIN", "Avant App_Run - entree dans la boucle principale");
    int result = App_Run(hInst, nCmdShow);
    Logger::Info("MAIN", "App_Run termine, code retour : " + std::to_string(result));

    if (gdiStatus == Gdiplus::Ok && gdiToken != 0) {
        Gdiplus::GdiplusShutdown(gdiToken);
        Logger::Info("MAIN", "GdiplusShutdown OK");
    }

    Logger::Close();
    return result;
}
