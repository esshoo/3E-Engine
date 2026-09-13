#include "engine/data/json_value.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <utility>

namespace threee::data {

const JsonValue* JsonValue::Find(std::string_view key) const {
    if (!IsObject()) {
        return nullptr;
    }

    auto it = objectValue.find(std::string(key));
    return it == objectValue.end() ? nullptr : &it->second;
}

std::string JsonValue::GetString(std::string_view key, std::string defaultValue) const {
    const JsonValue* value = Find(key);
    return value && value->IsString() ? value->stringValue : std::move(defaultValue);
}

bool JsonValue::GetBool(std::string_view key, bool defaultValue) const {
    const JsonValue* value = Find(key);
    return value && value->IsBool() ? value->boolValue : defaultValue;
}

double JsonValue::GetNumber(std::string_view key, double defaultValue) const {
    const JsonValue* value = Find(key);
    return value && value->IsNumber() ? value->numberValue : defaultValue;
}

namespace {

class Parser {
public:
    explicit Parser(std::string_view source)
        : m_source(source) {
    }

    JsonDocument Run() {
        JsonDocument document;

        SkipWhitespace();
        document.root = ParseValue();
        SkipWhitespace();

        if (m_error.empty() && m_pos != m_source.size()) {
            SetError("unexpected trailing characters");
        }

        document.error = m_error;
        return document;
    }

private:
    std::string_view m_source;
    std::size_t m_pos = 0;
    std::string m_error;

    char Peek() const {
        return m_pos < m_source.size() ? m_source[m_pos] : '\0';
    }

    char Next() {
        return m_pos < m_source.size() ? m_source[m_pos++] : '\0';
    }

    void SkipWhitespace() {
        while (m_pos < m_source.size() &&
               std::isspace(static_cast<unsigned char>(m_source[m_pos]))) {
            ++m_pos;
        }
    }

    void SetError(const std::string& message) {
        if (!m_error.empty()) {
            return;
        }

        std::ostringstream stream;
        stream << message << " at byte " << m_pos;
        m_error = stream.str();
    }

    bool Consume(char expected) {
        SkipWhitespace();
        if (Peek() != expected) {
            SetError(std::string("expected '") + expected + "'");
            return false;
        }

        ++m_pos;
        return true;
    }

    JsonValue ParseValue() {
        SkipWhitespace();

        const char c = Peek();
        if (c == '{') return ParseObject();
        if (c == '[') return ParseArray();
        if (c == '"') return ParseStringValue();
        if (c == 't') return ParseLiteral("true", JsonValue::Type::Bool, true);
        if (c == 'f') return ParseLiteral("false", JsonValue::Type::Bool, false);
        if (c == 'n') return ParseLiteral("null", JsonValue::Type::Null, false);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return ParseNumber();

        SetError("expected JSON value");
        return {};
    }

    JsonValue ParseLiteral(
        std::string_view token,
        JsonValue::Type type,
        bool boolValue) {

        JsonValue value;

        if (m_source.substr(m_pos, token.size()) != token) {
            SetError("invalid literal");
            return value;
        }

        m_pos += token.size();
        value.type = type;
        value.boolValue = boolValue;
        return value;
    }

    static void AppendUtf8(std::string& output, unsigned int codePoint) {
        if (codePoint <= 0x7F) {
            output.push_back(static_cast<char>(codePoint));
        }
        else if (codePoint <= 0x7FF) {
            output.push_back(static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
        else if (codePoint <= 0xFFFF) {
            output.push_back(static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F)));
            output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
        else {
            output.push_back(static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07)));
            output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }

    bool ParseHex4(unsigned int& value) {
        value = 0;

        for (int i = 0; i < 4; ++i) {
            if (m_pos >= m_source.size()) {
                SetError("unterminated unicode escape");
                return false;
            }

            const char c = Next();
            value <<= 4;

            if (c >= '0' && c <= '9') value |= static_cast<unsigned int>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<unsigned int>(10 + c - 'a');
            else if (c >= 'A' && c <= 'F') value |= static_cast<unsigned int>(10 + c - 'A');
            else {
                SetError("invalid unicode escape");
                return false;
            }
        }

        return true;
    }

    std::string ParseStringRaw() {
        std::string result;

        if (!Consume('"')) {
            return result;
        }

        while (m_pos < m_source.size()) {
            const char c = Next();

            if (c == '"') {
                return result;
            }

            if (static_cast<unsigned char>(c) < 0x20) {
                SetError("control character inside string");
                return {};
            }

            if (c != '\\') {
                result.push_back(c);
                continue;
            }

            if (m_pos >= m_source.size()) {
                SetError("unterminated escape sequence");
                return {};
            }

            const char escape = Next();
            switch (escape) {
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case '/': result.push_back('/'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;

                case 'u': {
                    unsigned int high = 0;
                    if (!ParseHex4(high)) {
                        return {};
                    }

                    if (high >= 0xD800 && high <= 0xDBFF) {
                        if (m_pos + 2 > m_source.size() ||
                            m_source[m_pos] != '\\' ||
                            m_source[m_pos + 1] != 'u') {
                            SetError("missing low surrogate");
                            return {};
                        }

                        m_pos += 2;

                        unsigned int low = 0;
                        if (!ParseHex4(low)) {
                            return {};
                        }

                        if (low < 0xDC00 || low > 0xDFFF) {
                            SetError("invalid low surrogate");
                            return {};
                        }

                        const unsigned int codePoint =
                            0x10000 + (((high - 0xD800) << 10) | (low - 0xDC00));
                        AppendUtf8(result, codePoint);
                    }
                    else {
                        AppendUtf8(result, high);
                    }
                    break;
                }

                default:
                    SetError("invalid escape sequence");
                    return {};
            }
        }

        SetError("unterminated string");
        return {};
    }

    JsonValue ParseStringValue() {
        JsonValue value;
        value.type = JsonValue::Type::String;
        value.stringValue = ParseStringRaw();
        return value;
    }

    JsonValue ParseNumber() {
        const std::size_t start = m_pos;

        if (Peek() == '-') {
            ++m_pos;
        }

        if (Peek() == '0') {
            ++m_pos;
        }
        else {
            if (!std::isdigit(static_cast<unsigned char>(Peek()))) {
                SetError("invalid number");
                return {};
            }

            while (std::isdigit(static_cast<unsigned char>(Peek()))) {
                ++m_pos;
            }
        }

        if (Peek() == '.') {
            ++m_pos;

            if (!std::isdigit(static_cast<unsigned char>(Peek()))) {
                SetError("invalid fraction");
                return {};
            }

            while (std::isdigit(static_cast<unsigned char>(Peek()))) {
                ++m_pos;
            }
        }

        if (Peek() == 'e' || Peek() == 'E') {
            ++m_pos;

            if (Peek() == '+' || Peek() == '-') {
                ++m_pos;
            }

            if (!std::isdigit(static_cast<unsigned char>(Peek()))) {
                SetError("invalid exponent");
                return {};
            }

            while (std::isdigit(static_cast<unsigned char>(Peek()))) {
                ++m_pos;
            }
        }

        const std::string token(m_source.substr(start, m_pos - start));
        char* end = nullptr;
        const double parsed = std::strtod(token.c_str(), &end);

        if (!end || *end != '\0') {
            SetError("invalid number");
            return {};
        }

        JsonValue value;
        value.type = JsonValue::Type::Number;
        value.numberValue = parsed;
        return value;
    }

    JsonValue ParseArray() {
        JsonValue value;
        value.type = JsonValue::Type::Array;

        if (!Consume('[')) {
            return value;
        }

        SkipWhitespace();

        if (Peek() == ']') {
            ++m_pos;
            return value;
        }

        while (m_error.empty()) {
            value.arrayValue.push_back(ParseValue());
            SkipWhitespace();

            if (Peek() == ']') {
                ++m_pos;
                break;
            }

            if (!Consume(',')) {
                break;
            }
        }

        return value;
    }

    JsonValue ParseObject() {
        JsonValue value;
        value.type = JsonValue::Type::Object;

        if (!Consume('{')) {
            return value;
        }

        SkipWhitespace();

        if (Peek() == '}') {
            ++m_pos;
            return value;
        }

        while (m_error.empty()) {
            SkipWhitespace();

            if (Peek() != '"') {
                SetError("expected object key");
                break;
            }

            const std::string key = ParseStringRaw();

            if (!Consume(':')) {
                break;
            }

            JsonValue child = ParseValue();
            value.objectValue.insert_or_assign(key, std::move(child));

            SkipWhitespace();

            if (Peek() == '}') {
                ++m_pos;
                break;
            }

            if (!Consume(',')) {
                break;
            }
        }

        return value;
    }
};

} // namespace

JsonDocument JsonDocument::Parse(std::string_view source) {
    Parser parser(source);
    return parser.Run();
}

JsonDocument JsonDocument::LoadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);

    if (!file) {
        JsonDocument document;
        document.error = "cannot open file: " + path.string();
        return document;
    }

    const std::string source(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    return Parse(source);
}

} // namespace threee::data