#include <zerom2m/tasks/stats_task.h>

#include <circle/logger.h>
#include <circle/memory.h>

extern CMemorySystem MemorySystem;

StatsTask::StatsTask(CScheduler &scheduler, SystemStats &stats)
    : CTask()
    , scheduler_(scheduler)
    , stats_(stats)
{
    SetName("stats");
}

void StatsTask::Run()
{
    CScheduler::Get()->MsSleep(5000);

    u64 start = stats_.idleCounter;
    CScheduler::Get()->MsSleep(10000);
    u64 end = stats_.idleCounter;

    const u64 maxIdlePer10s = end - start;

    u64 lastIdle = stats_.idleCounter;

    while (true)
    {
        CScheduler::Get()->MsSleep(10000);

        const u64 currentIdle = stats_.idleCounter;
        const u64 idleDelta = currentIdle - lastIdle;

        lastIdle = currentIdle;

        float idlePct =
            maxIdlePer10s
                ? (100.0f * idleDelta) / (float) maxIdlePer10s
                : 0.0f;

        if (idlePct > 100.0f)
        {
            idlePct = 100.0f;
        }

        const float cpuPct = 100.0f - idlePct;

        const size_t freeHeap =
            MemorySystem.GetHeapFreeSpace(HEAP_ANY);

        CLogger::Get()->Write(
            "stats",
            LogNotice,
            "free_heap=%uKB cpu=%.1f%% idle=%.1f%%",
            (unsigned)(freeHeap / 1024),
            cpuPct,
            idlePct);
    }
}