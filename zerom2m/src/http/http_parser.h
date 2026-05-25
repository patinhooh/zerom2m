/*
 * http_parser.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <zerom2m/http/types.h>

#include <circle/types.h>

namespace zerom2m::http
{

/**
 * @brief Simple in-place HTTP request parser.
 *
 * The parser operates on a writable buffer and produces views into that
 * buffer (for headers, target, path, query and body).
 */
class HttpParser
{
public:
    /**
     * @brief Parse a raw HTTP request buffer.
     *
     * @param data Pointer to a writable buffer containing the HTTP request
     *             (parser will NUL-terminate lines in-place).
     * @param length Length of the buffer in bytes.
     * @param request Output parsed request structure filled with views.
     * @return ResponseStatus HTTP response code indicating parse success or
     *         an error to be returned to the client.
     */
    ResponseStatus Parse(const u8 *data, size_t length, HttpRequest &request);

private:
    ResponseStatus ParseRequestLine(char *line, HttpRequest &request);
    ResponseStatus ParseHeaderLine(char *line, HttpRequest &request);

private:
    static constexpr size_t MaxHeaders = 32;
    HttpHeader              headers_[MaxHeaders];
    size_t                  headerCount_{0};
};

} // namespace zerom2m::http