/*
 * json.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "zerom2m/codecs/json.h"
#include "zerom2m/compat/collections.h"

#include <circle/types.h>
#include <circle/util.h>

// TODO: This is just a very basic implementation to get something working.

namespace zerom2m::codecs::json
{

using zerom2m::compat::Vector;

namespace
{
bool Equals(const char *lhs, const char *rhs)
{ return lhs != nullptr && rhs != nullptr && strcmp(lhs, rhs) == 0; }

const char *SkipWhitespace(const char *p)
{
    while (p != nullptr && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        ++p;
    }
    return p;
}
} // namespace

void AppendEscapedString(CString &out, const char *value)
{
    if (value == nullptr) { return; }

    for (const char *p = value; *p != '\0'; ++p) {
        switch (*p) {
            case '\\':
                out.Append("\\\\");
                break;
            case '"':
                out.Append("\\\"");
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
                out += *p;
                break;
        }
    }
}

void AppendStringField(CString &out, bool &first, const char *name, const char *value)
{
    if (value == nullptr || value[0] == '\0') { return; }

    if (!first) { out.Append(","); }
    first = false;

    out.Append("\"");
    out.Append(name);
    out.Append("\":\"");
    AppendEscapedString(out, value);
    out.Append("\"");
}

void AppendUnsignedField(CString &out, bool &first, const char *name, unsigned value)
{
    if (!first) { out.Append(","); }
    first = false;

    CString number;
    number.Format("%u", value);
    out.Append("\"");
    out.Append(name);
    out.Append("\":");
    out.Append(number);
}

void AppendBoolField(CString &out, bool &first, const char *name, bool value)
{
    if (!first) { out.Append(","); }
    first = false;

    out.Append("\"");
    out.Append(name);
    out.Append("\":");
    out.Append(value ? "true" : "false");
}

void AppendStringArrayField(
    CString &out, bool &first, const char *name, const char *const *values, size_t count)
{
    if (!first) { out.Append(","); }
    first = false;

    out.Append("\"");
    out.Append(name);
    out.Append("\":[");

    for (size_t i = 0; i < count; ++i) {
        if (i > 0) { out.Append(","); }

        out.Append("\"");
        AppendEscapedString(out, values[i]);
        out.Append("\"");
    }

    out.Append("]");
}

const char *FindKey(const char *json, const char *key)
{
    if (json == nullptr || key == nullptr) { return nullptr; }

    char   pattern[80];
    size_t patternLength     = 0;
    pattern[patternLength++] = '"';
    for (size_t i = 0; key[i] != '\0' && patternLength < sizeof(pattern) - 2; ++i) {
        pattern[patternLength++] = key[i];
    }
    pattern[patternLength++] = '"';
    pattern[patternLength]   = '\0';
    return strstr(json, pattern);
}

bool ExtractStringValue(const char *json, const char *key, char *buffer, size_t bufferSize)
{
    if (buffer == nullptr || bufferSize == 0) { return false; }

    const char *keyPos = FindKey(json, key);
    if (keyPos == nullptr) {
        buffer[0] = '\0';
        return false;
    }

    const char *valuePos = strchr(keyPos, ':');
    if (valuePos == nullptr) {
        buffer[0] = '\0';
        return false;
    }

    valuePos = SkipWhitespace(valuePos + 1);

    const char *valueEnd = valuePos;
    if (*valuePos == '"') {
        valuePos++;
        valueEnd = valuePos;
        while (*valueEnd != '\0' && *valueEnd != '"') {
            if (*valueEnd == '\\' && valueEnd[1] != '\0') {
                valueEnd += 2;
                continue;
            }
            ++valueEnd;
        }
    } else {
        while (*valueEnd != '\0' && *valueEnd != ',' && *valueEnd != '}' && *valueEnd != ' ' &&
               *valueEnd != '\t' && *valueEnd != '\r' && *valueEnd != '\n' && *valueEnd != ']') {
            ++valueEnd;
        }
    }

    const size_t valueLength = static_cast<size_t>(valueEnd - valuePos);
    const size_t copyLength  = valueLength < (bufferSize - 1) ? valueLength : (bufferSize - 1);
    memcpy(buffer, valuePos, copyLength);
    buffer[copyLength] = '\0';
    return true;
}

bool ExtractBoolValue(const char *json, const char *key, bool &value)
{
    char token[32]{};
    if (!ExtractStringValue(json, key, token, sizeof(token))) {
        const char *keyPos = FindKey(json, key);
        if (keyPos == nullptr) { return false; }

        const char *valuePos = strchr(keyPos, ':');
        if (valuePos == nullptr) { return false; }
        valuePos = SkipWhitespace(valuePos + 1);

        size_t i = 0;
        while (valuePos[i] != '\0' && valuePos[i] != ',' && valuePos[i] != '}' &&
               valuePos[i] != ' ' && valuePos[i] != '\t' && valuePos[i] != '\r' &&
               valuePos[i] != '\n' && i < sizeof(token) - 1) {
            token[i] = valuePos[i];
            i++;
        }
        token[i] = '\0';
    }

    if (Equals(token, "true") || Equals(token, "1")) {
        value = true;
        return true;
    }
    if (Equals(token, "false") || Equals(token, "0")) {
        value = false;
        return true;
    }

    return false;
}

bool ExtractUnsignedValue(const char *json, const char *key, unsigned &value)
{
    char token[32]{};
    if (!ExtractStringValue(json, key, token, sizeof(token))) {
        const char *keyPos = FindKey(json, key);
        if (keyPos == nullptr) { return false; }

        const char *valuePos = strchr(keyPos, ':');
        if (valuePos == nullptr) { return false; }
        valuePos = SkipWhitespace(valuePos + 1);

        size_t i = 0;
        while (valuePos[i] != '\0' && valuePos[i] != ',' && valuePos[i] != '}' &&
               valuePos[i] != ' ' && valuePos[i] != '\t' && valuePos[i] != '\r' &&
               valuePos[i] != '\n' && i < sizeof(token) - 1) {
            token[i] = valuePos[i];
            i++;
        }
        token[i] = '\0';
    }

    char          *end    = nullptr;
    const unsigned parsed = static_cast<unsigned>(strtoul(token, &end, 10));
    if (end == token || *end != '\0') { return false; }

    value = parsed;
    return true;
}

bool ExtractStringArrayValue(const char *json, const char *key, Vector<CString> &values)
{
    const char *keyPos = FindKey(json, key);
    if (keyPos == nullptr) { return false; }

    const char *valuePos = strchr(keyPos, ':');
    if (valuePos == nullptr) { return false; }
    valuePos = SkipWhitespace(valuePos + 1);
    if (*valuePos != '[') { return false; }

    ++valuePos;
    bool foundAny = false;
    while (*valuePos != '\0') {
        valuePos = SkipWhitespace(valuePos);
        if (*valuePos == ']') { break; }
        if (*valuePos != '"') {
            while (*valuePos != '\0' && *valuePos != ',' && *valuePos != ']') {
                ++valuePos;
            }
            if (*valuePos == ',') {
                ++valuePos;
                continue;
            }
            break;
        }

        ++valuePos;
        const char *valueEnd = valuePos;
        while (*valueEnd != '\0' && *valueEnd != '"') {
            if (*valueEnd == '\\' && valueEnd[1] != '\0') {
                valueEnd += 2;
                continue;
            }
            ++valueEnd;
        }

        const size_t len = static_cast<size_t>(valueEnd - valuePos);
        CString      item;
        for (size_t i = 0; i < len; ++i) {
            item += valuePos[i];
        }
        values.push_back(item);
        foundAny = true;

        valuePos = valueEnd;
        if (*valuePos == '"') { ++valuePos; }
        valuePos = SkipWhitespace(valuePos);
        if (*valuePos == ',') {
            ++valuePos;
            continue;
        }
        if (*valuePos == ']') { break; }
    }

    return foundAny;
}

} // namespace zerom2m::codecs::json
