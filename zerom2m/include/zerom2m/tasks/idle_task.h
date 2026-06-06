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