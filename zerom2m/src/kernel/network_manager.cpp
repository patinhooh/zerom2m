/*
 * network_manager.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include <zerom2m/kernel/network_manager.h>

#include <circle/logger.h>

namespace zerom2m::kernel
{

using namespace zerom2m::config;

namespace
{
const char FromNetworkManager[] = "netmgr";
}

NetworkManager::NetworkManager(CTimer &timer, CScheduler &scheduler)
    : timer_(timer)
    , scheduler_(scheduler)
    , wlan_(FIRMWARE_PATH)
    , net_(nullptr)
    , wpaSupplicant_(WPA_PATH)
{
}

NetworkManager::~NetworkManager() { delete net_; }

CNetSubSystem *NetworkManager::CreateNetSubSystem(const SystemConfig &config,
                                                  TNetDeviceType      devType)
{
    const u8 *ip      = config.network.dhcp ? nullptr : config.network.ip;
    const u8 *netmask = config.network.dhcp ? nullptr : config.network.netmask;
    const u8 *gateway = config.network.dhcp ? nullptr : config.network.gateway;
    const u8 *dns     = config.network.dhcp ? nullptr : config.network.dns;

    return new CNetSubSystem(ip, netmask, gateway, dns, config.system.hostname, devType);
}

bool NetworkManager::Initialize(SystemConfig &config)
{
    bool openNetEnabled = config.network.open_net_ssid.GetLength() > 0;
    bool wifiEnabled =
        config.network.mode == NetworkMode::Wifi || config.network.mode == NetworkMode::Auto;

    // Start with the requested device type
    TNetDeviceType devType = wifiEnabled ? NetDeviceTypeWLAN : NetDeviceTypeEthernet;

    net_ = CreateNetSubSystem(config, devType);

    bool wifiOk = false;
    if (wifiEnabled) {
        wifiOk = wlan_.Initialize();
        if (wifiOk) {
            CLogger::Get()->Write(FromNetworkManager, LogNotice, "WLAN driver initialized");
            if (openNetEnabled) {
                wifiOk = wlan_.JoinOpenNet(config.network.open_net_ssid);
                if (wifiOk)
                    CLogger::Get()->Write(FromNetworkManager, LogNotice, "Joining open network");
                else
                    CLogger::Get()->Write(
                        FromNetworkManager, LogWarning, "Failed to join open network");
            }
        } else {
            CLogger::Get()->Write(
                FromNetworkManager, LogWarning, "WLAN driver failed to initialize");
        }
    }

    // Fall back to Ethernet if WiFi failed
    if (wifiEnabled && !wifiOk) {
        CLogger::Get()->Write(FromNetworkManager, LogWarning, "Falling back to Ethernet");
        delete net_;
        net_ = CreateNetSubSystem(config, NetDeviceTypeEthernet);
        // Override so the rest of the system does not think WiFi is active
        config.network.mode = NetworkMode::Ethernet;
    }

    if (!net_->Initialize(FALSE)) {
        CLogger::Get()->Write(FromNetworkManager, LogError, "Network subsystem init failed");
        return false;
    }
    CLogger::Get()->Write(FromNetworkManager, LogNotice, "Network subsystem initialized");

    if (openNetEnabled || wifiOk) {
        if (!wpaSupplicant_.Initialize()) {
            CLogger::Get()->Write(FromNetworkManager, LogWarning, "WPA supplicant failed to start");
            // Not fatal, continue without WPA if open network or fallback is active
        } else {
            CLogger::Get()->Write(FromNetworkManager, LogNotice, "WPA supplicant started");
        }
    }

    return true;
}

bool NetworkManager::WaitUntilReady(unsigned timeoutMs)
{
    unsigned waited = 0;
    while (!net_->IsRunning() && waited < timeoutMs) {
        scheduler_.MsSleep(100);
        waited += 100;

        if ((waited % 1000) == 0) {
            CLogger::Get()->Write(
                FromNetworkManager, LogDebug, "Waiting for network: elapsed=%ums", waited);
        }

        scheduler_.Yield();
    }

    if (!net_->IsRunning()) {
        CLogger::Get()->Write(
            FromNetworkManager, LogError, "Network failed to come up after %ums", timeoutMs);
        return false;
    }

    CLogger::Get()->Write(FromNetworkManager, LogNotice, "Network is up");
    return true;
}

bool NetworkManager::IsRunning() { return net_->IsRunning(); }

bool NetworkManager::Restart(unsigned timeoutMs)
{
    CLogger::Get()->Write(FromNetworkManager, LogWarning, "Waiting for network recovery");

    unsigned waitedDs = 0; // 1ds == 100ms
    while (!net_->IsRunning() && waitedDs < timeoutMs) {
        scheduler_.MsSleep(100);
        waitedDs += 1;

        if ((waitedDs % 10) == 0) {
            CLogger::Get()->Write(
                FromNetworkManager, LogDebug, "Recovering network: elapsed=%ums", waitedDs);
        }

        scheduler_.Yield();
    }

    if (!net_->IsRunning()) {
        CLogger::Get()->Write(
            FromNetworkManager, LogError, "Network unrecoverable after %ums", timeoutMs);
        return false;
    }

    CLogger::Get()->Write(FromNetworkManager, LogNotice, "Network recovered");
    return true;
}

void NetworkManager::DumpStatus()
{
    CLogger::Get()->Write(
        FromNetworkManager, LogNotice, "Network is%s running", net_->IsRunning() ? "" : " not");

    if (wlan_.IsLinkUp()) wlan_.DumpStatus();
}

CNetSubSystem &NetworkManager::GetNetSubSystem() { return *net_; }

} // namespace zerom2m::kernel
