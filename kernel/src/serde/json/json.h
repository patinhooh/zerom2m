/*
 * json.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */

#pragma once

#include "zerom2m/compat/types.h"
#include "zerom2m/http/http_common.h"
#define JSMN_HEADER
#include "jsmn.h"

namespace zerom2m::serde::json
{

using namespace zerom2m::compat;

// Hard cap on jsmn tokens;
static const unsigned JSON_MAX_TOKENS =
    zerom2m::http::MAX_CONTENT_SIZE / 4u; // heuristic: 1 token per ~4 chars

enum JsonType {
    JSON_NULL    = 0,
    JSON_BOOLEAN = 1,
    JSON_NUMBER  = 2,
    JSON_STRING  = 3,
    JSON_ARRAY   = 4,
    JSON_OBJECT  = 5
};

class JsonValue; // forward declaration

struct JsonObjectEntry {
    CString    Key;
    JsonValue *Value{nullptr};

    JsonObjectEntry() = default;
    JsonObjectEntry(const char *k, JsonValue *v)
        : Key(k)
        , Value(v)
    {
    }
};

/**
 * Stored by value inside Vector<JsonObjectEntry>.
 * The owning JsonValue is responsible for deleting all pValue pointers.
 * Copy semantics here are shallow (pointer copy), only the enclosing JsonValue may trigger a copy
 * via Vector resize, and it is non-copyable itself so no double-free can occur.
 */
class JsonValue
{
public:
    // Constructors

    JsonValue();                              // JSON null
    explicit JsonValue(boolean booleanValue); // JSON boolean
    explicit JsonValue(double numberValue);   // JSON number
    explicit JsonValue(const char *str);      // JSON string (copies)
    explicit JsonValue(const CString &str);   // JSON string (copies)
    explicit JsonValue(StringView sv);        // JSON string (copies via StringView)
    explicit JsonValue(JsonType type);        // JSON_ARRAY or JSON_OBJECT (empty)

    ~JsonValue();

    JsonType GetType() const { return type_; }

    // Typed accessors
    // Optional is empty when the value's type doesn't match the accessor.
    Optional<boolean> GetBoolean() const;
    Optional<double>  GetNumber() const;

    // Returns a pointer into the internal CString, valid as long as this JsonValue is alive and
    // unmodified. nullptr if not JSON_STRING.
    const CString *GetString() const;

    // JSON_ARRAY

    void       AppendElement(JsonValue *v); // AppendElement takes ownership of value.
    size_t     GetArraySize() const;
    JsonValue *GetElement(size_t idx) const; // nullptr if index out of range

    // Raw access for advanced iteration
    const Vector<JsonValue *> &GetElements() const { return ArrayValue_; }

    // JSON_OBJECT

    void       AddMember(const char *k, JsonValue *v); // AddMember takes ownership of value.
    void       AddMember(StringView k, JsonValue *v);  // AddMember takes ownership of value.
    size_t     GetMemberCount() const;
    JsonValue *GetMember(const char *k) const; // nullptr if key not found
    JsonValue *GetMember(StringView k) const;  // nullptr if key not found

    // Raw access for iteration: for (auto& e : obj->GetMembers()) { e.Key ... }
    const Vector<JsonObjectEntry> &GetMembers() const { return ObjectValue_; }

    //
    // Serialisation

    CString Serialize() const;
    CString SerializePretty(u8 depth = 2) const;

private:
    void SerializeString(CString &out, const char *str) const;
    void AppendIndent(CString &out, u8 depth) const;

    JsonType type_;

    // Scalar storage (only one is meaningful at a time)
    boolean booleanValue_{false};
    double  numberValue_{0.0};
    CString stringValue_;

    // Composite storage
    Vector<JsonValue *>     ArrayValue_;  // JSON_ARRAY
    Vector<JsonObjectEntry> ObjectValue_; // JSON_OBJECT

    // JsonDocument::ParseValue needs access to call reserve() on private containers
    friend class JsonDocument;

    // Non-copyable — children are owned raw pointers
    JsonValue(const JsonValue &)            = delete;
    JsonValue &operator=(const JsonValue &) = delete;
};

// Static parser. Drives jsmn to turn a JSON text into a JsonValue tree.
class JsonDocument
{
public:
    // Returns the root JsonValue (caller owns it) or nullptr on error.
    static JsonValue *Parse(const char *jsonText);
    static JsonValue *Parse(StringView sv);

private:
    static JsonValue *ParseValue(const char *jsonText,
                                 const void *tokens, // jsmntok_t*
                                 int         count,
                                 int        &idx);

    static CString ExtractToken(const char *jsonText, int start, int end);
    static double  ParseDouble(const char *str);
};

} // namespace zerom2m::serde::json
