/*
 * network_manager.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <zerom2m/config/system_config.h>
#include <zerom2m/kernel/paths.h>

#include <circle/net/netsubsystem.h>
#include <circle/sched/scheduler.h>
#include <circle/timer.h>
#include <circle/types.h>
#include <wlan/bcm4343.h>
#include <wlan/hostap/wpa_supplicant/wpasupplicant.h>

namespace zerom2m::kernel
{

/**
 * @brief Owns all network hardware and manages its lifecycle.
 *
 * NetworkManager owns:
 *   - CBcm4343Device  (WLAN driver)
 *   - CNetSubSystem   (IP stack)
 *   - CWPASupplicant  (WPA authentication)
 *
 * Call Initialize() during kernel init, then pass NetworkManager& into
 * SystemManager which uses it for readiness polling and recovery.
 */
class NetworkManager
{
public:
    /**
     * @param timer      The system timer
     * @param scheduler  The Circle cooperative scheduler
     */
    NetworkManager(CTimer &timer, CScheduler &scheduler);
    ~NetworkManager();

    /**
     * @brief Selects network mode, brings up WiFi or falls back to Ethernet,
     *        starts the IP stack and WPA supplicant.
     *
     * Replaces all network-related code previously in Kernel::Initialize().
     *
     * @param config  System configuration (may be mutated on WiFi to Ethernet fallback)
     * @return true   Network subsystem initialised successfully
     * @return false  Fatal initialization failure
     */
    bool Initialize(config::SystemConfig &config);

    /**
     * @brief Blocks cooperatively until the network is ready or timeout expires.
     *
     * @param timeoutMs Maximum time to wait in milliseconds (default: 30 seconds)
     * @return true  Network is up
     * @return false Timed out
     */
    bool WaitUntilReady(unsigned timeoutMs = 30000);

    /**
     * @brief Returns whether the network is currently running.
     */
    bool IsRunning();

    /**
     * @brief Waits cooperatively for the network to recover after a drop.
     *
     * If the network does not recover within timeoutMs, returns false so the
     * caller can decide to halt.
     *
     * @param timeoutMs Maximum time to wait for recovery (default: 60 seconds)
     * @return true  Network recovered
     * @return false Network unrecoverable within timeout
     */
    bool Restart(unsigned timeoutMs = 60000);

    /**
     * @brief Dumps current network status to the logger.
     */
    void DumpStatus();

    /**
     * @brief Gets the IP for this device.
     */
    CString GetIP();

    /**
     * @brief Returns the underlying network subsystem.
     *
     * Used by SystemManager to pass to services that need it (e.g. HttpServer).
     */
    CNetSubSystem &GetNetSubSystem();

private:
    CNetSubSystem *CreateNetSubSystem(const config::SystemConfig &config, TNetDeviceType devType);

    CTimer        &timer_;
    CScheduler    &scheduler_;
    CBcm4343Device wlan_;
    CNetSubSystem *net_;
    CWPASupplicant wpaSupplicant_;
};

} // namespace zerom2m::kernel
