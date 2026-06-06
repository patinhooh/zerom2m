/*
 * stats_task.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <circle/sched/scheduler.h>
#include <zerom2m/tasks/idle_task.h>

class StatsTask : public CTask
{
public:
    StatsTask(CScheduler& scheduler, SystemStats& stats);

protected:
    void Run() override;

private:
    CScheduler& scheduler_;
    SystemStats& stats_;
};