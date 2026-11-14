#pragma once
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace opsu {
namespace json {

/**
 * Small JSON reader with the accessor semantics of org.json (the stleary reference implementation that the Java download
 * servers use): JSONObject / JSONArray with get* (throw) and opt* (default) accessors. Port of the subset the servers and
 * DownloadServer use; there is no org.json source in the core, so this is checked by tests/test_batch20.cpp, not by a JVM golden.
 *
 * Accessor rules (org.json 2018-2020):
 *  - getString: only a JSON string (a number is NOT converted: JSONException). optString converts anything but null / missing
 *    (numbers with Java's toString: 12 -> "12", 1.0 -> "1.0", true -> "true"; containers -> compact JSON).
 *  - getInt / optInt: a number (Double -> Java (int) cast, saturating) or a string that Integer.parseInt accepts ("12", not "12.0").
 *  - getDouble / optDouble: a number or a string Double.parseDouble accepts. optDouble's default is NaN like Java's overload without one.
 *  - getBoolean / optBoolean: true / false, or the strings "true" / "false" (any case).
 *  - JSON null is kept as a value (has() is true, isNull() is true); opt* give the default for it, get* throw.
 *  - Numbers are Integer when they fit in 32 bits, Long in 64 bits, else Double (Java: BigInteger / BigDecimal).
 *    "-0" and anything with '.', 'e', 'E' is a Double; a double that overflows to Infinity becomes the STRING of its text (org.json quirk).
 *  - Duplicate keys in an object throw JSONException. Text after the closing bracket of the top-level value is ignored.
 *
 * Deliberate deviations (org.json's JSONTokener is very lenient; this parser is strict JSON):
 *  - no single quotes, unquoted keys / strings, `=` / `=>` / `;` separators, trailing or empty elements (`[1,,2]`, `[1,]`),
 *    leading-zero numbers or `//` comments: those are JSONException here. The servers talk to JSON APIs that emit valid JSON.
 *  - nesting deeper than 512 is a JSONException (Java would overflow the stack).
 *  - strings are UTF-8; a lone \\uD800-\\uDFFF escape becomes U+FFFD (a Java String could hold the lone surrogate).
 *  - key order is sorted (java.util.HashMap order is unspecified).
 *  - getLong / optLong, put*, remove, names and friends are not ported (nothing uses them).
 */
struct JSONException : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class JSONObject;
class JSONArray;
struct Null {};  // JSONObject.NULL

using Value = std::variant<Null, bool, int32_t, int64_t, double, std::string, std::shared_ptr<JSONArray>, std::shared_ptr<JSONObject>>;

/** A JSON object. */
class JSONObject {
public:
    JSONObject() = default;
    /** new JSONObject(String): the text must start (after whitespace) with '{'. @throws JSONException */
    explicit JSONObject(const std::string& text);

    bool has(const std::string& key) const { return map_.count(key) != 0; }
    int length() const { return static_cast<int>(map_.size()); }
    std::vector<std::string> keys() const;
    /** True when the key is missing or its value is JSON null. */
    bool isNull(const std::string& key) const;

    std::string getString(const std::string& key) const;
    int getInt(const std::string& key) const;
    double getDouble(const std::string& key) const;
    bool getBoolean(const std::string& key) const;
    const JSONObject& getJSONObject(const std::string& key) const;
    const JSONArray& getJSONArray(const std::string& key) const;

    std::string optString(const std::string& key, const std::string& def = "") const;
    int optInt(const std::string& key, int def = 0) const;
    double optDouble(const std::string& key, double def = std::numeric_limits<double>::quiet_NaN()) const;
    bool optBoolean(const std::string& key, bool def = false) const;
    /** The nested object, or nullptr (Java null) when missing or not an object. Valid while this object lives. */
    const JSONObject* optJSONObject(const std::string& key) const;
    const JSONArray* optJSONArray(const std::string& key) const;

    /** Compact JSON text (keys sorted). */
    std::string toString() const;

    // building (used by the parser and tests)
    void set(const std::string& key, Value v) { map_[key] = std::move(v); }
    const Value* opt(const std::string& key) const;

private:
    std::map<std::string, Value> map_;
};

/** A JSON array. */
class JSONArray {
public:
    JSONArray() = default;
    /** new JSONArray(String): the text must start (after whitespace) with '['. @throws JSONException */
    explicit JSONArray(const std::string& text);

    int length() const { return static_cast<int>(items_.size()); }
    bool isNull(int i) const;

    std::string getString(int i) const;
    int getInt(int i) const;
    double getDouble(int i) const;
    bool getBoolean(int i) const;
    const JSONObject& getJSONObject(int i) const;
    const JSONArray& getJSONArray(int i) const;

    std::string optString(int i, const std::string& def = "") const;
    int optInt(int i, int def = 0) const;
    double optDouble(int i, double def = std::numeric_limits<double>::quiet_NaN()) const;
    bool optBoolean(int i, bool def = false) const;
    const JSONObject* optJSONObject(int i) const;
    const JSONArray* optJSONArray(int i) const;

    std::string toString() const;

    void add(Value v) { items_.push_back(std::move(v)); }
    const Value* opt(int i) const { return (i >= 0 && i < length()) ? &items_[static_cast<size_t>(i)] : nullptr; }

private:
    std::vector<Value> items_;
};

}  // namespace json
}  // namespace opsu
