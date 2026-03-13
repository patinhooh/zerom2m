/*
 * blinktask.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "zerom2m/blinktask.h"
#include <circle/actled.h>
#include <circle/sched/scheduler.h>
#include <circle/sched/task.h>

namespace zerom2m
{

BlinkTask::BlinkTask(CActLED *pLED, unsigned intervalMs)
    : CTask()
    , led_(pLED)
    , intervalMs_(intervalMs)
{
}

BlinkTask::~BlinkTask() { led_ = nullptr; }

void BlinkTask::Run()
{
    while (true) {
        led_->On();
        CScheduler::Get()->MsSleep(intervalMs_);

        led_->Off();
        CScheduler::Get()->MsSleep(intervalMs_);
    }
}

} // namespace zerom2m
