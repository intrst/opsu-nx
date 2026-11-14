#include "AndroidLogger.h"

#include <android/log.h>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <utility>

#include "opsu/logging/Log.h"

namespace opsu {
namespace {

int toAndroidPriority(int level) {
    switch (level) {
        case 5: return ANDROID_LOG_VERBOSE;
        case 3: return ANDROID_LOG_DEBUG;
        case 4: return ANDROID_LOG_INFO;
        case 1: return ANDROID_LOG_WARN;
        default: return ANDROID_LOG_ERROR;
    }
}

std::string timestamp() {  // "HH:mm:ss.SSS", same as DefaultLogger
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%02d:%02d:%02d.%03d", tm.tm_hour, tm.tm_min, tm.tm_sec,
                  static_cast<int>(ms.count()));
    return buf;
}

// logd drops/truncates entries over ~4 KB, so split long messages (at newlines when possible).
void writeLogcat(int prio, const char* tag, const std::string& text) {
    constexpr size_t kMax = 3500;
    size_t pos = 0;
    do {
        size_t len = text.size() - pos;
        if (len > kMax) {
            len = kMax;
            size_t nl = text.rfind('\n', pos + len - 1);
            if (nl != std::string::npos && nl > pos) len = nl - pos;
        }
        std::string chunk = text.substr(pos, len);
        __android_log_print(prio, tag, "%s", chunk.c_str());
        pos += len;
        if (pos < text.size() && text[pos] == '\n') ++pos;
    } while (pos < text.size());
}

bool isLevelTag(const std::string& tag) {
    return tag == "Trace" || tag == "Debug" || tag == "Info" || tag == "Warn" || tag == "Error";
}

std::unique_ptr<AndroidLogger> g_installed;
std::mutex g_installMutex;

}  // namespace

AndroidLogger::AndroidLogger(std::string logDir, std::string logcatTag)
    : logDir_(std::move(logDir)), logcatTag_(std::move(logcatTag)) {
    if (!logDir_.empty()) logPath_ = (std::filesystem::path(logDir_) / "opsu.log").string();
}

AndroidLogger::~AndroidLogger() = default;

// Java opened opsu.log once at startup (truncating) and wrote a "Run date" line. Same here, on first enable.
bool AndroidLogger::openFileLocked() {
    if (file_.is_open()) return true;
    if (logPath_.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(logDir_, ec);
    file_.open(logPath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) return false;
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::tm tm{};
    localtime_r(&t, &tm);
    std::strftime(buf, sizeof buf, "%a %b %d %H:%M:%S %Y", &tm);
    file_ << "Run date: " << buf << "\n";
    file_.flush();
    return true;
}

void AndroidLogger::setSaveToFile(bool value) {
    std::lock_guard<std::mutex> lock(mutex_);
    saveToFile_ = value;
    if (value && !openFileLocked()) {
        __android_log_print(ANDROID_LOG_WARN, logcatTag_.empty() ? "opsu" : logcatTag_.c_str(),
                            "Could not open log file '%s'", logPath_.c_str());
    }
}

void AndroidLogger::log(int level, const std::string& tag, const std::string& message,
                        const std::exception* e) {
    // logcat
    std::string text = (logcatTag_.empty() || isLevelTag(tag)) ? message : "[" + tag + "] " + message;
    if (e) text += std::string(": ") + e->what();
    writeLogcat(toAndroidPriority(level), logcatTag_.empty() ? tag.c_str() : logcatTag_.c_str(), text);

    // file (format identical to DefaultLogger)
    std::lock_guard<std::mutex> lock(mutex_);
    if (saveToFile_ && file_.is_open()) {
        file_ << timestamp() << " " << tag << ": " << message << "\n";
        if (e) file_ << e->what() << "\n";
        file_.flush();
    }
}

AndroidLogger* installAndroidLogger(const std::string& logDir, bool saveToFile,
                                    const std::string& logcatTag) {
    std::lock_guard<std::mutex> lock(g_installMutex);
    auto next = std::make_unique<AndroidLogger>(logDir, logcatTag);
    next->setSaveToFile(saveToFile);
    Log::setLogger(next.get());  // switch first, then free the old one
    g_installed = std::move(next);
    return g_installed.get();
}

}  // namespace opsu
