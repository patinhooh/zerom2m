/*
 * http_codec.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <zerom2m/http/types.h>

#include <circle/string.h>

namespace zerom2m::http
{

/**
 * @brief Shared helpers for HTTP request/response formatting.
 */
class HttpCodec
{
public:
    /**
     * @brief Build an HTTP/1.1 request header block.
     *
     * The helper adds `Host`, `User-Agent`, optional caller headers, and
     * `Connection: close`. It also adds `Content-Length` when a body is present.
     */
    static void SerializeRequest(const HttpRequest &request,
                                 CString           &outHeader,
                                 const char        *host,
                                 const char        *userAgent);

    /**
     * @brief Serialize a HTTP response header block.
     */
    static void SerializeResponse(const HttpResponse &response, CString &outHeader);

    /**
     * @brief Parse a raw HTTP/1.x response buffer.
     */
    static bool ParseResponse(const u8 *data, size_t length, HttpResponse &response);

    /**
     * @brief Parse a raw HTTP/1.x request buffer.
     */
    static ResponseStatus ParseRequest(const u8 *data, size_t length, HttpRequest &request);
};

} // namespace zerom2m::http