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