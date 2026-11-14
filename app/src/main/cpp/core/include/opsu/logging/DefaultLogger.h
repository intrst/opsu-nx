#pragma once
#include <mutex>
#include <ostream>
#include "opsu/logging/ILogger.h"

namespace opsu {

/**
 * Logs every message to the console and, if enabled, to a log stream.
 * Port of kww.opsu.logging.DefaultLogger; the log stream replaces DefaultLogSystem.out.
 */
class DefaultLogger : public ILogger {
public:
    /** The log file stream (Java: DefaultLogSystem.out). May be null. Not owned. */
    static std::ostream* logStream;

    void setSaveToFile(bool value) override { saveToFile_ = value; }

    void trace(const std::string& tag, const std::string& m) override { log(tag, m, nullptr); }
    void trace(const std::string& tag, const std::string& m, const std::exception& e) override { log(tag, m, &e); }
    void debug(const std::string& tag, const std::string& m) override { log(tag, m, nullptr); }
    void debug(const std::string& tag, const std::string& m, const std::exception& e) override { log(tag, m, &e); }
    void info(const std::string& tag, const std::string& m) override { log(tag, m, nullptr); }
    void info(const std::string& tag, const std::string& m, const std::exception& e) override { log(tag, m, &e); }
    void warn(const std::string& tag, const std::string& m) override { log(tag, m, nullptr); }
    void warn(const std::string& tag, const std::string& m, const std::exception& e) override { log(tag, m, &e); }
    void error(const std::string& tag, const std::string& m) override { log(tag, m, nullptr); }
    void error(const std::string& tag, const std::string& m, const std::exception& e) override { log(tag, m, &e); }

private:
    void log(const std::string& tag, const std::string& message, const std::exception* e);
    static std::string timestamp();  // "HH:mm:ss.SSS"

    bool saveToFile_ = false;
    std::mutex mutex_;
};

}  // namespace opsu
