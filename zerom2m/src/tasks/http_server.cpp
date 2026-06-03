/*
 * http_server.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include <zerom2m/handlers/index_handler.h>
#include <zerom2m/http/router.h>
#include <zerom2m/onem2m/onem2m_service.h>
#include <zerom2m/tasks/http_server.h>

#include <assert.h>
#include <circle/logger.h>
#include <circle/string.h>
#include <circle/util.h>

namespace zerom2m::tasks
{

using zerom2m::onem2m::OneM2MService;

namespace
{
const char FromHttpServer[] = "http_server";
} // namespace

HttpServer::HttpServer(CNetSubSystem      *net,
                       HttpAdapter        *httpBinding,
                       CActLED            *led,
                       const SystemConfig *config,
                       CSocket            *socket)
    : HttpDaemon(net,
                 &router_,
                 socket,
                 config->http.max_content_size,
                 config->http.port,
                 config->http.timeout_seconds,
                 config->http.max_clients)
    , led_(led)
    , config_(config)
    , router_()
    , indexHandler_(config)
    , httpAdapter_(httpBinding)
{
    // Register handlers for different routes.
    router_.Register(http::RequestMethod::GET, "/*", httpAdapter_);
    router_.Register(http::RequestMethod::POST, "/*", httpAdapter_);
    // XXX: Added PUT and DELETE for completeness, even though they are not currently supported by
    // the service.
    router_.Register(http::RequestMethod::PUT, "/*", httpAdapter_);
    router_.Register(http::RequestMethod::DELETE, "/*", httpAdapter_);
    // TODO: Move this index info into an AE from the node it self resource and serve it from
    // there
    router_.Register(http::RequestMethod::GET, "/", &indexHandler_);
}

HttpServer::~HttpServer()
{
    led_    = nullptr;
    config_ = nullptr;
}

} // namespace zerom2m::tasks
