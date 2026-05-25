/*
 * string_view.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <circle/string.h>
#include <circle/util.h>

namespace zerom2m::compat
{

struct StringView {
    const char *Data{nullptr};
    size_t      Length{0};
};

inline CString StringViewToCString(const StringView &sv)
{
    if (sv.Data == nullptr || sv.Length == 0) { return CString{}; }

    CString s;
    s.Append("");

    for (size_t i = 0; i < sv.Length; ++i) {
        s += sv.Data[i];
    }
    return s;
}

inline bool StringViewEquals(StringView sv, const char *str)
{
    if (!str) return false;
    size_t len = strlen(str);
    if (sv.Length != len) return false;
    for (size_t i = 0; i < len; ++i)
        if (sv.Data[i] != str[i]) return false;
    return true;
}

} // namespace zerom2m::compat
