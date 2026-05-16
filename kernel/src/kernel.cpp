/*
 * kernel.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "zerom2m/kernel.h"
#include "zerom2m/blinktask.h"
#include "zerom2m/configparser.h"
#include "zerom2m/httpserver.h"

#include <circle/net/netsubsystem.h>

// SD card
#define DRIVE "SD:"
#define FIRMWARE_PATH DRIVE "/firmware/"
#define CONFIG_PATH DRIVE "/kernel.cfg"
#define WPA_PATH DRIVE "/wpa_supplicant.conf"

// Makefile CPPFLAGS
#ifndef COMMIT_HASH
#define COMMIT_HASH "unknown"
#endif

#ifndef REBOOTMAGIC
#define REBOOTMAGIC "reboot"
#endif

#ifndef USERBAUD
#define USERBAUD 115200
#endif

namespace zerom2m
{

namespace
{
const char            FromKernel[]    = "kernel";
volatile ShutdownMode shutdownRequest = ShutdownMode::None;

void SetReboot() { shutdownRequest = ShutdownMode::Reboot; }

} // namespace

Kernel::Kernel()
    : screen_(kernelOptions_.GetWidth(), kernelOptions_.GetHeight())
    , serial_(&interrupt_)
    , timer_(&interrupt_)
    , logger_(kernelOptions_.GetLogLevel(), &timer_)
    , usbHci_(&interrupt_, &timer_)
    , emmc_(&interrupt_, &timer_, &led_)
    , wlan_(FIRMWARE_PATH)
    , net_(nullptr)
    , wpaSupplicant_(WPA_PATH)
{
    led_.Off();              // bootloader turns it on
    led_.Blink(3, 200, 200); // show we are alive
}

Kernel::~Kernel() { delete net_; }

bool Kernel::Initialize()
{
    bool ok       = true;
    bool timerOk  = false;
    bool loggerOk = false;

    if (ok) ok = screen_.Initialize();
    if (ok) ok = serial_.Initialize(USERBAUD);
    if (ok) ok = interrupt_.Initialize();
    if (ok) ok = timer_.Initialize();
    if (ok) timerOk = ok;
    if (ok) {
        CDevice *logDevice = deviceNameService_.GetDevice(kernelOptions_.GetLogDevice(), false);
        if (logDevice == nullptr) logDevice = &screen_;
        ok = logger_.Initialize(logDevice);
        if (ok) {
            loggerOk = true;
            logger_.Write(FromKernel, LogNotice, "ZeroM2M Kernel '%s'", COMMIT_HASH);
            logger_.Write(FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);
            logger_.Write(FromKernel,
                          LogNotice,
                          "Logger initialized with log level %u",
                          kernelOptions_.GetLogLevel());
        }
    }

    if (ok) ok = usbHci_.Initialize();
    if (ok) ok = emmc_.Initialize();
    if (ok) {
        if (f_mount(&fileSystem_, DRIVE, 1) != FR_OK) {
            logger_.Write(FromKernel, LogError, "Cannot mount drive: %s", DRIVE);
            ok = false;
        }
    }

    ConfigParser parser(kernelConfig_, logger_);
    if (!parser.Load(CONFIG_PATH)) {
        logger_.Write(FromKernel, LogWarning, "Could not load " CONFIG_PATH ", using defaults");
    } else {
        parser.DumpConfig();
    }

    TNetDeviceType devType = (kernelConfig_.network.mode == NetworkMode::Wifi ||
                              kernelConfig_.network.mode == NetworkMode::Auto)
                                 ? NetDeviceTypeWLAN
                                 : NetDeviceTypeEthernet;

    // nullptr signals DHCP to CNetSubSystem
    const u8 *ip      = kernelConfig_.network.dhcp ? nullptr : kernelConfig_.network.ip;
    const u8 *netmask = kernelConfig_.network.dhcp ? nullptr : kernelConfig_.network.netmask;
    const u8 *gateway = kernelConfig_.network.dhcp ? nullptr : kernelConfig_.network.gateway;
    const u8 *dns     = kernelConfig_.network.dhcp ? nullptr : kernelConfig_.network.dns;

    if (ok) {
        net_ = new CNetSubSystem(ip, netmask, gateway, dns, kernelConfig_.system.hostname, devType);
    }

    bool openNetEnabled = kernelConfig_.network.open_net_ssid.GetLength() > 0;
    bool wifiEnabled    = kernelConfig_.network.mode == NetworkMode::Wifi ||
                          kernelConfig_.network.mode == NetworkMode::Auto;

    bool wifiOk = false;
    if (wifiEnabled && ok) {
        wifiOk = wlan_.Initialize();

        if (wifiOk) {
            logger_.Write(FromKernel, LogNotice, "WLAN driver initialized");
            if (openNetEnabled) {
                wifiOk = wlan_.JoinOpenNet(kernelConfig_.network.open_net_ssid);
                if (wifiOk) logger_.Write(FromKernel, LogNotice, "Joining open network is enabled");
                else logger_.Write(FromKernel, LogWarning, "Failed to join open network");
            }
        } else {
            logger_.Write(FromKernel, LogWarning, "WLAN driver failed to initialize");
        }
    }

    // Fall back to Ethernet
    if (!wifiOk && wifiEnabled) {
        logger_.Write(FromKernel, LogWarning, "Falling back to Ethernet");

        delete net_;
        net_ = new CNetSubSystem(
            ip, netmask, gateway, dns, kernelConfig_.system.hostname, NetDeviceTypeEthernet);

        // Override network mode in config to avoid confusion in other parts of the system
        kernelConfig_.network.mode = NetworkMode::Ethernet;
    }

    if (ok) ok = net_->Initialize(FALSE);

    if (ok) logger_.Write(FromKernel, LogNotice, "Network subsystem initialized");

    if (ok && openNetEnabled && wifiOk) {
        ok = wpaSupplicant_.Initialize();
        if (ok) logger_.Write(FromKernel, LogNotice, "WPA supplicant started");
    }

    if (ok) {
        logger_.Write(FromKernel, LogNotice, "Initialization succeeded");
    } else if (loggerOk) {
        logger_.Write(FromKernel, LogError, "Initialization failed");
        if (timerOk) timer_.MsDelay(1000);
    }
    return ok;
}

ShutdownMode Kernel::Run()
{
    logger_.Write(FromKernel, LogDebug, "Running kernel");
    logger_.Write(FromKernel, LogDebug, "Setting Reboot Magic to '%s'", REBOOTMAGIC);
    serial_.RegisterMagicReceivedHandler(REBOOTMAGIC, SetReboot);

    while (!net_->IsRunning()) {
        scheduler_.MsSleep(100);
    }

    if (kernelConfig_.network.mode == NetworkMode::Wifi ||
        kernelConfig_.network.mode == NetworkMode::Auto) {
        wlan_.DumpStatus();
    }

    CString ip;
    net_->GetConfig()->GetIPAddress()->Format(&ip);
    logger_.Write(FromKernel, LogNotice, "HTTP server listening at http://%s/", (const char *)ip);

    new HttpServer(kernelConfig_.http.port, net_, &led_);
    new BlinkTask(&led_, 1000);

    while (true) {
        scheduler_.Yield();
        if (shutdownRequest != ShutdownMode::None) {
            logger_.Write(FromKernel, LogNotice, "Shutdown requested: %d", shutdownRequest);
            timer_.MsDelay(1000);
            return shutdownRequest;
        }
    }
    return ShutdownMode::Halt;
}

} // namespace zerom2m
