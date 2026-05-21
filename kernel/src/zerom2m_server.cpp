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

ZeroM2MServer::ZeroM2MServer(CNetSubSystem      *net,
                             CActLED            *led,
                             const KernelConfig *config,
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
{
    // Register handlers for different routes.
    router_.Register(http::RequestMethod::GET, "/", &indexHandler_);
}

ZeroM2MServer::~ZeroM2MServer()
{
    led_    = nullptr;
    config_ = nullptr;
}

} // namespace zerom2m
