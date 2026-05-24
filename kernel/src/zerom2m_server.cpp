/*
 * zerom2m_server.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "zerom2m/zerom2m_server.h"
#include "zerom2m/handlers/index_handler.h"
#include "zerom2m/http/router.h"

#include <assert.h>
#include <circle/logger.h>
#include <circle/string.h>
#include <circle/util.h>

namespace zerom2m
{

namespace
{
const char FromHttpServer[] = "http_server";
} // namespace

ZeroM2MServer::ZeroM2MServer(CNetSubSystem         *net,
                             CActLED               *led,
                             const KernelConfig    *config,
                             onem2m::OneM2MService &service,
                             CSocket               *socket)
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
    , service_(service)
    , indexHandler_(config)
    , httpAdapter_(service_)
{
    // Make sure the service is initialized before we start.
    service_.Initialize();

    // Register handlers for different routes.
    router_.Register(http::RequestMethod::GET, "/m2m*", &httpAdapter_);
    router_.Register(http::RequestMethod::POST, "/m2m*", &httpAdapter_);
    // TODO: Move this index info into an AE from the node it self resource and serve it from there
    router_.Register(http::RequestMethod::GET, "/", &indexHandler_);
}

ZeroM2MServer::~ZeroM2MServer()
{
    led_    = nullptr;
    config_ = nullptr;
}

} // namespace zerom2m
