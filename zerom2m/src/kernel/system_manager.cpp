/*
 * system_manager.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "system_manager.h"

#include <zerom2m/compat/shutdown_mode.h>
#include <zerom2m/kernel/kernel.h>
#include <zerom2m/kernel/network_manager.h>
#include <zerom2m/onem2m/onem2m_service.h>
#include <zerom2m/tasks/blink_task.h>
#include <zerom2m/tasks/http_server.h>

#include <circle/logger.h>

namespace zerom2m::kernel
{

namespace
{
const char FromSystemManager[] = "sysmgr";
}

SystemManager::SystemManager(CActLED              &led,
                             CTimer               &timer,
                             CScheduler           &scheduler,
                             config::SystemConfig &config,
                             NetworkManager       &networkManager)
    : led_(led)
    , timer_(timer)
    , scheduler_(scheduler)
    , config_(config)
    , networkManager_(networkManager)
    , shutdownRequest_(ShutdownMode::None)
{
}

ShutdownMode SystemManager::Run()
{
    CLogger::Get()->Write(FromSystemManager, LogNotice, "Starting system manager");

    if (!networkManager_.WaitUntilReady()) {
        CLogger::Get()->Write(FromSystemManager, LogError, "Network unavailable, halting");
        timer_.MsDelay(1000);
        return ShutdownMode::Halt;
    }

    networkManager_.DumpStatus();

    StartServices();

    CLogger::Get()->Write(FromSystemManager, LogNotice, "All services started");

    while (shutdownRequest_ == ShutdownMode::None) {
        scheduler_.MsSleep(100);
        scheduler_.Yield();

        if (!networkManager_.IsRunning()) {
            CLogger::Get()->Write(
                FromSystemManager, LogWarning, "Network lost, attempting recovery");

            if (!networkManager_.Restart()) {
                CLogger::Get()->Write(
                    FromSystemManager, LogError, "Network unrecoverable, halting");
                timer_.MsDelay(1000);
                return ShutdownMode::Halt;
            }

            CLogger::Get()->Write(FromSystemManager, LogNotice, "Network restored, resuming");
        }
    }

    CLogger::Get()->Write(
        FromSystemManager, LogNotice, "Shutdown requested: %d", (int)shutdownRequest_);
    timer_.MsDelay(1000);
    return shutdownRequest_;
}

void SystemManager::RequestShutdown(ShutdownMode mode) { shutdownRequest_ = mode; }

void SystemManager::StartServices()
{
    // Each line here is a service. The kernel knows nothing about these.
    // Services are CTask subclasses: constructing them registers them with
    // the Circle scheduler automatically.

    // Make sure the service is initialized before we start.
    onem2m::OneM2MService::Get().Initialize(config_);
    onem2m::OneM2MService::Get().SetNetSubSystem(networkManager_.GetNetSubSystem());

    new tasks::BlinkTask(&scheduler_, &led_, 1000);
    new tasks::HttpServer(&networkManager_.GetNetSubSystem(), &led_, &config_);
}

} // namespace zerom2m::kernel
