/*
 * system_manager.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <zerom2m/compat/shutdown_mode.h>
#include <zerom2m/config/system_config.h>
#include <zerom2m/kernel/network_manager.h>

#include <circle/actled.h>
#include <circle/sched/scheduler.h>
#include <circle/timer.h>

namespace zerom2m::kernel
{

using zerom2m::compat::ShutdownMode;

/**
 * @brief Owns system-level boot sequencing and the main run loop.
 *
 * The kernel initialises hardware, constructs a SystemManager, and calls Run().
 * Everything after that, network readiness, service startup, network recovery, and shutdown
 * handling, is the SystemManager's responsibility.
 */
class SystemManager
{
public:
    /**
     * @param led             An active LED instance for status indication
     * @param timer           The system timer
     * @param scheduler       The Circle cooperative scheduler
     * @param config          The system configuration
     * @param networkManager  Owns all network hardware and state
     */
    SystemManager(CActLED              &led,
                  CTimer               &timer,
                  CScheduler           &scheduler,
                  config::SystemConfig &config,
                  NetworkManager       &networkManager);

    /**
     * @brief Runs the system: waits for network, starts services, loops until shutdown.
     *
     * Monitors network health during operation. If the network drops, attempts recovery for up to
     * 60 seconds before halting cleanly.
     *
     * Blocks until a shutdown is requested or a fatal error occurs.
     *
     * @return The ShutdownMode to pass back to the bootloader.
     */
    ShutdownMode Run();

    /**
     * @brief Request a shutdown from any context (e.g. serial magic handler).
     */
    void RequestShutdown(ShutdownMode mode);

private:
    /**
     * @brief Spawns all application-level CTask services.
     *
     * Called once, after the network is confirmed ready.
     * Add new services here, nowhere else needs to change.
     */
    void StartServices();

    CActLED              &led_;
    CTimer               &timer_;
    CScheduler           &scheduler_;
    config::SystemConfig &config_;
    NetworkManager       &networkManager_;

    volatile ShutdownMode shutdownRequest_;
};

} // namespace zerom2m::kernel
