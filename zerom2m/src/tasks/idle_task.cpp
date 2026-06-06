/*
 * idle_task.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include <zerom2m/tasks/idle_task.h>
#include <circle/sched/scheduler.h>

IdleMonitorTask::IdleMonitorTask(SystemStats& stats)
    : CTask()
    , stats_(stats)
{
    SetName("idle_monitor");
}

void IdleMonitorTask::Run()
{
    while (true)
    {
        ++stats_.idleCounter;

        // Let other tasks run
        CScheduler::Get()->Yield();
    }
}