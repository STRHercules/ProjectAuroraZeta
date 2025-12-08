#include "Logger.h"

namespace Engine {

namespace {
std::string_view toLabel(LogLevel level) {
    switch (level) {
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
        default:
            return "ERROR";
    }
}
}  // namespace

void Logger::log(LogLevel level, std::string_view message) {
    using namespace std::chrono;

    const auto now = system_clock::now();
    const auto t = system_clock::to_time_t(now);
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count();
    std::cout << '[' << oss.str() << "] [" << toLabel(level) << "] " << message << '\n';
}

}  // namespace Engine
