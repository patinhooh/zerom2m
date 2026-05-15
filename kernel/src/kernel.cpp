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
#include "zerom2m/httpserver.h"

// SD card / firmware
#define DRIVE "SD:"
#define FIRMWARE_PATH DRIVE "/firmware/"
#define CONFIG_FILE DRIVE "/wpa_supplicant.conf"

// Network configuration
// #define USE_OPEN_NET "TEST" // SSID
#define USE_DHCP

#ifndef USE_DHCP
static const u8 IPAddress[]      = {192, 168, 0, 250};
static const u8 NetMask[]        = {255, 255, 255, 0};
static const u8 DefaultGateway[] = {192, 168, 0, 1};
static const u8 DNSServer[]      = {192, 168, 0, 1};
#endif

// Makefile CPPFLAGS
// QEMU_SAFE, QEMU safe mode skips hardware-specific modules.

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
    : screen_(options_.GetWidth(), options_.GetHeight())
    , serial_(&interrupt_)
    , timer_(&interrupt_)
    , logger_(options_.GetLogLevel(), &timer_)
    , usbHci_(&interrupt_, &timer_)
    , emmc_(&interrupt_, &timer_, &led_)
    , wlan_(FIRMWARE_PATH)
#ifndef USE_DHCP
    , net_(IPAddress, NetMask, DefaultGateway, DNSServer)
#elif defined(QEMU_SAFE)
    , net_(0, 0, 0, 0, DEFAULT_HOSTNAME, NetDeviceTypeEthernet)
#else
    , net_(0, 0, 0, 0, DEFAULT_HOSTNAME, NetDeviceTypeWLAN)
#endif
    , wpaSupplicant_(CONFIG_FILE)
{
    led_.Off();              // bootloader turns it on
    led_.Blink(3, 200, 200); // show we are alive
}

Kernel::~Kernel() {}

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
        CDevice *logDevice = deviceNameService_.GetDevice(options_.GetLogDevice(), false);
        if (logDevice == nullptr) logDevice = &screen_;
        ok = logger_.Initialize(logDevice);
        if (ok) {
            loggerOk = true;
            logger_.Write(FromKernel, LogNotice, "ZeroM2M Kernel '%s'", COMMIT_HASH);
            logger_.Write(FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);
            logger_.Write(FromKernel,
                          LogNotice,
                          "Logger initialized with log level %u",
                          options_.GetLogLevel());
#ifdef QEMU_SAFE
            logger_.Write(
                FromKernel, LogNotice, "QEMU_SAFE mode: some hardware modules where disabled");
#endif
        }
    }
#ifdef QEMU_SAFE
    // Only using usb in QEMU for now.
    if (ok) ok = usbHci_.Initialize();
#endif
    if (ok) ok = emmc_.Initialize();
    if (ok) {
        if (f_mount(&fileSystem_, DRIVE, 1) != FR_OK) {
            logger_.Write(FromKernel, LogError, "Cannot mount drive: %s", DRIVE);
            ok = false;
        }
    }
#ifndef QEMU_SAFE
    if (ok) ok = wlan_.Initialize();
    if (ok) logger_.Write(FromKernel, LogNotice, "WLAN driver initialized");
#ifdef USE_OPEN_NET
    if (ok) ok = wlan_.JoinOpenNet(USE_OPEN_NET);
#endif // USE_OPEN_NET
#endif // !QEMU_SAFE

    if (ok) ok = net_.Initialize(FALSE); // FALSE = don't block waiting for link

    if (ok) logger_.Write(FromKernel, LogNotice, "Network subsystem initialized");

#if !defined(QEMU_SAFE) && !defined(USE_OPEN_NET)
    if (ok) ok = wpaSupplicant_.Initialize();
    if (ok) logger_.Write(FromKernel, LogNotice, "WPA supplicant started");
#endif

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

    while (!net_.IsRunning()) {
        scheduler_.MsSleep(100);
    }

#ifndef QEMU_SAFE
    wlan_.DumpStatus();
#endif

    CString ip;
    net_.GetConfig()->GetIPAddress()->Format(&ip);
    logger_.Write(FromKernel, LogNotice, "HTTP server listening at http://%s/", (const char *)ip);

    new HttpServer(&net_, &led_);
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
