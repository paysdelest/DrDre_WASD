#pragma once
#include <fstream>
#include <string>
#include <mutex>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <windows.h>

class Logger {
public:

    enum Level { INFO, WARNING, ERR, CRITICAL };

    // Enable or disable logging (controlled by settings.ini logging=0/1)
    static void SetEnabled(bool enabled) {
        Enabled() = enabled;
    }

    static bool IsEnabled() {
        return Enabled();
    }

    static void Init(const std::string& filename = "app.log") {
        std::lock_guard<std::mutex> lock(Mutex());
        if (!Enabled()) return; // FIX: ne rien faire si logging desactive
        Stream().open(filename, std::ios::out | std::ios::trunc);
        Stream() << "[SYSTEM] === Demarrage de l'application ===" << std::endl;
        Stream() << "[SYSTEM] Fichier log initialise : " << filename << std::endl;
        Stream().flush();
    }

    static void Close() {
        std::lock_guard<std::mutex> lock(Mutex());
        if (!Enabled() || !Stream().is_open()) return;
        Stream() << "[SYSTEM] === Arret de l'application ===" << std::endl;
        Stream().flush();
        Stream().close();
    }

    static void Log(Level level, const std::string& section, const std::string& message) {
        if (!Enabled()) return; // FIX: sortie rapide si logging desactive
        std::lock_guard<std::mutex> lock(Mutex());
        if (!Stream().is_open()) return;

        std::string entry = "[" + Timestamp() + "] "
                          + "[" + LevelStr(level) + "] "
                          + "[" + section + "] "
                          + message;

        Stream() << entry << std::endl;
        Stream().flush();

        OutputDebugStringA((entry + "\n").c_str());
    }

    static void Info(const std::string& section, const std::string& msg)     { Log(INFO,     section, msg); }
    static void Warn(const std::string& section, const std::string& msg)     { Log(WARNING,  section, msg); }
    static void Error(const std::string& section, const std::string& msg)    { Log(ERR,      section, msg); }
    static void Critical(const std::string& section, const std::string& msg) { Log(CRITICAL, section, msg); }

private:

    static bool& Enabled() { static bool e = false; return e; } // desactive par defaut

    static std::string Timestamp() {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_val{};
        localtime_s(&tm_val, &t);
        std::ostringstream oss;
        oss << std::put_time(&tm_val, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    static std::string LevelStr(Level l) {
        switch (l) {
            case INFO:     return "INFO    ";
            case WARNING:  return "WARNING ";
            case ERR:      return "ERROR   ";
            case CRITICAL: return "CRITICAL";
            default:       return "UNKNOWN ";
        }
    }

    static std::ofstream& Stream() { static std::ofstream s; return s; }
    static std::mutex&    Mutex()  { static std::mutex m;    return m; }
};
