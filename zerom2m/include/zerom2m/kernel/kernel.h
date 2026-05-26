/*
 * kernel.h
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
#include <zerom2m/kernel/paths.h>

#include <SDCard/emmc.h>
#include <circle/actled.h>
#include <circle/devicenameservice.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/koptions.h>
#include <circle/logger.h>
#include <circle/sched/scheduler.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/timer.h>
#include <circle/types.h>
#include <circle/usb/usbhcidevice.h>
#include <fatfs/ff.h>

namespace zerom2m::kernel
{

using zerom2m::compat::ShutdownMode;
using zerom2m::config::SystemConfig;

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

class Kernel
{
public:
    /**
     * @brief Construct a new Kernel
     *
     * Initializes members but does not start subsystems.
     */
    Kernel();

    /**
     * @brief Destroy the Kernel
     */
    ~Kernel() = default;

    /**
     * @brief Initialize all kernel subsystems
     *
     * Initializes screen, serial, interrupt system, timer, logger, USB, EMMC,
     * filesystem, and network (via NetworkManager).
     *
     * @return true if initialization succeeded
     * @return false if initialization failed
     */
    bool Initialize();

    /**
     * @brief Main kernel run loop
     *
     * Hands off to SystemManager which starts services and monitors the system.
     *
     * @return ShutdownMode requested (Reboot or Halt)
     */
    ShutdownMode Run();

private:
    CActLED            led_;
    CKernelOptions     kernelOptions_;
    CDeviceNameService deviceNameService_;
    CScreenDevice      screen_;
    CSerialDevice      serial_;
    CExceptionHandler  exceptionHandler_;
    CInterruptSystem   interrupt_;
    CTimer             timer_;
    CLogger            logger_;
    CScheduler         scheduler_;
    CUSBHCIDevice      usbHci_;
    CEMMCDevice        emmc_;
    FATFS              fileSystem_;
    SystemConfig       systemConfig_;
    NetworkManager     networkManager_;
};

} // namespace zerom2m::kernel
