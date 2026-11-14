#pragma once
#include <exception>
#include <string>
#include "opsu/logging/ILogger.h"

namespace opsu {

/** Static logging facade. Port of fluddokt.opsu.fake.Log. */
class Log {
public:
    /** Logging levels (android values). */
    static constexpr int VERBOSE = 2, DEBUG = 3, INFO = 4, WARN = 5, ERROR = 6, ASSERT = 7;

    /** Sets the logger (not owned; must outlive its use). nullptr restores the default DefaultLogger. */
    static void setLogger(ILogger* in);
    static void setSaveToFile(bool value);
    static void setLoggingLevel(int level);
    static bool isLoggingAvailable(int level);

    static void log(const std::string& message);
    static void log(const std::string& prefix, const std::string& message);
    static void trace(const std::string& message);
    static void trace(const std::string& prefix, const std::string& message);
    static void debug(const std::string& message);
    static void debug(const std::string& prefix, const std::string& message);
    static void info(const std::string& message);
    static void info(const std::string& prefix, const std::string& message);
    static void warn(const std::string& message);
    static void warn(const std::string& prefix, const std::string& message);
    static void warn(const std::string& message, const std::exception& e);
    static void warn(const std::string& prefix, const std::string& message, const std::exception& e);
    static void error(const std::string& message);
    static void error(const std::string& prefix, const std::string& message);
    static void error(const std::exception& e);
    static void error(const std::string& message, const std::exception& e);
    static void error(const std::string& prefix, const std::string& message, const std::exception& e);
    static void setVerbose(bool) {}
};

}  // namespace opsu
