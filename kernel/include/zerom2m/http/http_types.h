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

#include <circle/string.h>
#include <circle/types.h>

namespace zerom2m::http
{

struct StringView {
    const char *Data{nullptr};
    size_t      Length{0};
};

struct HttpHeader {
    StringView Name;
    StringView Value;
};

struct HttpRequest {
    RequestMethod Method{RequestMethod::RequestMethodUnknown};

    StringView Target;
    StringView Path;
    StringView Query;

    StringView Version;

    const HttpHeader *Headers{nullptr};
    size_t            HeaderCount{0};

    const u8 *Body{nullptr};
    size_t    BodyLength{0};
};

struct HttpResponse {
    ResponseStatus Status{ResponseStatus::OK};

    const HttpHeader *Headers{nullptr};
    size_t            HeaderCount{0};

    const u8 *Body{nullptr};
    size_t    BodyLength{0};
};

} // namespace zerom2m::http
