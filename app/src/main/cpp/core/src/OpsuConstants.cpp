#include "opsu/OpsuConstants.h"
#include <cctype>
#include <cstdio>

namespace opsu {
namespace OpsuConstants {

std::string urlEncode(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        // same unreserved set as java.net.URLEncoder
        if (std::isalnum(c) || c == '.' || c == '-' || c == '*' || c == '_') {
            out += static_cast<char>(c);
        } else if (c == ' ') {
            out += '+';
        } else {
            char buf[4];
            std::snprintf(buf, sizeof buf, "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

std::string getChangelogURI(const std::string& version) {
    return std::string("https://github.com/fluddokt/opsu/releases/tag/") + urlEncode(version);
}

}  // namespace OpsuConstants
}  // namespace opsu
