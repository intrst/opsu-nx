#include "opsu/logging/StubLogger.h"
#include <iostream>

namespace opsu {

StubLogger& StubLogger::get() {
    static StubLogger instance;
    return instance;
}

static void out(const std::string& tag, const std::string& m) {
    std::cout << "[" << tag << "] " << m << std::endl;
}
static void out(const std::string& tag, const std::string& m, const std::exception& e) {
    out(tag, m);
    std::cerr << e.what() << std::endl;  // replaces Throwable.printStackTrace()
}

void StubLogger::trace(const std::string& t, const std::string& m) { out(t, m); }
void StubLogger::trace(const std::string& t, const std::string& m, const std::exception& e) { out(t, m, e); }
void StubLogger::debug(const std::string& t, const std::string& m) { out(t, m); }
void StubLogger::debug(const std::string& t, const std::string& m, const std::exception& e) { out(t, m, e); }
void StubLogger::info(const std::string& t, const std::string& m) { out(t, m); }
void StubLogger::info(const std::string& t, const std::string& m, const std::exception& e) { out(t, m, e); }
void StubLogger::warn(const std::string& t, const std::string& m) { out(t, m); }
void StubLogger::warn(const std::string& t, const std::string& m, const std::exception& e) { out(t, m, e); }
void StubLogger::error(const std::string& t, const std::string& m) { out(t, m); }
void StubLogger::error(const std::string& t, const std::string& m, const std::exception& e) { out(t, m, e); }

}  // namespace opsu
