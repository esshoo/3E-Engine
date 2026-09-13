#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace threee::data {

struct JsonValue {
    enum class Type {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    Type type = Type::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::vector<JsonValue> arrayValue;
    std::unordered_map<std::string, JsonValue> objectValue;

    bool IsNull() const { return type == Type::Null; }
    bool IsBool() const { return type == Type::Bool; }
    bool IsNumber() const { return type == Type::Number; }
    bool IsString() const { return type == Type::String; }
    bool IsArray() const { return type == Type::Array; }
    bool IsObject() const { return type == Type::Object; }

    const JsonValue* Find(std::string_view key) const;

    std::string GetString(std::string_view key, std::string defaultValue = {}) const;
    bool GetBool(std::string_view key, bool defaultValue = false) const;
    double GetNumber(std::string_view key, double defaultValue = 0.0) const;
};

struct JsonDocument {
    JsonValue root;
    std::string error;

    bool Ok() const { return error.empty(); }

    static JsonDocument Parse(std::string_view source);
    static JsonDocument LoadFile(const std::filesystem::path& path);
};

} // namespace threee::data