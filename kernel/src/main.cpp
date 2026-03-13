/*
 * main.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "zerom2m/kernel.h"
#include <circle/startup.h>

using zerom2m::Kernel;
using zerom2m::ShutdownMode;

int main(void)
{
    Kernel Kernel;
    if (!Kernel.Initialize()) {
        // XXX: Rebooting if kernel fails to initialize
        reboot();
        return EXIT_REBOOT;
    }

    ShutdownMode ShutdownMode = Kernel.Run();
    switch (ShutdownMode) {
        case ShutdownMode::Reboot:
            reboot();
            return EXIT_REBOOT;

        case ShutdownMode::Halt:
        case ShutdownMode::None:
        default:
            halt();
            return EXIT_HALT;
    }
}
