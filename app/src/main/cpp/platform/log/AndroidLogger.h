#pragma once
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include "opsu/logging/ILogger.h"

namespace opsu {

/**
 * ILogger over __android_log_print. The tag given by the caller is used as the logcat tag
 * (opsu::Log passes "Trace", "Debug", "Info", "Warn", "Error"), only if the logcat tag is empty.
 * By default a fixed logcat tag "opsu" is used instead; the caller's tag is then prefixed to the
 * message as "[tag] ", except when it is one of Trace/Debug/Info/Warn/Error (the priority shows the level).
 * Exception overloads append e.what() to the message.
 * When setSaveToFile(true) is on, lines are also written to <logDir>/opsu.log in the same format
 * as DefaultLogger ("HH:mm:ss.SSS tag: message", then what() on its own line).
 * Thread-safe.
 */
class AndroidLogger : public ILogger {
public:
    /** @param logDir      folder for the log file (e.g. the opsu config dir). Created if missing. Empty = no file logging.
     *  @param logcatTag   fixed logcat tag, default "opsu"; empty = pass the caller's tag through. */
    explicit AndroidLogger(std::string logDir = "", std::string logcatTag = "opsu");
    ~AndroidLogger() override;

    void setSaveToFile(bool value) override;

    void trace(const std::string& tag, const std::string& m) override { log(5, tag, m, nullptr); }
    void trace(const std::string& tag, const std::string& m, const std::exception& e) override { log(5, tag, m, &e); }
    void debug(const std::string& tag, const std::string& m) override { log(3, tag, m, nullptr); }
    void debug(const std::string& tag, const std::string& m, const std::exception& e) override { log(3, tag, m, &e); }
    void info(const std::string& tag, const std::string& m) override { log(4, tag, m, nullptr); }
    void info(const std::string& tag, const std::string& m, const std::exception& e) override { log(4, tag, m, &e); }
    void warn(const std::string& tag, const std::string& m) override { log(1, tag, m, nullptr); }
    void warn(const std::string& tag, const std::string& m, const std::exception& e) override { log(1, tag, m, &e); }
    void error(const std::string& tag, const std::string& m) override { log(0, tag, m, nullptr); }
    void error(const std::string& tag, const std::string& m, const std::exception& e) override { log(0, tag, m, &e); }

    /** Full path of the log file ("" if no log folder). */
    const std::string& logFilePath() const { return logPath_; }

private:
    // level codes: 5=trace(VERBOSE) 3=debug 4=info 1=warn 0=error; mapped in the .cpp
    void log(int level, const std::string& tag, const std::string& message, const std::exception* e);
    bool openFileLocked();

    std::string logDir_, logPath_, logcatTag_;
    std::mutex mutex_;  // guards saveToFile_ and file_
    bool saveToFile_ = false;
    std::ofstream file_;
};

/**
 * Creates an AndroidLogger and installs it with Log::setLogger. Log does not own its logger,
 * so the instance is kept alive in a static for the life of the process. Calling it again
 * replaces the previous instance. Returns the installed logger.
 */
AndroidLogger* installAndroidLogger(const std::string& logDir, bool saveToFile = false,
                                    const std::string& logcatTag = "opsu");

}  // namespace opsu
