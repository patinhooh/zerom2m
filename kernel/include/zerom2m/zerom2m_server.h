/*
 * zerom2m_server.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include "zerom2m/handlers/index_handler.h"
#include "zerom2m/http/http_daemon.h"
#include "zerom2m/http/http_handler.h"
#include "zerom2m/http/http_types.h"
#include "zerom2m/http/router.h"
#include "zerom2m/kernel_config.h"
#include "zerom2m/onem2m/adapters/http/http_adapter.h"

#include <circle/actled.h>
#include <circle/types.h>

namespace zerom2m
{

class ZeroM2MServer : public http::HttpDaemon
{
public:
    /**
     * @brief Construct a new ZeroM2MServer instance
     *
     * @param net Pointer to the network subsystem
     * @param led Pointer to the LED to control
     * @param config Pointer to the kernel configuration
     * @param socket Pointer to the socket for this instance.
     *               Pass nullptr for the first instance, which acts as the listener.
     */
    ZeroM2MServer(CNetSubSystem         *net,
                  CActLED               *led,
                  const KernelConfig    *config,
                  onem2m::OneM2MService &service,
                  codecs::AutoCodec     &codec,
                  CSocket               *socket = nullptr);

    ~ZeroM2MServer(void);

private:
    CActLED            *led_;
    const KernelConfig *config_{nullptr};

    http::Router router_;

    onem2m::OneM2MService &service_;
    codecs::AutoCodec     &codec_;

    // TODO: Move this index info into an AE from the node it self resource and serve it from there
    handlers::IndexHandler              indexHandler_;
    onem2m::adapters::http::HttpAdapter httpAdapter_;
};

} // namespace zerom2m