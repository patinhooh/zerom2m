/*
 * blink_task.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include <zerom2m/tasks/blink_task.h>

#include <circle/actled.h>
#include <circle/sched/scheduler.h>
#include <circle/sched/task.h>

namespace zerom2m::tasks
{

BlinkTask::BlinkTask(CScheduler *scheduler, CActLED *led, unsigned intervalMs)
    : CTask()
    , scheduler_(scheduler)
    , led_(led)
    , intervalMs_(intervalMs)
{
}

BlinkTask::~BlinkTask() { led_ = nullptr; }

void BlinkTask::Run()
{
    while (true) {
        led_->On();
        scheduler_->MsSleep(intervalMs_);

        led_->Off();
        scheduler_->MsSleep(intervalMs_);
    }
}

} // namespace zerom2m::tasks
