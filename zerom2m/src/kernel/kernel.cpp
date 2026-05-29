/*
 * kernel.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "system_manager.h"

#include <zerom2m/compat/shutdown_mode.h>
#include <zerom2m/config/config_parser.h>
#include <zerom2m/kernel/kernel.h>
#include <zerom2m/kernel/network_manager.h>

#include <zerom2m/sqlite/sqlite3.h>

namespace zerom2m::kernel
{

using zerom2m::compat::ShutdownMode;
using namespace zerom2m::config;

namespace
{
const char FromKernel[] = "kernel";

SystemManager *gSystemManager = nullptr;

void OnRebootMagic()
{
    if (gSystemManager != nullptr) gSystemManager->RequestShutdown(ShutdownMode::Reboot);
}

void test_sqlite()
{
    sqlite3* db = nullptr;

    int rc = sqlite3_initialize();
    // should return SQLITE_OK (0)

    rc = sqlite3_open(":memory:", &db);
    // may fail OR may succeed depending on build flags

    if (rc == SQLITE_OK)
    {
        sqlite3_close(db);
    }
}

} // namespace

Kernel::Kernel()
    : screen_(kernelOptions_.GetWidth(), kernelOptions_.GetHeight())
    , serial_(&interrupt_)
    , timer_(&interrupt_)
    , logger_(kernelOptions_.GetLogLevel(), &timer_)
    , usbHci_(&interrupt_, &timer_)
    , emmc_(&interrupt_, &timer_, &led_)
    , networkManager_(timer_, scheduler_)
{
    led_.Off(); // bootloader turns it on
}

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
            CLogger::Get()->Write(FromKernel, LogNotice, "ZeroM2M Kernel '%s'", COMMIT_HASH);
            CLogger::Get()->Write(FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);
            CLogger::Get()->Write(
                FromKernel, LogNotice, "Logger level: %u", kernelOptions_.GetLogLevel());
        }
    }

    if (ok) ok = usbHci_.Initialize();
    if (ok) ok = emmc_.Initialize();
    if (ok) {
        if (f_mount(&fileSystem_, DRIVE, 1) != FR_OK) {
            CLogger::Get()->Write(FromKernel, LogError, "Cannot mount drive: %s", DRIVE);
            ok = false;
        }
    }

    ConfigParser parser(systemConfig_, logger_);
    if (!parser.Load(CONFIG_PATH)) {
        CLogger::Get()->Write(
            FromKernel, LogWarning, "Could not load " CONFIG_PATH ", using defaults");
    } else {
        parser.DumpConfig();
    }

    // Network init is fully delegated to NetworkManager
    if (ok) ok = networkManager_.Initialize(systemConfig_);

    if (ok) CLogger::Get()->Write(FromKernel, LogNotice, "Initialization succeeded");
    else if (loggerOk) {
        CLogger::Get()->Write(FromKernel, LogError, "Initialization failed");
        if (timerOk) timer_.MsDelay(1000);
    }

    return ok;
}

ShutdownMode Kernel::Run()
{
    CLogger::Get()->Write(FromKernel, LogDebug, "Running kernel");
    CLogger::Get()->Write(FromKernel, LogDebug, "Serial magic handler: %s", REBOOTMAGIC);
    serial_.RegisterMagicReceivedHandler(REBOOTMAGIC, OnRebootMagic);

    test_sqlite();
    return ShutdownMode::Halt;
    SystemManager sysMgr(led_, timer_, scheduler_, systemConfig_, networkManager_);
    gSystemManager = &sysMgr;

    // blocks until shutdown requested or fatal error occurs
    ShutdownMode result = sysMgr.Run();

    gSystemManager = nullptr;
    return result;
}

} // namespace zerom2m::kernel
