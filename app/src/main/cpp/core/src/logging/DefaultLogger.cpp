#include "opsu/logging/DefaultLogger.h"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iostream>

namespace opsu {

std::ostream* DefaultLogger::logStream = nullptr;

std::string DefaultLogger::timestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::snprintf(buf, sizeof buf, "%02d:%02d:%02d.%03d", tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms.count()));
    return buf;
}

void DefaultLogger::log(const std::string& tag, const std::string& message, const std::exception* e) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "[" << tag << "] " << message << std::endl;
    if (e) std::cerr << e->what() << std::endl;  // replaces printStackTrace()

    if (saveToFile_ && logStream) {
        *logStream << timestamp() << " " << tag << ": " << message << "\n";
        if (e) *logStream << e->what() << "\n";
        logStream->flush();
    }
}

}  // namespace opsu
