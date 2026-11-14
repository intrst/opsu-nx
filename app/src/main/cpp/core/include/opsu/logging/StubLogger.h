#pragma once
#include "opsu/logging/ILogger.h"

namespace opsu {

/** Logger that just prints messages to stdout/stderr. Port of StubLogger.java (singleton). */
class StubLogger final : public ILogger {
public:
    static StubLogger& get();

    void setSaveToFile(bool) override {}  // not intended to be implemented

    void trace(const std::string& tag, const std::string& m) override;
    void trace(const std::string& tag, const std::string& m, const std::exception& e) override;
    void debug(const std::string& tag, const std::string& m) override;
    void debug(const std::string& tag, const std::string& m, const std::exception& e) override;
    void info(const std::string& tag, const std::string& m) override;
    void info(const std::string& tag, const std::string& m, const std::exception& e) override;
    void warn(const std::string& tag, const std::string& m) override;
    void warn(const std::string& tag, const std::string& m, const std::exception& e) override;
    void error(const std::string& tag, const std::string& m) override;
    void error(const std::string& tag, const std::string& m, const std::exception& e) override;

private:
    StubLogger() = default;
};

}  // namespace opsu
