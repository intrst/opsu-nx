// Behavioural tests for batch 21: java::urlEncode edge cases the golden file cannot hold (malformed input) + the server idioms.
#include <cstdio>
#include <string>
#include "opsu/util/JavaCompat.h"

using namespace opsu;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

int main() {
    CHECK(java::urlEncode("") == "");
    CHECK(java::urlEncode("a b") == "a+b");
    CHECK(java::urlEncode("A.B-C*D_E") == "A.B-C*D_E");
    CHECK(java::urlEncode("~") == "%7E");            // '~' is NOT safe in Java's URLEncoder
    CHECK(java::urlEncode("100%") == "100%25");
    CHECK(java::urlEncode("日") == "%E6%97%A5");
    CHECK(java::urlEncode("😀") == "%F0%9F%98%80");
    CHECK(java::urlEncode(std::string("a\0b", 3)) == "a%00b");
    // malformed UTF-8 can't exist in a Java String: each maximal bad subpart becomes U+FFFD
    CHECK(java::urlEncode("\xff") == "%EF%BF%BD");
    CHECK(java::urlEncode("a\x80z") == "a%EF%BF%BDz");
    CHECK(java::urlEncode("\xe6\x97") == "%EF%BF%BD");  // truncated 3-byte sequence = one replacement
    // idiom used by MnetworkServer: URLEncoder.encode(q, "UTF-8").replace("+", "%20")
    {
        std::string e = java::urlEncode("a b+c");
        std::string r;
        for (char c : e) { if (c == '+') r += "%20"; else r += c; }
        CHECK(e == "a+b%2Bc");
        CHECK(r == "a%20b%2Bc");
    }
    if (failures == 0) std::printf("batch21: all tests passed\n");
    return failures ? 1 : 0;
}
