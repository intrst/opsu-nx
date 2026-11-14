#pragma once
#include <exception>
#include <string>

namespace opsu {

/** Logger interface with 5 levels of logging. Port of kww.opsu.logging.ILogger. */
class ILogger {
public:
    virtual ~ILogger() = default;

    /** Sets whether logs should be written to file. */
    virtual void setSaveToFile(bool value) = 0;

    virtual void trace(const std::string& tag, const std::string& message) = 0;
    virtual void trace(const std::string& tag, const std::string& message, const std::exception& e) = 0;
    virtual void debug(const std::string& tag, const std::string& message) = 0;
    virtual void debug(const std::string& tag, const std::string& message, const std::exception& e) = 0;
    virtual void info(const std::string& tag, const std::string& message) = 0;
    virtual void info(const std::string& tag, const std::string& message, const std::exception& e) = 0;
    virtual void warn(const std::string& tag, const std::string& message) = 0;
    virtual void warn(const std::string& tag, const std::string& message, const std::exception& e) = 0;
    virtual void error(const std::string& tag, const std::string& message) = 0;
    virtual void error(const std::string& tag, const std::string& message, const std::exception& e) = 0;
};

}  // namespace opsu
