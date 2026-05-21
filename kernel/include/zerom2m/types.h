/*
 * types.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <circle/types.h>

namespace zerom2m
{

struct StringView {
    const char *Data{nullptr};
    size_t      Length{0};
};

} // namespace zerom2m
