/*
 * light_example.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <zerom2m/config/system_config.h>
#include <zerom2m/kernel/network_manager.h>
#include <zerom2m/onem2m/bindings/http/http_adapter.h>
#include <zerom2m/onem2m/notification_handler.h>
#include <zerom2m/tasks/p2p_node.h>

#include <circle/actled.h>

namespace zerom2m::tasks
{

using zerom2m::config::SystemConfig;
using zerom2m::kernel::NetworkManager;
using zerom2m::onem2m::INotificationHandler;
using zerom2m::onem2m::bindings::http::HttpAdapter;
using zerom2m::onem2m::types::Notification;
using zerom2m::onem2m::types::ResponsePrimitive;

class LightExample : public P2PNode
{
public:
    LightExample(CScheduler     *scheduler,
                 NetworkManager *net,
                 HttpAdapter    *httpAdapter,
                 SystemConfig   *config,
                 CActLED        *led,
                 bool            initialState = false);

    virtual ~LightExample();

    void Run() override;

protected:
    // Sub class hooks

    virtual ResponsePrimitive OnData(const Notification &sgn) override;
    virtual ResponsePrimitive OnVerification(const Notification &sgn) override;

    // Helper methods

    void SetState(bool on);

private:
    CScheduler     *scheduler_;
    NetworkManager *net_;
    HttpAdapter    *httpAdapter_;
    SystemConfig   *config_;
    CActLED        *led_;
    bool            state_;
};

} // namespace zerom2m::tasks