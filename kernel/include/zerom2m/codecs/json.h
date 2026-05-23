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

#include "zerom2m/compat/collections.h"

#include <circle/string.h>
#include <circle/types.h>

// TODO: This is just a very basic implementation to get something working.
using zerom2m::compat::Vector;

namespace zerom2m::codecs::json
{

void AppendEscapedString(CString &out, const char *value);
void AppendStringField(CString &out, bool &first, const char *name, const char *value);
void AppendUnsignedField(CString &out, bool &first, const char *name, unsigned value);
void AppendBoolField(CString &out, bool &first, const char *name, bool value);
void AppendStringArrayField(
    CString &out, bool &first, const char *name, const char *const *values, size_t count);

const char *FindKey(const char *json, const char *key);
bool        ExtractStringValue(const char *json, const char *key, char *buffer, size_t bufferSize);
bool        ExtractBoolValue(const char *json, const char *key, bool &value);
bool        ExtractUnsignedValue(const char *json, const char *key, unsigned &value);
bool        ExtractStringArrayValue(const char *json, const char *key, Vector<CString> &values);

} // namespace zerom2m::codecs::json
