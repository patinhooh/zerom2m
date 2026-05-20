/*
 * http_serializer.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include "zerom2m/http/http_types.h"

#include <circle/string.h>

namespace zerom2m::http
{

class HttpSerializer
{
public:
    static void        Serialize(const HttpResponse &response, CString &outHeader);
    static const char *StatusReason(ResponseStatus status);
};

} // namespace zerom2m::http
