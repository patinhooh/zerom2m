/*
 * sqlite_vfs.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include <circle/string.h>
#include <circle/util.h>
#include <fatfs/ff.h>
#include <zerom2m/sqlite/sqlite3.h>

#include <circle/logger.h>

// ---------------------------------------------------------------------------
// File handle layout
//
// SQLite allocates exactly szOsFile bytes for each sqlite3_file.  We need our
// FatFs state to live inside that allocation, so we wrap sqlite3_file as the
// FIRST member of CircleFile.  That makes a CircleFile* and a sqlite3_file*
// point to the same address, which is what SQLite expects.
// ---------------------------------------------------------------------------
struct CircleFile {
    sqlite3_file base; // MUST be first – SQLite passes us a sqlite3_file*
    FIL          fil;
    int          valid;
};

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

// Helper: cast the sqlite3_file* SQLite hands us back to our wrapper
static inline CircleFile *toCircleFile(sqlite3_file *p)
{ return reinterpret_cast<CircleFile *>(p); }

// ---------------------------------------------------------------------------
// sqlite3_io_methods implementations
// ---------------------------------------------------------------------------

static int circleClose(sqlite3_file *pFile)
{
    CircleFile *f = toCircleFile(pFile);
    if (f->valid) {
        f_close(&f->fil);
        f->valid = 0;
    }
    // SQLite owns the memory (it allocated szOsFile bytes); do NOT delete.
    return SQLITE_OK;
}

static int circleRead(sqlite3_file *pFile, void *zBuf, int iAmt, sqlite3_int64 iOfst)
{
    CircleFile *f = toCircleFile(pFile);
    if (!f->valid) return SQLITE_IOERR_READ;

    FSIZE_t fileSize = f_size(&f->fil);

    // Empty file = new database. Signal short read so SQLite initializes it.
    if (fileSize == 0) {
        memset(zBuf, 0, iAmt);
        return SQLITE_IOERR_SHORT_READ;
    }

    if (f_lseek(&f->fil, (FSIZE_t)iOfst) != FR_OK) return SQLITE_IOERR_READ;

    UINT    nRead = 0;
    FRESULT res   = f_read(&f->fil, zBuf, (UINT)iAmt, &nRead);
    if (res != FR_OK) return SQLITE_IOERR_READ;

    if ((int)nRead == iAmt) return SQLITE_OK;

    memset(static_cast<char *>(zBuf) + nRead, 0, (size_t)(iAmt - nRead));
    return SQLITE_IOERR_SHORT_READ;
}

static int circleWrite(sqlite3_file *pFile, const void *zBuf, int iAmt, sqlite3_int64 iOfst)
{
    CircleFile *f = toCircleFile(pFile);
    if (!f->valid) return SQLITE_IOERR_WRITE;

    FRESULT rs = f_lseek(&f->fil, (FSIZE_t)iOfst);
    if (rs != FR_OK) return SQLITE_IOERR_WRITE;

    UINT    nWritten = 0;
    FRESULT res      = f_write(&f->fil, zBuf, (UINT)iAmt, &nWritten);
    if (res != FR_OK || (int)nWritten != iAmt) return SQLITE_IOERR_WRITE;

    return SQLITE_OK;
}

static int circleTruncate(sqlite3_file *pFile, sqlite3_int64 size)
{
    CircleFile *f = toCircleFile(pFile);
    if (!f->valid) return SQLITE_IOERR_TRUNCATE;

    if (f_lseek(&f->fil, (FSIZE_t)size) != FR_OK) return SQLITE_IOERR_TRUNCATE;
    if (f_truncate(&f->fil) != FR_OK) return SQLITE_IOERR_TRUNCATE;

    return SQLITE_OK;
}

static int circleSync(sqlite3_file *pFile, int /*flags*/)
{
    CircleFile *f = toCircleFile(pFile);
    if (!f->valid) return SQLITE_OK;
    FRESULT res = f_sync(&f->fil);
    if (res != FR_OK) {
        CLogger::Get()->Write("vfs", LogError, "f_sync failed: %d", res);
        return SQLITE_IOERR_FSYNC;
    }
    return SQLITE_OK;
}

static int circleFileSize(sqlite3_file *pFile, sqlite3_int64 *pSize)
{
    CircleFile *f = toCircleFile(pFile);
    if (!f->valid) return SQLITE_IOERR;

    // f_size() returns stale value until sync — seek to end to get real size
    if (f_lseek(&f->fil, f_size(&f->fil)) != FR_OK) return SQLITE_IOERR;
    *pSize = (sqlite3_int64)f_tell(&f->fil);
    return SQLITE_OK;
}

// Single-process bare-metal: locking is a no-op
static int circleLock(sqlite3_file *, int) { return SQLITE_OK; }
static int circleUnlock(sqlite3_file *, int) { return SQLITE_OK; }
static int circleCheckReservedLock(sqlite3_file *, int *pOut)
{
    *pOut = 0;
    return SQLITE_OK;
}

static int circleFileControl(sqlite3_file *, int, void *) { return SQLITE_NOTFOUND; }
static int circleSectorSize(sqlite3_file *) { return 512; }
static int circleDeviceCharacteristics(sqlite3_file *) { return 0; }

static const sqlite3_io_methods circleMethods = {
    1,
    circleClose,
    circleRead,
    circleWrite,
    circleTruncate,
    circleSync,
    circleFileSize,
    circleLock,
    circleUnlock,
    circleCheckReservedLock,
    circleFileControl,
    circleSectorSize,
    circleDeviceCharacteristics,
    nullptr, // xShmMap
    nullptr, // xShmLock
    nullptr, // xShmBarrier
    nullptr, // xShmUnmap
    nullptr, // xFetch
    nullptr  // xUnfetch
};

// ---------------------------------------------------------------------------
// sqlite3_vfs implementations
// ---------------------------------------------------------------------------

static int circleOpen(
    sqlite3_vfs * /*pVfs*/, const char *zName, sqlite3_file *pFile, int flags, int *pOutFlags)
{
    // CLogger::Get()->Write(
    //     "vfs", LogNotice, "OPEN called: '%s' flags=0x%x", zName ? zName : "(null)", flags);
    CircleFile *f = toCircleFile(pFile);
    f->valid      = 0;
    // pMethods must be set even on failure so SQLite can call xClose safely
    pFile->pMethods = nullptr;

    const char *normalizedPath = zName;
    if (!normalizedPath) return SQLITE_CANTOPEN;

    // Build FatFs open flags from SQLite open flags
    BYTE fflags = 0;

    if (flags & SQLITE_OPEN_READONLY) {
        fflags = FA_READ | FA_OPEN_EXISTING;
    } else {
        // Writable open
        fflags = FA_READ | FA_WRITE;

        if (flags & SQLITE_OPEN_CREATE) {
            fflags |= (flags & SQLITE_OPEN_EXCLUSIVE) ? FA_CREATE_NEW : FA_OPEN_ALWAYS;
        } else {
            fflags |= FA_OPEN_EXISTING;
        }
    }

    FRESULT res = f_open(&f->fil, zName, fflags);
    if (res != FR_OK) return SQLITE_CANTOPEN;

    f->valid        = 1;
    pFile->pMethods = &circleMethods;
    if (pOutFlags) *pOutFlags = flags;
    return SQLITE_OK;
}

static int circleDelete(sqlite3_vfs * /*pVfs*/, const char *zName, int /*syncDir*/)
{
    // f_unlink does not take a FATFS* — it uses the drive prefix in the path
    FRESULT res = f_unlink(zName);
    return (res == FR_OK) ? SQLITE_OK : SQLITE_IOERR_DELETE;
}

static int circleAccess(sqlite3_vfs * /*pVfs*/, const char *zName, int /*flags*/, int *pResOut)
{
    FILINFO finfo;
    FRESULT res = f_stat(zName, &finfo);
    *pResOut    = (res == FR_OK) ? 1 : 0;
    return SQLITE_OK;
}

static int circleFullPathname(sqlite3_vfs * /*pVfs*/, const char *zName, int nOut, char *zOut)
{
    sqlite3_snprintf(nOut, zOut, "%s", zName);
    return SQLITE_OK;
}

// Dynamic loading: not supported on bare-metal
static void *circleDlOpen(sqlite3_vfs *, const char *) { return nullptr; }
static void  circleDlError(sqlite3_vfs *, int n, char *z)
{ sqlite3_snprintf(n, z, "dlopen not supported"); }
static void (*circleDlSym(sqlite3_vfs *, void *, const char *))(void) { return nullptr; }
static void circleDlClose(sqlite3_vfs *, void *) {}

static int circleRandomness(sqlite3_vfs * /*pVfs*/, int nByte, char *zBuf)
{
    static unsigned seed = 0x12345678;
    for (int i = 0; i < nByte; i++) {
        seed    = seed * 1103515245 + 12345;
        zBuf[i] = (char)((seed >> 16) & 0xFF);
    }

    return nByte;
}

static int circleSleep(sqlite3_vfs * /*pVfs*/, int microseconds)
{
    // Bare-metal busy-wait; replace with CTimer::SimpleusDelay if available
    volatile unsigned delay = (unsigned)microseconds * 5u;
    while (delay--) {}
    return microseconds;
}

static int circleCurrentTime(sqlite3_vfs * /*pVfs*/, double *prNow)
{
    // Fixed epoch offset — replace with real RTC if available.
    // 2440587.5 = Julian day of Unix epoch (1970-01-01).
    // Returning that gives SQLite a valid, stable timestamp.
    *prNow = 2440587.5;
    return SQLITE_OK;
}

static int circleCurrentTimeInt64(sqlite3_vfs *pVfs, sqlite3_int64 *piNow)
{
    double now = 0.0;
    circleCurrentTime(pVfs, &now);
    *piNow = (sqlite3_int64)(now * 86400000.0);
    return SQLITE_OK;
}

static sqlite3_vfs circleVfs = {
    3,                  /* iVersion */
    sizeof(CircleFile), /* szOsFile – must fit our full CircleFile struct */
    256,                /* mxPathname */
    nullptr,            /* pNext (managed by SQLite) */
    "circle",           /* zName */
    nullptr,            /* pAppData */
    circleOpen,
    circleDelete,
    circleAccess,
    circleFullPathname,
    circleDlOpen,
    circleDlError,
    circleDlSym,
    circleDlClose,
    circleRandomness,
    circleSleep,
    circleCurrentTime,
    nullptr,                /* xGetLastError (v2) */
    circleCurrentTimeInt64, /* xCurrentTimeInt64 (v3) */
    nullptr,                // xSetSystemCall
    nullptr,                // xGetSystemCall
    nullptr                 // xNextSystemCall
};

bool RegisterCircleVfs()
{
    int i = sqlite3_vfs_register(&circleVfs, 0);

    sqlite3_vfs *vfs = sqlite3_vfs_find("circle");
    CLogger::Get()->Write("vfs", LogNotice, "registered: %p", vfs);

    return i == SQLITE_OK;
}

// Called by SQLite during sqlite3_initialize(), but we call it on kernel init to ensure our VFS is
// registered before any DB operations.
extern "C" int sqlite3_os_init(void) { return SQLITE_OK; }
extern "C" int sqlite3_os_end(void) { return SQLITE_OK; }
