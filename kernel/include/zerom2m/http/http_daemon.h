/*
 * http_daemon.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include "zerom2m/http/http_common.h"
#include "zerom2m/http/http_handler.h"
#include "zerom2m/http/http_parser.h"
#include "zerom2m/http/http_serializer.h"
#include "zerom2m/http/http_types.h"

#include <circle/net/ipaddress.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/socket.h>
#include <circle/sched/task.h>
#include <circle/types.h>

namespace zerom2m::http
{

class HttpDaemon : public CTask
{
public:
    /**
     * @brief Construct an HttpDaemon
     *
     * @param netSubSystem Network subsystem used for listening/accepting
     * @param handler Application-level request handler (may be nullptr)
     * @param socket If nullptr: run in listener mode; otherwise worker for this socket
     * @param maxContentSize Maximum allowed request content size
     * @param port TCP port to bind when in listener mode
     * @param timeoutSeconds Socket receive timeout in seconds
     * @param maxClients Maximum number of concurrent clients (listener only)
     */
    HttpDaemon(CNetSubSystem *netSubSystem,
               IHttpHandler  *handler,
               CSocket       *socket         = nullptr,
               unsigned       maxContentSize = MAX_CONTENT_SIZE,
               u16            port           = DEFAULT_PORT,
               unsigned       timeoutSeconds = 0,
               unsigned       maxClients     = MAX_CLIENTS);

    /**
     * @brief Destroy the HttpDaemon
     */
    ~HttpDaemon(void);

    /**
     * @brief Task entry point. Starts listener or processes a worker connection.
     */
    void Run(void) override;

    /**
     * @brief Optional hook for access logging. Override in subclasses to capture
     *        transport-level access logs (remote IP, method, uri, status, size).
     */
    virtual void WriteAccessLog(const CIPAddress &remoteIP,
                                RequestMethod     requestMethod,
                                const char       *requestURI,
                                ResponseStatus    status,
                                unsigned          contentLength);

private:
    /**
     * @brief Listener loop: bind, listen and spawn worker tasks for accepted sockets.
     */
    void Listener(void);

    /**
     * @brief Worker: process a single connection, parse request and send response.
     */
    void Worker(void);

private:
    CNetSubSystem *netSubSystem_;
    CSocket       *socket_;
    IHttpHandler  *handler_; // application handler
    unsigned       maxContentSize_;
    u16            port_;
    unsigned       timeoutSeconds_;
    unsigned       maxClients_;

    static unsigned instanceCount_;
};

} // namespace zerom2m::http
