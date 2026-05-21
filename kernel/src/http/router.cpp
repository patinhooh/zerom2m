/*
 * router.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "zerom2m/http/router.h"

#include <circle/logger.h>
#include <string.h>

namespace zerom2m::http
{

void Router::Register(RequestMethod method, const char *pathPrefix, IHttpHandler *handler)
{
    if (routeCount_ >= MaxRoutes) {
        delete handler;
        return;
    }

    Route &r        = routes_[routeCount_++];
    r.method        = method;
    const char *pfx = pathPrefix ? pathPrefix : "";
    r.wildcard      = false;
    size_t len      = strlen(pfx);
    if (len > 0 && pfx[len - 1] == '*') {
        r.wildcard = true;
        char   buf[256];
        size_t copyLen = (len - 1) < (sizeof(buf) - 1) ? (len - 1) : (sizeof(buf) - 1);
        memcpy(buf, pfx, copyLen);
        buf[copyLen] = '\0';
        r.prefix     = buf;
    } else {
        r.prefix = pfx;
    }
    r.handler = handler;
}

static bool PathMatches(const CString &prefix, bool wildcard, const StringView &path)
{
    size_t prefixLen = prefix.GetLength();
    if (wildcard) {
        if (path.Data == nullptr) return false;
        if (path.Length < prefixLen) return false;
        return memcmp((const char *)prefix, path.Data, prefixLen) == 0;
    } else {
        if (path.Data == nullptr) return false;
        return path.Length == prefixLen && memcmp((const char *)prefix, path.Data, prefixLen) == 0;
    }
}

HttpResponse Router::HandleRequest(const HttpRequest &request)
{
    // exact match takes precedence over wildcard; iterate in registration order
    for (unsigned i = 0; i < routeCount_; ++i) {
        Route &r = routes_[i];
        if (r.method != request.Method) continue;
        if (PathMatches(r.prefix, r.wildcard, request.Path)) {
            if (r.handler) return r.handler->HandleRequest(request);
        }
    }

    // Not found
    HttpResponse resp;
    resp.Status = ResponseStatus::NotFound;
    return resp;
}

} // namespace zerom2m::http
