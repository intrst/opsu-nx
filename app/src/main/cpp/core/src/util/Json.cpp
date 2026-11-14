#include "opsu/util/Json.h"
#include <cmath>
#include <cstdio>
#include "opsu/util/JavaCompat.h"

namespace opsu {
namespace json {

namespace {

constexpr int MAX_DEPTH = 512;

// ------------------------------------------------------------------ parser
class Parser {
public:
    explicit Parser(const std::string& s) : s_(s) {}

    void skipWs() { while (i_ < s_.size() && static_cast<unsigned char>(s_[i_]) <= ' ') i_++; }
    bool atEnd() const { return i_ >= s_.size(); }
    char peek() const { return s_[i_]; }

    JSONException err(const std::string& m) const { return JSONException(m + " at " + std::to_string(i_)); }

    Value parseValue(int depth) {
        if (depth > MAX_DEPTH) throw err("Nesting too deep");
        skipWs();
        if (atEnd()) throw err("Unexpected end of text");
        char c = s_[i_];
        if (c == '{') return std::make_shared<JSONObject>(parseObject(depth + 1));
        if (c == '[') return std::make_shared<JSONArray>(parseArray(depth + 1));
        if (c == '"') return parseString();
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        if (s_.compare(i_, 4, "true") == 0) { i_ += 4; return true; }
        if (s_.compare(i_, 5, "false") == 0) { i_ += 5; return false; }
        if (s_.compare(i_, 4, "null") == 0) { i_ += 4; return Null{}; }
        throw err("Unexpected character");
    }

    JSONObject parseObject(int depth) {
        i_++;  // '{'
        JSONObject o;
        skipWs();
        if (!atEnd() && peek() == '}') { i_++; return o; }
        while (true) {
            skipWs();
            if (atEnd() || peek() != '"') throw err("A JSONObject key must be a string");
            std::string key = parseString();
            skipWs();
            if (atEnd() || peek() != ':') throw err("Expected a ':' after a key");
            i_++;
            if (o.has(key)) throw JSONException("Duplicate key \"" + key + "\"");
            o.set(key, parseValue(depth));
            skipWs();
            if (atEnd()) throw err("A JSONObject text must end with '}'");
            char c = s_[i_++];
            if (c == '}') return o;
            if (c != ',') throw err("Expected a ',' or '}'");
        }
    }

    JSONArray parseArray(int depth) {
        i_++;  // '['
        JSONArray a;
        skipWs();
        if (!atEnd() && peek() == ']') { i_++; return a; }
        while (true) {
            a.add(parseValue(depth));
            skipWs();
            if (atEnd()) throw err("Expected a ',' or ']'");
            char c = s_[i_++];
            if (c == ']') return a;
            if (c != ',') throw err("Expected a ',' or ']'");
        }
    }

    static void appendUtf8(std::string& out, uint32_t cp) {
        if (cp < 0x80) out.push_back(static_cast<char>(cp));
        else if (cp < 0x800) { out.push_back(static_cast<char>(0xC0 | (cp >> 6))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
        else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    uint32_t hex4() {
        if (i_ + 4 > s_.size()) throw err("Illegal escape");
        uint32_t v = 0;
        for (int k = 0; k < 4; k++) {
            char c = s_[i_++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<uint32_t>(c - 'A' + 10);
            else throw err("Illegal escape");
        }
        return v;
    }

    std::string parseString() {
        i_++;  // opening quote
        std::string out;
        while (true) {
            if (atEnd()) throw err("Unterminated string");
            char c = s_[i_++];
            if (c == '"') return out;
            if (c == '\n' || c == '\r') throw err("Unterminated string");
            if (c != '\\') { out.push_back(c); continue; }
            if (atEnd()) throw err("Unterminated string");
            char e = s_[i_++];
            switch (e) {
                case 'b': out.push_back('\b'); break;
                case 't': out.push_back('\t'); break;
                case 'n': out.push_back('\n'); break;
                case 'f': out.push_back('\f'); break;
                case 'r': out.push_back('\r'); break;
                case '"': case '\'': case '\\': case '/': out.push_back(e); break;
                case 'u': {
                    uint32_t u = hex4();
                    if (u >= 0xD800 && u <= 0xDBFF && i_ + 1 < s_.size() && s_[i_] == '\\' && s_[i_ + 1] == 'u') {
                        size_t save = i_;
                        i_ += 2;
                        uint32_t lo = hex4();
                        if (lo >= 0xDC00 && lo <= 0xDFFF) { appendUtf8(out, 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00)); break; }
                        i_ = save;  // not a pair: the high half is lone
                    }
                    appendUtf8(out, (u >= 0xD800 && u <= 0xDFFF) ? 0xFFFD : u);
                    break;
                }
                default: throw err("Illegal escape");
            }
        }
    }

    Value parseNumber() {
        size_t start = i_;
        if (s_[i_] == '-') i_++;
        if (atEnd() || s_[i_] < '0' || s_[i_] > '9') throw err("Bad number");
        if (s_[i_] == '0') {
            i_++;
            if (!atEnd() && s_[i_] >= '0' && s_[i_] <= '9') throw err("Leading zero");
        } else {
            while (!atEnd() && s_[i_] >= '0' && s_[i_] <= '9') i_++;
        }
        bool decimal = false;
        if (!atEnd() && s_[i_] == '.') {
            decimal = true;
            i_++;
            size_t d = 0;
            while (!atEnd() && s_[i_] >= '0' && s_[i_] <= '9') { i_++; d++; }
            if (d == 0) throw err("Bad number");
        }
        if (!atEnd() && (s_[i_] == 'e' || s_[i_] == 'E')) {
            decimal = true;
            i_++;
            if (!atEnd() && (s_[i_] == '+' || s_[i_] == '-')) i_++;
            size_t d = 0;
            while (!atEnd() && s_[i_] >= '0' && s_[i_] <= '9') { i_++; d++; }
            if (d == 0) throw err("Bad number");
        }
        std::string text = s_.substr(start, i_ - start);
        if (text == "-0") return -0.0;
        if (!decimal) {
            // Integer, else Long, else (Java: BigInteger) Double
            try {
                long long v = std::stoll(text);
                if (v >= INT32_MIN && v <= INT32_MAX) return static_cast<int32_t>(v);
                return static_cast<int64_t>(v);
            } catch (const std::out_of_range&) {
                // falls through to double
            }
        }
        double d = java::parseDouble(text);
        if (std::isinf(d) || std::isnan(d)) return text;  // org.json: stringToValue returns the text itself
        return d;
    }

private:
    const std::string& s_;
    size_t i_ = 0;
};

// ------------------------------------------------------------------ conversions (shared by object / array accessors)
JSONException wrong(const std::string& what, const char* kind) { return JSONException(what + " is not a " + kind + "."); }

std::string javaToString(const Value& v);

std::string quote(const std::string& s) {
    std::string out = "\"";
    char prev = 0;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        switch (c) {
            case '"': case '\\': out.push_back('\\'); out.push_back(static_cast<char>(c)); break;
            case '/': if (prev == '<') out.push_back('\\'); out.push_back('/'); break;
            case '\b': out += "\\b"; break;
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            case '\f': out += "\\f"; break;
            case '\r': out += "\\r"; break;
            default: {
                uint32_t cp = c;
                size_t len = 1;
                if (c >= 0xC2 && c < 0xE0 && i + 1 < s.size()) { cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu); len = 2; }
                else if (c >= 0xE0 && c < 0xF0 && i + 2 < s.size()) {
                    cp = ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) | (static_cast<unsigned char>(s[i + 2]) & 0x3Fu);
                    len = 3;
                }
                if (cp < 0x20 || (cp >= 0x80 && cp < 0xA0) || (cp >= 0x2000 && cp < 0x2100)) {
                    char buf[8]; std::snprintf(buf, sizeof buf, "\\u%04x", cp);
                    out += buf;
                } else out.append(s, i, len);
                i += len - 1;
            }
        }
        prev = static_cast<char>(c);
    }
    return out + "\"";
}

// JSONObject.numberToString: Java's toString with a trailing ".0" / zeros removed
std::string numberToString(double d) {
    if (std::isnan(d) || std::isinf(d)) return "null";
    std::string s = java::doubleToString(d);
    if (s.find('.') != std::string::npos && s.find('e') == std::string::npos && s.find('E') == std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s;
}

std::string toJson(const Value& v) {
    switch (v.index()) {
        case 0: return "null";
        case 1: return std::get<bool>(v) ? "true" : "false";
        case 2: return std::to_string(std::get<int32_t>(v));
        case 3: return std::to_string(std::get<int64_t>(v));
        case 4: return numberToString(std::get<double>(v));
        case 5: return quote(std::get<std::string>(v));
        case 6: return std::get<std::shared_ptr<JSONArray>>(v)->toString();
        default: return std::get<std::shared_ptr<JSONObject>>(v)->toString();
    }
}

// Object.toString() of a value as optString sees it
std::string javaToString(const Value& v) {
    switch (v.index()) {
        case 0: return "null";
        case 4: return java::doubleToString(std::get<double>(v));
        case 5: return std::get<std::string>(v);
        default: return toJson(v);
    }
}


std::string getStringV(const Value* v, const std::string& what) {
    if (v && v->index() == 5) return std::get<std::string>(*v);
    throw wrong(what, "string");
}

int getIntV(const Value* v, const std::string& what) {
    if (v) {
        switch (v->index()) {
            case 2: return std::get<int32_t>(*v);
            case 3: return static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(std::get<int64_t>(*v))));  // Long.intValue
            case 4: return java::toInt(std::get<double>(*v));
            case 5:
                try { return java::parseInt(std::get<std::string>(*v)); } catch (const std::exception&) { break; }
            default: break;
        }
    }
    throw wrong(what, "int");
}

double getDoubleV(const Value* v, const std::string& what) {
    if (v) {
        switch (v->index()) {
            case 2: return std::get<int32_t>(*v);
            case 3: return static_cast<double>(std::get<int64_t>(*v));
            case 4: return std::get<double>(*v);
            case 5:
                try { return java::parseDouble(std::get<std::string>(*v)); } catch (const std::exception&) { break; }
            default: break;
        }
    }
    throw wrong(what, "double");
}

bool getBoolV(const Value* v, const std::string& what) {
    if (v) {
        if (v->index() == 1) return std::get<bool>(*v);
        if (v->index() == 5) {
            std::string s = std::get<std::string>(*v);
            for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
            if (s == "true") return true;
            if (s == "false") return false;
        }
    }
    throw wrong(what, "boolean");
}

std::string optStringV(const Value* v, const std::string& def) {
    if (!v || v->index() == 0) return def;
    return javaToString(*v);
}

const JSONObject* optObj(const Value* v) { return (v && v->index() == 7) ? std::get<std::shared_ptr<JSONObject>>(*v).get() : nullptr; }
const JSONArray* optArr(const Value* v) { return (v && v->index() == 6) ? std::get<std::shared_ptr<JSONArray>>(*v).get() : nullptr; }

std::string keyName(const std::string& k) { return "JSONObject[" + quote(k) + "]"; }
std::string idxName(int i) { return "JSONArray[" + std::to_string(i) + "]"; }

const Value& need(const Value* v, const std::string& what) {
    if (!v) throw JSONException(what + " not found.");
    return *v;
}

}  // namespace

// ------------------------------------------------------------------ JSONObject
JSONObject::JSONObject(const std::string& text) {
    Parser p(text);
    p.skipWs();
    if (p.atEnd() || p.peek() != '{') throw JSONException("A JSONObject text must begin with '{'");
    *this = p.parseObject(1);
}

const Value* JSONObject::opt(const std::string& key) const {
    auto it = map_.find(key);
    return it == map_.end() ? nullptr : &it->second;
}
std::vector<std::string> JSONObject::keys() const {
    std::vector<std::string> k;
    for (const auto& kv : map_) k.push_back(kv.first);
    return k;
}
bool JSONObject::isNull(const std::string& key) const { const Value* v = opt(key); return !v || v->index() == 0; }

std::string JSONObject::getString(const std::string& k) const { need(opt(k), keyName(k)); return getStringV(opt(k), keyName(k)); }
int JSONObject::getInt(const std::string& k) const { need(opt(k), keyName(k)); return getIntV(opt(k), keyName(k)); }
double JSONObject::getDouble(const std::string& k) const { need(opt(k), keyName(k)); return getDoubleV(opt(k), keyName(k)); }
bool JSONObject::getBoolean(const std::string& k) const { need(opt(k), keyName(k)); return getBoolV(opt(k), keyName(k)); }
const JSONObject& JSONObject::getJSONObject(const std::string& k) const {
    need(opt(k), keyName(k));
    const JSONObject* o = optObj(opt(k));
    if (!o) throw wrong(keyName(k), "JSONObject");
    return *o;
}
const JSONArray& JSONObject::getJSONArray(const std::string& k) const {
    need(opt(k), keyName(k));
    const JSONArray* a = optArr(opt(k));
    if (!a) throw wrong(keyName(k), "JSONArray");
    return *a;
}

std::string JSONObject::optString(const std::string& k, const std::string& def) const { return optStringV(opt(k), def); }
int JSONObject::optInt(const std::string& k, int def) const { try { return getInt(k); } catch (const JSONException&) { return def; } }
double JSONObject::optDouble(const std::string& k, double def) const { try { return getDouble(k); } catch (const JSONException&) { return def; } }
bool JSONObject::optBoolean(const std::string& k, bool def) const { try { return getBoolean(k); } catch (const JSONException&) { return def; } }
const JSONObject* JSONObject::optJSONObject(const std::string& k) const { return optObj(opt(k)); }
const JSONArray* JSONObject::optJSONArray(const std::string& k) const { return optArr(opt(k)); }

std::string JSONObject::toString() const {
    std::string out = "{";
    bool first = true;
    for (const auto& kv : map_) {
        if (!first) out += ",";
        first = false;
        out += quote(kv.first) + ":" + toJson(kv.second);
    }
    return out + "}";
}

// ------------------------------------------------------------------ JSONArray
JSONArray::JSONArray(const std::string& text) {
    Parser p(text);
    p.skipWs();
    if (p.atEnd() || p.peek() != '[') throw JSONException("A JSONArray text must start with '['");
    *this = p.parseArray(1);
}

bool JSONArray::isNull(int i) const { const Value* v = opt(i); return !v || v->index() == 0; }

std::string JSONArray::getString(int i) const { need(opt(i), idxName(i)); return getStringV(opt(i), idxName(i)); }
int JSONArray::getInt(int i) const { need(opt(i), idxName(i)); return getIntV(opt(i), idxName(i)); }
double JSONArray::getDouble(int i) const { need(opt(i), idxName(i)); return getDoubleV(opt(i), idxName(i)); }
bool JSONArray::getBoolean(int i) const { need(opt(i), idxName(i)); return getBoolV(opt(i), idxName(i)); }
const JSONObject& JSONArray::getJSONObject(int i) const {
    need(opt(i), idxName(i));
    const JSONObject* o = optObj(opt(i));
    if (!o) throw wrong(idxName(i), "JSONObject");
    return *o;
}
const JSONArray& JSONArray::getJSONArray(int i) const {
    need(opt(i), idxName(i));
    const JSONArray* a = optArr(opt(i));
    if (!a) throw wrong(idxName(i), "JSONArray");
    return *a;
}

std::string JSONArray::optString(int i, const std::string& def) const { return optStringV(opt(i), def); }
int JSONArray::optInt(int i, int def) const { try { return getInt(i); } catch (const JSONException&) { return def; } }
double JSONArray::optDouble(int i, double def) const { try { return getDouble(i); } catch (const JSONException&) { return def; } }
bool JSONArray::optBoolean(int i, bool def) const { try { return getBoolean(i); } catch (const JSONException&) { return def; } }
const JSONObject* JSONArray::optJSONObject(int i) const { return optObj(opt(i)); }
const JSONArray* JSONArray::optJSONArray(int i) const { return optArr(opt(i)); }

std::string JSONArray::toString() const {
    std::string out = "[";
    for (size_t i = 0; i < items_.size(); i++) {
        if (i) out += ",";
        out += toJson(items_[i]);
    }
    return out + "]";
}

}  // namespace json
}  // namespace opsu
