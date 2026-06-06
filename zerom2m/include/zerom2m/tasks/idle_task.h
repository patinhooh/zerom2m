/*
 * idle_task.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <circle/sched/task.h>

struct SystemStats
{
    volatile u64 idleCounter = 0;
};

class IdleMonitorTask : public CTask
{
public:
    explicit IdleMonitorTask(SystemStats& stats);

protected:
    void Run() override;

private:
    SystemStats& stats_;
};