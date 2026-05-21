/*
 * router.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include "zerom2m/http/http_handler.h"
#include "zerom2m/http/http_types.h"

#include <circle/string.h>

namespace zerom2m::http
{

class Router : public IHttpHandler
{
public:
    // Router does NOT take ownership of handlers.
    // Handlers must outlive the Router.
    void Register(RequestMethod method, const char *pathPrefix, IHttpHandler *handler);

    HttpResponse HandleRequest(const HttpRequest &request) override;

private:
    struct Route {
        RequestMethod method;
        CString       prefix;
        IHttpHandler *handler{nullptr};
        bool          wildcard{false};
    };

    static const unsigned MaxRoutes = 16;
    Route                 routes_[MaxRoutes];
    unsigned              routeCount_{0};
};

} // namespace zerom2m::http
