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

#include "zerom2m/kernelconfig.h"

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/sched/scheduler.h>
#include <circle/usb/usbhcidevice.h>
#include <SDCard/emmc.h>
#include <fatfs/ff.h>
#include <wlan/bcm4343.h>
#include <circle/net/netsubsystem.h>
#include <wlan/hostap/wpa_supplicant/wpasupplicant.h>
#include <circle/types.h>

namespace zerom2m
{

enum ShutdownMode { None, Halt, Reboot };

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
     *
     * Cleans up resources (destructors may be empty for some subsystems).
     */
    ~Kernel();

    /**
     * @brief Initialize all kernel subsystems
     *
     * Initializes screen, serial, interrupt system, timer, logger, network, etc.
     *
     * @return true if initialization succeeded
     * @return false if initialization failed
     */
    bool Initialize();

    /**
     * @brief Main kernel run loop
     *
     * Starts scheduler, runs tasks, waits for shutdown requests.
     *
     * @return ShutdownMode Mode requested (Reboot, Halt, or None)
     */
    ShutdownMode Run();

private:
    CActLED            led_;
    CKernelOptions     kernelOptions_; // Circle kernel options
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
    KernelConfig       kernelConfig_; // Kernel configuration loaded from file
    CBcm4343Device     wlan_;
    CNetSubSystem      *net_;
    CWPASupplicant     wpaSupplicant_;
};

} // namespace zerom2m
