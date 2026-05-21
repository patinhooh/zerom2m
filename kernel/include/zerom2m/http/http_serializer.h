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

/**
 * @brief Helpers to serialize HTTP responses and map status codes to reason strings.
 */
class HttpSerializer
{
public:
    /**
     * @brief Serialize response status and headers into an HTTP/1.1 header block.
     *
     * This always writes a `Content-Length` header and a `Connection: close` line.
     */
    static void Serialize(const HttpResponse &response, CString &outHeader);

    /**
     * @brief Return the textual reason phrase for a status code.
     */
    static const char *StatusReason(ResponseStatus status);
};

} // namespace zerom2m::http
