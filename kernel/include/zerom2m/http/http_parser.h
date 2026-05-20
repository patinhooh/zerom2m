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

#include "zerom2m/http/http_types.h"

#include <circle/types.h>

namespace zerom2m::http
{

class HttpParser
{
public:
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
