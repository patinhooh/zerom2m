/*
 * main.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include <zerom2m/compat/shutdown_mode.h>
#include <zerom2m/kernel/kernel.h>

#include <circle/startup.h>

using zerom2m::compat::ShutdownMode;
using zerom2m::kernel::Kernel;

int main(void)
{
    Kernel kernel;
    if (!kernel.Initialize()) {
        halt();
        return EXIT_HALT;
    }

    ShutdownMode shutdown = kernel.Run();
    switch (shutdown) {
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
