/*
 * json.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#define JSMN_STATIC // keep jsmn symbols translation-unit-local
#include "jsmn.h"

#include "json.h"

#include "zerom2m/compat/utils.h"
#include <circle/string.h>

namespace zerom2m::serde::json
{

JsonValue::JsonValue()
    : type_(JSON_NULL)
{
}

JsonValue::JsonValue(boolean booleanValue)
    : type_(JSON_BOOLEAN)
    , booleanValue_(booleanValue)
{
}

JsonValue::JsonValue(double numberValue)
    : type_(JSON_NUMBER)
    , numberValue_(numberValue)
{
}

JsonValue::JsonValue(const char *str)
    : type_(JSON_STRING)
    , stringValue_(str ? str : "")
{
}

JsonValue::JsonValue(const CString &str)
    : type_(JSON_STRING)
    , stringValue_(str)
{
}

JsonValue::JsonValue(StringView sv)
    : type_(JSON_STRING)
    , stringValue_(StringViewToCString(sv))
{
}

JsonValue::JsonValue(JsonType type)
    : type_((type == JSON_ARRAY || type == JSON_OBJECT) ? type : JSON_NULL)
{
}

JsonValue::~JsonValue()
{
    for (size_t i = 0; i < ArrayValue_.size(); ++i)
        delete ArrayValue_[i];

    for (size_t i = 0; i < ObjectValue_.size(); ++i)
        delete ObjectValue_[i].Value;
}

// Typed accessors
Optional<boolean> JsonValue::GetBoolean() const
{
    if (type_ != JSON_BOOLEAN) return {};
    return Optional<boolean>(booleanValue_);
}

Optional<double> JsonValue::GetNumber() const
{
    if (type_ != JSON_NUMBER) return {};
    return Optional<double>(numberValue_);
}

const CString *JsonValue::GetString() const
{ return (type_ == JSON_STRING) ? &stringValue_ : nullptr; }

// Array operations
void JsonValue::AppendElement(JsonValue *pValue)
{
    if (type_ != JSON_ARRAY || !pValue) return;
    ArrayValue_.push_back(pValue);
}

size_t JsonValue::GetArraySize() const { return (type_ == JSON_ARRAY) ? ArrayValue_.size() : 0u; }

JsonValue *JsonValue::GetElement(size_t idx) const
{
    if (type_ != JSON_ARRAY || idx >= ArrayValue_.size()) return nullptr;
    return ArrayValue_[idx];
}

// Object operations
void JsonValue::AddMember(const char *pKey, JsonValue *pValue)
{
    if (type_ != JSON_OBJECT || !pKey || !pValue) return;
    ObjectValue_.push_back(JsonObjectEntry(pKey, pValue));
}

void JsonValue::AddMember(StringView key, JsonValue *pValue)
{
    if (type_ != JSON_OBJECT || !key.Data || !pValue) return;
    CString k = StringViewToCString(key);
    ObjectValue_.push_back(JsonObjectEntry((const char *)k, pValue));
}

size_t JsonValue::GetMemberCount() const
{ return (type_ == JSON_OBJECT) ? ObjectValue_.size() : 0u; }

JsonValue *JsonValue::GetMember(const char *pKey) const
{
    if (type_ != JSON_OBJECT || !pKey) return nullptr;
    for (size_t i = 0; i < ObjectValue_.size(); ++i)
        if (strcmp((const char *)ObjectValue_[i].Key, pKey) == 0) return ObjectValue_[i].Value;
    return nullptr;
}

JsonValue *JsonValue::GetMember(StringView key) const
{
    if (type_ != JSON_OBJECT || !key.Data) return nullptr;
    for (size_t i = 0; i < ObjectValue_.size(); ++i)
        if (StringViewEquals(key, (const char *)ObjectValue_[i].Key)) return ObjectValue_[i].Value;
    return nullptr;
}

// Serialisation helpers

// Appends a properly escaped JSON string literal (including surrounding quotes)
// to out.
void JsonValue::SerializeString(CString &out, const char *str) const
{
    out.Append("\"");
    for (const char *p = str; *p; ++p) {
        char c = *p;
        switch (c) {
            case '"':
                out.Append("\\\"");
                break;
            case '\\':
                out.Append("\\\\");
                break;
            case '\b':
                out.Append("\\b");
                break;
            case '\f':
                out.Append("\\f");
                break;
            case '\n':
                out.Append("\\n");
                break;
            case '\r':
                out.Append("\\r");
                break;
            case '\t':
                out.Append("\\t");
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    CString hex;
                    hex.Format("\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out.Append((const char *)hex);
                } else {
                    char buf[2] = {c, '\0'};
                    out.Append(buf);
                }
                break;
        }
    }
    out.Append("\"");
}

void JsonValue::AppendIndent(CString &out, u8 depth) const
{
    for (unsigned i = 0; i < depth; ++i)
        out.Append("  ");
}

CString JsonValue::Serialize() const
{
    CString out;

    switch (type_) {
        case JSON_NULL:
            out.Append("null");
            break;
        case JSON_BOOLEAN:
            out.Append(booleanValue_ ? "true" : "false");
            break;
        case JSON_NUMBER: {
            CString n = DoubleToCString(numberValue_);
            out.Append((const char *)n);
            break;
        }
        case JSON_STRING:
            SerializeString(out, (const char *)stringValue_);
            break;
        case JSON_ARRAY: {
            out.Append("[");
            for (size_t i = 0; i < ArrayValue_.size(); ++i) {
                if (i > 0) out.Append(",");
                CString child = ArrayValue_[i]->Serialize();
                out.Append((const char *)child);
            }
            out.Append("]");
            break;
        }
        case JSON_OBJECT: {
            out.Append("{");
            for (size_t i = 0; i < ObjectValue_.size(); ++i) {
                if (i > 0) out.Append(",");
                SerializeString(out, (const char *)ObjectValue_[i].Key);
                out.Append(":");
                CString child = ObjectValue_[i].Value->Serialize();
                out.Append((const char *)child);
            }
            out.Append("}");
            break;
        }
    }
    return out;
}

CString JsonValue::SerializePretty(u8 depth) const
{
    CString out;

    switch (type_) {
        case JSON_NULL:
            out.Append("null");
            break;
        case JSON_BOOLEAN:
            out.Append(booleanValue_ ? "true" : "false");
            break;
        case JSON_NUMBER: {
            CString n = DoubleToCString(numberValue_);
            out.Append((const char *)n);
            break;
        }
        case JSON_STRING:
            SerializeString(out, (const char *)stringValue_);
            break;
        case JSON_ARRAY: {
            if (ArrayValue_.empty()) {
                out.Append("[]");
                break;
            }
            out.Append("[\n");
            for (size_t i = 0; i < ArrayValue_.size(); ++i) {
                if (i > 0) out.Append(",\n");
                AppendIndent(out, depth + 1);
                CString child = ArrayValue_[i]->SerializePretty(depth + 1);
                out.Append((const char *)child);
            }
            out.Append("\n");
            AppendIndent(out, depth);
            out.Append("]");
            break;
        }
        case JSON_OBJECT: {
            if (ObjectValue_.empty()) {
                out.Append("{}");
                break;
            }
            out.Append("{\n");
            for (size_t i = 0; i < ObjectValue_.size(); ++i) {
                if (i > 0) out.Append(",\n");
                AppendIndent(out, depth + 1);
                SerializeString(out, (const char *)ObjectValue_[i].Key);
                out.Append(": ");
                CString child = ObjectValue_[i].Value->SerializePretty(depth + 1);
                out.Append((const char *)child);
            }
            out.Append("\n");
            AppendIndent(out, depth);
            out.Append("}");
            break;
        }
    }
    return out;
}

JsonValue *JsonDocument::Parse(const char *jsonText)
{
    if (!jsonText) return nullptr;

    jsmn_parser parser;
    jsmn_init(&parser);

    size_t len = strlen(jsonText);

    // First pass, count tokens required
    int neededTokens = jsmn_parse(&parser, jsonText, len, nullptr, 0);
    if (neededTokens <= 0) return nullptr;
    unsigned tokensCount = static_cast<unsigned>(neededTokens);
    if (tokensCount > JSON_MAX_TOKENS) tokensCount = JSON_MAX_TOKENS;

    jsmntok_t *tokens = new jsmntok_t[tokensCount];

    jsmn_init(&parser);
    int resultCount = jsmn_parse(&parser, jsonText, len, tokens, tokensCount);

    JsonValue *value = nullptr;
    if (resultCount > 0) {
        int idx = 0;
        value   = ParseValue(jsonText, tokens, resultCount, idx);
    }

    delete[] tokens;
    return value;
}

JsonValue *JsonDocument::Parse(StringView sv)
{
    if (!sv.Data || sv.Length == 0) return nullptr;

    // Copy to a null-terminated buffer for jsmn
    char *buffer = new char[sv.Length + 1];
    for (size_t i = 0; i < sv.Length; ++i)
        buffer[i] = sv.Data[i];
    buffer[sv.Length] = '\0';

    JsonValue *value = Parse(buffer);
    delete[] buffer;
    return value;
}

// Recursive descent
JsonValue *JsonDocument::ParseValue(const char *jsonText, const void *tokens, int count, int &idx)
{
    if (idx >= count) return nullptr;

    const jsmntok_t *toks = static_cast<const jsmntok_t *>(tokens);
    const jsmntok_t &tok  = toks[idx];

    switch (tok.type) {
        case JSMN_PRIMITIVE: {
            CString     raw = ExtractToken(jsonText, tok.start, tok.end);
            const char *p   = (const char *)raw;
            idx++;

            if (strcmp(p, "null") == 0) return new JsonValue();
            if (strcmp(p, "true") == 0) return new JsonValue(static_cast<boolean>(true));
            if (strcmp(p, "false") == 0) return new JsonValue(static_cast<boolean>(false));

            return new JsonValue(ParseDouble(p));
        }
        case JSMN_STRING: {
            CString val = ExtractToken(jsonText, tok.start, tok.end);
            idx++;
            return new JsonValue((const char *)val);
        }
        case JSMN_ARRAY: {
            int childrenCount = tok.size;
            idx++; // consume array token

            JsonValue *Array = new JsonValue(JSON_ARRAY);
            Array->ArrayValue_.reserve(static_cast<size_t>(childrenCount));

            for (int i = 0; i < childrenCount; ++i) {
                if (idx >= count) break;
                JsonValue *element = ParseValue(jsonText, toks, count, idx);
                if (element) Array->AppendElement(element);
            }
            return Array;
        }

        // Object
        case JSMN_OBJECT: {
            int pairs = tok.size;
            idx++; // consume object token

            JsonValue *obj = new JsonValue(JSON_OBJECT);
            obj->ObjectValue_.reserve(static_cast<size_t>(pairs));

            for (int i = 0; i < pairs; ++i) {
                if (idx >= count) break;

                // Key must be a string token
                if (toks[idx].type != JSMN_STRING) break;
                CString key = ExtractToken(jsonText, toks[idx].start, toks[idx].end);
                idx++;

                if (idx >= count) break;

                JsonValue *pVal = ParseValue(jsonText, toks, count, idx);
                if (pVal) obj->AddMember((const char *)key, pVal);
            }
            return obj;
        }

        default:
            idx++;
            return nullptr;
    }
}

// Helpers
CString JsonDocument::ExtractToken(const char *jsonText, int start, int end)
{
    int len = end - start;
    if (len <= 0) return CString("");

    char *buffer = new char[len + 1];
    for (int i = 0; i < len; ++i)
        buffer[i] = jsonText[start + i];
    buffer[len] = '\0';

    CString str(buffer);
    delete[] buffer;
    return str;
}

double JsonDocument::ParseDouble(const char *p)
{
    double d   = 0.0;
    bool   neg = false;

    if (*p == '-') {
        neg = true;
        ++p;
    }

    while (*p >= '0' && *p <= '9')
        d = d * 10.0 + static_cast<double>(*p++ - '0');

    if (*p == '.') {
        ++p;
        double frac = 0.1;
        while (*p >= '0' && *p <= '9') {
            d += static_cast<double>(*p++ - '0') * frac;
            frac *= 0.1;
        }
    }

    if (*p == 'e' || *p == 'E') {
        ++p;
        bool negExp = false;
        if (*p == '-') {
            negExp = true;
            ++p;
        } else if (*p == '+') {
            ++p;
        }
        int exp = 0;
        while (*p >= '0' && *p <= '9')
            exp = exp * 10 + (*p++ - '0');
        double mult = 1.0;
        for (int i = 0; i < exp; ++i)
            mult *= 10.0;
        if (negExp) d /= mult;
        else d *= mult;
    }
    return neg ? -d : d;
}

} // namespace zerom2m::serde::json
