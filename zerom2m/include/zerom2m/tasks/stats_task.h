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