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

#include <circle/actled.h>
#include <circle/devicenameservice.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/koptions.h>
#include <circle/logger.h>
#include <circle/net/netsubsystem.h>
#include <circle/sched/scheduler.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/timer.h>
#include <circle/types.h>
#include <circle/usb/usbhcidevice.h>

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
    CKernelOptions     options_;
    CDeviceNameService deviceNameService_;
    CScreenDevice      screen_;
    CSerialDevice      serial_;
    CExceptionHandler  exceptionHandler_;
    CInterruptSystem   interrupt_;
    CTimer             timer_;
    CLogger            logger_;
    CUSBHCIDevice      usbHci_;
    CScheduler         scheduler_;
    CNetSubSystem      net_;
};

} // namespace zerom2m
