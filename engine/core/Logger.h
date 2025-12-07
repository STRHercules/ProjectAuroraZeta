// Minimal console logger; replace with structured logging later.
#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>

namespace Engine {

enum class LogLevel { Info, Warning, Error };

class Logger {
public:
    static void log(LogLevel level, std::string_view message);
};

inline void logInfo(std::string_view msg) { Logger::log(LogLevel::Info, msg); }
inline void logWarn(std::string_view msg) { Logger::log(LogLevel::Warning, msg); }
inline void logError(std::string_view msg) { Logger::log(LogLevel::Error, msg); }

}  // namespace Engine
