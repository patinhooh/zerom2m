/*
 * utils.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <circle/string.h>

namespace zerom2m::compat
{

inline CString DoubleToCString(double d)
{
    CString s;
    // Emit as integer when the value is a whole number (avoids "42.000000").
    s64 iv = static_cast<s64>(d);
    if (static_cast<double>(iv) == d && d >= -1e15 && d <= 1e15) s.Format("%lld", iv);
    else s.Format("%g", d);
    return s;
}

} // namespace zerom2m::compat