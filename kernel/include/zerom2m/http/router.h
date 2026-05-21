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

/**
 * @brief Simple path-based router for dispatching requests to handlers.
 *
 * Routes may be registered with an optional trailing '*' to indicate a
 * wildcard prefix match. The Router does NOT take ownership of handlers;
 * callers must ensure registered handlers outlive the Router.
 */
class Router : public IHttpHandler
{
public:
    /**
     * @brief Register a handler for requests matching method and path prefix.
     *
     * @param method HTTP method to match
     * @param pathPrefix Path prefix to match (suffix '*' enables wildcard)
     * @param handler Pointer to handler (not owned)
     */
    void Register(RequestMethod method, const char *pathPrefix, IHttpHandler *handler);

    /**
     * @brief Dispatch the request to the first matching registered handler.
     *
     * Returns 404 Not Found when no route matches.
     */
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
