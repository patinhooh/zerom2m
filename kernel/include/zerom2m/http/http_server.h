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

#include "zerom2m/http/http_daemon.h"
#include "zerom2m/http/http_handler.h"
#include "zerom2m/http/http_types.h"

#include "zerom2m/kernelconfig.h"
#include <circle/actled.h>

namespace zerom2m::http
{

class HttpServer : public HttpDaemon, public IHttpHandler
{
public:
    /**
     * @brief Construct a new HttpServer instance
     *
     * @param net Pointer to the network subsystem
     * @param led Pointer to the LED to control
     * @param config Pointer to the kernel configuration
     * @param maxContentSize Maximum request size to accept (bytes)
     * @param timeoutSeconds Receive timeout (seconds)
     * @param maxClients Maximum concurrent clients (listener limit)
     * @param socket Pointer to the socket for this instance.
     *               Pass nullptr for the first instance, which acts as the listener.
     */
    HttpServer(u16                 port,
               CNetSubSystem      *net,
               CActLED            *led,
               const KernelConfig *config,
               unsigned            maxContentSize,
               unsigned            timeoutSeconds,
               unsigned            maxClients,
               CSocket            *socket = nullptr);

    ~HttpServer(void);

    /**
     * @brief Handle a parsed HTTP request and produce a response
     *
     * @param request Parsed request object
     * @return HttpResponse Structured response
     */
    HttpResponse HandleRequest(const HttpRequest &request) override;

private:
    CActLED            *led_;
    const KernelConfig *config_{nullptr};
};

} // namespace zerom2m::http