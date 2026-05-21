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
    // Listener mode: pass pSocket == nullptr.
    // Worker mode: pass an accepted CSocket.
    HttpDaemon(CNetSubSystem *netSubSystem,
               IHttpHandler  *handler,
               CSocket       *socket         = nullptr,
               unsigned       maxContentSize = 0,
               u16            port           = HTTP_DEFAULT_PORT,
               unsigned       timeoutSeconds = 0,
               unsigned       maxClients     = 10);
    ~HttpDaemon(void);

    void Run(void) override;

    // access logging hook (transport only)
    virtual void WriteAccessLog(const CIPAddress &remoteIP,
                                RequestMethod     requestMethod,
                                const char       *requestURI,
                                ResponseStatus    status,
                                unsigned          contentLength);

private:
    void Listener(void); // accepts incoming connections and creates worker task
    void Worker(void);   // processes a single connection

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
