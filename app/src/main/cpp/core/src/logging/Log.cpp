#include "opsu/logging/Log.h"
#include "opsu/logging/DefaultLogger.h"

namespace opsu {
namespace {
ILogger* g_logger = nullptr;
int g_level = Log::VERBOSE;
ILogger& logger() {
    static DefaultLogger def;
    return g_logger ? *g_logger : def;
}
std::string pre(const std::string& p, const std::string& m) { return "[" + p + "] " + m; }
}  // namespace

void Log::setLogger(ILogger* in) { g_logger = in; }
void Log::setSaveToFile(bool v) { logger().setSaveToFile(v); }
void Log::setLoggingLevel(int l) { g_level = l; }
bool Log::isLoggingAvailable(int level) { return !(level < g_level || g_level >= ASSERT); }

void Log::log(const std::string& m) { trace(m); }
void Log::log(const std::string& p, const std::string& m) { trace(p, m); }
void Log::trace(const std::string& m) { if (isLoggingAvailable(VERBOSE)) logger().trace("Trace", m); }
void Log::trace(const std::string& p, const std::string& m) { if (isLoggingAvailable(VERBOSE)) logger().trace("Trace", pre(p, m)); }
void Log::debug(const std::string& m) { if (isLoggingAvailable(DEBUG)) logger().debug("Debug", m); }
void Log::debug(const std::string& p, const std::string& m) { if (isLoggingAvailable(DEBUG)) logger().debug("Debug", pre(p, m)); }
void Log::info(const std::string& m) { if (isLoggingAvailable(INFO)) logger().info("Info", m); }
void Log::info(const std::string& p, const std::string& m) { if (isLoggingAvailable(INFO)) logger().info("Info", pre(p, m)); }
void Log::warn(const std::string& m) { if (isLoggingAvailable(WARN)) logger().warn("Warn", m); }
void Log::warn(const std::string& p, const std::string& m) { if (isLoggingAvailable(WARN)) logger().warn("Warn", pre(p, m)); }
void Log::warn(const std::string& m, const std::exception& e) { if (isLoggingAvailable(WARN)) logger().warn("Warn", m, e); }
void Log::warn(const std::string& p, const std::string& m, const std::exception& e) { if (isLoggingAvailable(WARN)) logger().warn("Warn", pre(p, m), e); }
void Log::error(const std::string& m) { if (isLoggingAvailable(ERROR)) logger().error("Error", m); }
void Log::error(const std::string& p, const std::string& m) { if (isLoggingAvailable(ERROR)) logger().error("Error", pre(p, m)); }
void Log::error(const std::exception& e) { if (isLoggingAvailable(ERROR)) error("", e); }
void Log::error(const std::string& m, const std::exception& e) { if (isLoggingAvailable(ERROR)) logger().error("Error", m, e); }
void Log::error(const std::string& p, const std::string& m, const std::exception& e) { if (isLoggingAvailable(ERROR)) logger().error("Error", pre(p, m), e); }

}  // namespace opsu
