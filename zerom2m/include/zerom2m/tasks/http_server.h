/*
 * http_server.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <zerom2m/config/system_config.h>
#include <zerom2m/handlers/index_handler.h>
#include <zerom2m/http/http_daemon.h>
#include <zerom2m/http/http_handler.h>
#include <zerom2m/http/router.h>
#include <zerom2m/http/types.h>
#include <zerom2m/onem2m/bindings/http/http_adapter.h>

#include <circle/actled.h>
#include <circle/types.h>

namespace zerom2m::tasks
{
using zerom2m::config::SystemConfig;
using zerom2m::http::HttpDaemon;
using zerom2m::http::Router;
using zerom2m::onem2m::OneM2MService;
using zerom2m::onem2m::bindings::http::HttpAdapter;

class HttpServer : public HttpDaemon
{
public:
    /**
     * @brief Construct a new HttpServer instance
     *
     * @param net Pointer to the network subsystem
     * @param led Pointer to the LED to control
     * @param config Pointer to the system configuration
     * @param socket Pointer to the socket for this instance.
     *               Pass nullptr for the first instance, which acts as the listener.
     */
    HttpServer(CNetSubSystem      *net,
               CActLED            *led,
               const SystemConfig *config,
               CSocket            *socket = nullptr);

    ~HttpServer(void);

private:
    CActLED            *led_;
    const SystemConfig *config_{nullptr};
    Router              router_;
    // TODO: Move this index info into an AE from the node it self resource and serve it from there
    handlers::IndexHandler indexHandler_;
    HttpAdapter            httpAdapter_;
};

} // namespace zerom2m::tasks