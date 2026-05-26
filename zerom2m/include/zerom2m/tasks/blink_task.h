/*
 * blink_task.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <circle/actled.h>
#include <circle/sched/scheduler.h>
#include <circle/sched/task.h>

namespace zerom2m::tasks
{

class BlinkTask : public CTask
{
public:
    /**
     * @brief Construct a new BlinkTask
     *
     * @param led Pointer to the LED device to blink
     * @param intervalMs Blink interval in milliseconds
     */
    BlinkTask(CScheduler *scheduler, CActLED *led, unsigned intervalMs);

    ~BlinkTask();

protected:
    void Run() override;

private:
    CScheduler *scheduler_;
    CActLED *led_;
    unsigned intervalMs_;
};

} // namespace zerom2m::tasks
