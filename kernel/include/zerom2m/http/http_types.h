/*
 * http_types.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include "zerom2m/http/http_common.h"
#include "zerom2m/types.h"

#include <circle/string.h>
#include <circle/types.h>

namespace zerom2m::http
{

/**
 * @brief Name/value pair representing a single HTTP header.
 */
struct HttpHeader {
    StringView Name;
    StringView Value;
};

/**
 * @brief Parsed HTTP request representation.
 *
 * `Target` holds the original request target (path + optional query).
 * `Path` and `Query` are views into the parsed `Target` buffer.
 */
struct HttpRequest {
    RequestMethod Method{RequestMethod::RequestMethodUnknown};

    StringView Target; /**< Raw request-target */
    StringView Path;   /**< Path portion of the target */
    StringView Query;  /**< Query-string portion (if any) */

    StringView Version; /**< HTTP version token, e.g. "HTTP/1.1" */

    const HttpHeader *Headers{nullptr}; /**< Array of headers (pointing into parse buffer) */
    size_t            HeaderCount{0};

    const u8 *Body{nullptr}; /**< Pointer to request body (if any) */
    size_t    BodyLength{0};
};

/**
 * @brief HTTP response description returned by handlers.
 */
struct HttpResponse {
    ResponseStatus Status{ResponseStatus::OK};

    const HttpHeader *Headers{nullptr}; /**< Optional extra headers to emit */
    size_t            HeaderCount{0};

    const u8 *Body{nullptr}; /**< Optional response body buffer */
    size_t    BodyLength{0};
};

} // namespace zerom2m::http
