// Tests for batch 20 (util/Json: org.json style reader, util/JavaCompat parseDouble / doubleToString).
// Behavioural: there is no org.json in the core to run as a golden, so each documented rule of Json.h is pinned here.
#include <climits>
#include <cmath>
#include <cstdio>
#include <string>
#include "opsu/util/Json.h"
#include "opsu/util/JavaCompat.h"

using namespace opsu;
using namespace opsu::json;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
#define THROWS(expr) do { bool t_ = false; try { (void)(expr); } catch (const JSONException&) { t_ = true; } if (!t_) { std::printf("FAIL %s:%d  expected JSONException: %s\n", __FILE__, __LINE__, #expr); failures++; } } while (0)

static JSONObject O(const std::string& s) { return JSONObject(s); }

int main() {
    // ---- a payload shaped like the Ripple / Catboy search results the servers read
    {
        JSONArray arr("[{\"SetID\":1234,\"LastUpdate\":\"2015-09-30T09:39:04Z\",\"Title\":\"T\",\"Artist\":\"A\",\"Creator\":\"C\"}, "
                      "{\"SetID\":5,\"LastUpdate\":\"x\",\"Title\":\"\\u30c6\",\"Artist\":\"A\",\"Creator\":\"C\"}]");
        CHECK(arr.length() == 2);
        const JSONObject& a = arr.getJSONObject(0);
        CHECK(a.getInt("SetID") == 1234 && a.getString("Title") == "T" && a.getString("LastUpdate") == "2015-09-30T09:39:04Z");
        CHECK(arr.getJSONObject(1).getString("Title") == "\xE3\x83\x86");  // U+30C6 as UTF-8
        THROWS(a.getString("Missing"));
        THROWS(a.getInt("Missing"));
        THROWS(arr.getJSONObject(2));
        CHECK(arr.optJSONObject(2) == nullptr && arr.optJSONObject(0) != nullptr);
    }
    // top-level shape rules
    THROWS(JSONArray("{\"a\":1}"));
    THROWS(JSONObject("[1]"));
    THROWS(JSONObject(""));
    THROWS(JSONArray("  "));
    CHECK(JSONArray("  \n[ ]  ").length() == 0);
    CHECK(JSONObject("{}").length() == 0);
    CHECK(JSONObject("{\"a\":1} trailing garbage").length() == 1);  // text after the value is ignored
    THROWS(JSONObject("{\"a\":1"));
    THROWS(JSONArray("[1,2"));
    THROWS(JSONObject("{\"a\":1,\"a\":2}"));  // duplicate key

    // ---- getString is strict, optString converts
    {
        JSONObject o = O("{\"s\":\"x\",\"i\":12,\"l\":12345678901,\"d\":1.5,\"w\":1.0,\"e\":1e7,\"t\":true,\"n\":null,\"o\":{\"k\":[1,2.0]},\"a\":[\"q\"]}");
        CHECK(o.getString("s") == "x");
        THROWS(o.getString("i"));
        THROWS(o.getString("t"));
        THROWS(o.getString("n"));
        THROWS(o.getString("o"));
        CHECK(o.optString("s", "def") == "x");
        CHECK(o.optString("i") == "12" && o.optString("l") == "12345678901");
        CHECK(o.optString("d") == "1.5" && o.optString("w") == "1.0" && o.optString("e") == "1.0E7");
        CHECK(o.optString("t") == "true");
        CHECK(o.optString("n", "def") == "def" && o.optString("missing", "def") == "def" && o.optString("missing") == "");
        CHECK(o.optString("o") == "{\"k\":[1,2]}");  // numberToString drops the ".0"
        CHECK(o.optString("a") == "[\"q\"]");
        CHECK(o.has("n") && o.isNull("n") && o.isNull("missing") && !o.isNull("s") && !o.has("missing"));
        CHECK(o.length() == 10);
    }

    // ---- getInt / optInt
    {
        JSONObject o = O("{\"a\":12,\"b\":12.9,\"c\":-12.9,\"d\":1e20,\"e\":\"12\",\"f\":\"12.0\",\"g\":\"x\",\"h\":true,\"i\":null,"
                         "\"j\":2147483648,\"k\":3000000000,\"l\":-2147483648,\"m\":\" 7\",\"n\":\"+5\",\"p\":[1],\"q\":-1e20,\"r\":1e999}");
        CHECK(o.getInt("a") == 12 && o.getInt("b") == 12 && o.getInt("c") == -12);
        CHECK(o.getInt("d") == INT_MAX && o.getInt("q") == INT_MIN);     // Double.intValue saturates
        CHECK(o.getInt("e") == 12 && o.getInt("n") == 5);                // Integer.parseInt
        THROWS(o.getInt("f"));
        THROWS(o.getInt("g"));
        THROWS(o.getInt("h"));
        THROWS(o.getInt("i"));
        THROWS(o.getInt("m"));                                            // parseInt does not trim
        THROWS(o.getInt("p"));
        CHECK(o.getInt("j") == INT_MIN);                                  // Long.intValue wraps
        CHECK(o.getInt("k") == -1294967296);
        CHECK(o.getInt("l") == INT_MIN);
        THROWS(o.getInt("r"));                                            // 1e999 became the STRING "1e999"
        CHECK(o.optString("r") == "1e999");
        CHECK(o.optInt("a", 9) == 12 && o.optInt("g", 9) == 9 && o.optInt("missing", 9) == 9 && o.optInt("i", 9) == 9 && o.optInt("h") == 0);
    }

    // ---- getDouble / optDouble
    {
        JSONObject o = O("{\"a\":1.5,\"b\":3,\"c\":\"2.5\",\"d\":\"abc\",\"e\":true,\"f\":null,\"g\":\" 4 \",\"h\":\"NaN\",\"i\":-0,\"j\":12345678901,\"k\":\"1e3\"}");
        CHECK(o.getDouble("a") == 1.5 && o.getDouble("b") == 3.0 && o.getDouble("c") == 2.5 && o.getDouble("j") == 12345678901.0);
        CHECK(o.getDouble("g") == 4.0 && o.getDouble("k") == 1000.0);     // Double.parseDouble trims
        CHECK(std::isnan(o.getDouble("h")));
        CHECK(std::signbit(o.getDouble("i")) && o.getDouble("i") == 0.0);  // "-0" is a Double -0.0
        THROWS(o.getDouble("d"));
        THROWS(o.getDouble("e"));
        THROWS(o.getDouble("f"));
        THROWS(o.getDouble("missing"));
        CHECK(o.optDouble("a", -1) == 1.5 && o.optDouble("d", -1) == -1 && o.optDouble("e", -1) == -1 && o.optDouble("f", -1) == -1 && o.optDouble("missing", -1) == -1);
        CHECK(std::isnan(o.optDouble("missing")));                         // default is NaN
    }

    // ---- getBoolean / optBoolean
    {
        JSONObject o = O("{\"a\":true,\"b\":false,\"c\":\"TRUE\",\"d\":\"False\",\"e\":\"yes\",\"f\":1,\"g\":null}");
        CHECK(o.getBoolean("a") && !o.getBoolean("b") && o.getBoolean("c") && !o.getBoolean("d"));
        THROWS(o.getBoolean("e"));
        THROWS(o.getBoolean("f"));
        THROWS(o.getBoolean("g"));
        CHECK(o.optBoolean("e", true) == true && o.optBoolean("f") == false && o.optBoolean("missing", true) && o.optBoolean("a"));
    }

    // ---- nested accessors
    {
        JSONObject o = O("{\"o\":{\"x\":1},\"a\":[1,[2],{\"y\":2}],\"s\":\"t\"}");
        CHECK(o.optJSONObject("o") && o.optJSONObject("o")->getInt("x") == 1);
        CHECK(o.optJSONObject("a") == nullptr && o.optJSONObject("s") == nullptr && o.optJSONObject("missing") == nullptr);
        CHECK(o.optJSONArray("a") && o.optJSONArray("a")->length() == 3 && o.optJSONArray("o") == nullptr && o.optJSONArray("s") == nullptr);
        THROWS(o.getJSONObject("a"));
        THROWS(o.getJSONArray("o"));
        THROWS(o.getJSONObject("missing"));
        const JSONArray& a = o.getJSONArray("a");
        CHECK(a.getInt(0) == 1 && a.getJSONArray(1).getInt(0) == 2 && a.getJSONObject(2).getInt("y") == 2);
        CHECK(a.optJSONObject(0) == nullptr && a.optJSONArray(1) != nullptr && a.optJSONObject(7) == nullptr && a.optJSONArray(-1) == nullptr);
        THROWS(a.getString(0));
        THROWS(a.getInt(-1));
        CHECK(a.optInt(1, 5) == 5 && a.optString(0) == "1" && a.optString(99, "d") == "d" && a.optDouble(0, 0) == 1.0 && !a.optBoolean(0));
        CHECK(a.isNull(99) && !a.isNull(0));
        CHECK(JSONArray("[null,1]").isNull(0) && JSONArray("[null,1]").optString(0, "d") == "d");
    }

    // ---- strings and escapes
    {
        CHECK(O("{\"s\":\"a\\\"b\\\\c\\/d\\b\\f\\n\\r\\t\"}").getString("s") == "a\"b\\c/d\b\f\n\r\t");
        CHECK(O("{\"s\":\"\\u0041\\u00e9\\u20AC\"}").getString("s") == "A\xC3\xA9\xE2\x82\xAC");
        CHECK(O("{\"s\":\"\\ud83d\\ude00\"}").getString("s") == "\xF0\x9F\x98\x80");            // surrogate pair
        CHECK(O("{\"s\":\"\\ud83dx\"}").getString("s") == "\xEF\xBF\xBDx");                      // lone high half
        CHECK(O("{\"s\":\"\\ude00\"}").getString("s") == "\xEF\xBF\xBD");                         // lone low half
        CHECK(O("{\"s\":\"\\ud83d\\ue000\"}").getString("s") == "\xEF\xBF\xBD" "\xEE\x80\x80");   // high half then U+E000 (not a low half)
        CHECK(O("{\"s\":\"\\ud83d\\u0041\"}").getString("s") == "\xEF\xBF\xBD" "A");              // high half then a normal escape
        CHECK(O("{\"s\":\"\xE3\x83\x86 raw\"}").getString("s") == "\xE3\x83\x86 raw");           // raw UTF-8 passes through
        CHECK(O("{\"\\u0061\":1}").getInt("a") == 1);                                              // escapes in keys
        THROWS(O("{\"s\":\"a\nb\"}"));                                                            // raw newline
        THROWS(O("{\"s\":\"a\rb\"}"));
        CHECK(O("{\"s\":\"a\tb\"}").getString("s") == "a\tb");                                    // raw tab is accepted
        THROWS(O("{\"s\":\"\\x\"}"));
        THROWS(O("{\"s\":\"\\u12\"}"));
        THROWS(O("{\"s\":\"\\u12G4\"}"));
        THROWS(O("{\"s\":\"abc"));
        THROWS(O("{\"s\":\"abc\\"));
    }

    // ---- numbers
    {
        JSONObject o = O("{\"a\":0,\"b\":-7,\"c\":2147483647,\"d\":2147483648,\"e\":-2147483649,\"f\":9223372036854775807,\"g\":9223372036854775808,"
                         "\"h\":99999999999999999999,\"i\":0.5,\"j\":-0.0,\"k\":1E2,\"l\":1e-2,\"m\":-1e999,\"n\":1.7976931348623157E308}");
        CHECK(o.optString("a") == "0" && o.optString("b") == "-7" && o.optString("c") == "2147483647");
        CHECK(o.optString("d") == "2147483648" && o.optString("e") == "-2147483649" && o.optString("f") == "9223372036854775807");
        CHECK(o.optString("g") == "9.223372036854776E18");               // beyond Long: a Double here (BigInteger in Java)
        CHECK(o.optString("h") == "1.0E20");
        CHECK(o.optString("i") == "0.5" && o.optString("j") == "-0.0" && o.optString("k") == "100.0" && o.optString("l") == "0.01");
        CHECK(o.optString("m") == "-1e999");                              // text, like org.json
        CHECK(o.optString("n") == "1.7976931348623157E308");
        THROWS(O("{\"a\":01}"));
        THROWS(O("{\"a\":-}"));
        THROWS(O("{\"a\":1.}"));
        THROWS(O("{\"a\":.5}"));
        THROWS(O("{\"a\":1e}"));
        THROWS(O("{\"a\":+1}"));
        THROWS(O("{\"a\":0x10}"));
        THROWS(O("{\"a\":NaN}"));
        THROWS(O("{\"a\":tru}"));
        THROWS(O("{\"a\":True}"));
    }

    // ---- structure rules (strict JSON on purpose)
    THROWS(O("{'a':1}"));
    THROWS(O("{a:1}"));
    THROWS(O("{\"a\" 1}"));
    THROWS(O("{\"a\"=1}"));
    THROWS(O("{\"a\":1;\"b\":2}"));
    THROWS(O("{\"a\":1,}"));
    THROWS(O("{,}"));
    THROWS(JSONArray("[1,]"));
    THROWS(JSONArray("[,1]"));
    THROWS(JSONArray("[1 2]"));
    THROWS(JSONArray("[1;2]"));
    {
        std::string deep500(500, '['), close500(500, ']');
        JSONArray ok(deep500 + close500);                                // depth 500 is fine
        CHECK(ok.length() == 1);
        std::string deep600(600, '['), close600(600, ']');
        THROWS(JSONArray(deep600 + close600));                           // deeper than 512 throws instead of overflowing the stack
        std::string deepObj;
        for (int i = 0; i < 600; i++) deepObj += "{\"a\":";
        deepObj += "1";
        for (int i = 0; i < 600; i++) deepObj += "}";
        THROWS(JSONObject(deepObj));
    }

    // ---- toString / quoting
    {
        JSONObject o = O("{\"b\":[true,null,\"x\\n\"],\"a\":1,\"c\":1.5,\"d\":2.0,\"e\":\"</b>\",\"f\":\"\\u0080\\u2028\\u0001\",\"g\":\"q\\\"\\\\\"}");
        CHECK(o.toString() == "{\"a\":1,\"b\":[true,null,\"x\\n\"],\"c\":1.5,\"d\":2,\"e\":\"<\\/b>\",\"f\":\"\\u0080\\u2028\\u0001\",\"g\":\"q\\\"\\\\\"}");
        CHECK(JSONArray("[]").toString() == "[]" && JSONObject("{}").toString() == "{}");
        CHECK(JSONArray("[1e7,1e-5,123456789012]").toString() == "[1.0E7,1.0E-5,123456789012]");
        CHECK(o.keys().size() == 7 && o.keys()[0] == "a");
    }

    // ---- JavaCompat additions
    CHECK(java::doubleToString(1.0) == "1.0" && java::doubleToString(0.1) == "0.1" && java::doubleToString(1e7) == "1.0E7");
    CHECK(java::doubleToString(-0.0) == "-0.0" && java::doubleToString(1.0 / 3) == "0.3333333333333333");
    CHECK(java::doubleToString(1e-3) == "0.001" && java::doubleToString(9.999e-4) == "9.999E-4");
    CHECK(java::parseDouble(" 2.5 ") == 2.5 && java::parseDouble("1e3") == 1000.0 && java::parseDouble("1.5d") == 1.5);
    CHECK(std::isinf(java::parseDouble("1e400")) && java::parseDouble("1e-400") == 0.0);
    bool nfe = false;
    try { java::parseDouble("1e"); } catch (const java::NumberFormatException&) { nfe = true; }
    CHECK(nfe);
    CHECK(java::parseFloat("1.1") == 1.1f && java::floatToString(0.1f) == "0.1" && java::floatToString(1e7f) == "1.0E7");  // float versions unchanged

    std::printf(failures ? "batch20: %d FAILED\n" : "batch20: all ok\n", failures);
    return failures ? 1 : 0;
}
