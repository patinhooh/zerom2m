/*
 * storage.h
 *
 * ZeroM2M — FAT32 Storage Layer
 * Copyright (C) 2026 ZeroM2M Authors
 * GPL-3.0
 *
 * Architecture:
 *   StorageManager
 *     ├── MetaStore   — mutable resources (AE, Container, Subscription)
 *     │                 stored as serialised key=value records in /meta/<name>.dat
 *     └── LogStore    — ContentInstance append-only log per container
 *                       /apps/<ae>/<cnt>/data.log  (binary records + CRC32)
 *
 * FAT32 access uses Circle's FatFs wrapper (fatfs/ff.h).
 * Call StorageManager::Get() once after the filesystem is mounted.
 */
#pragma once

#include <circle/util.h>          // memcpy, memset, strlen …
#include <fatfs/ff.h>             // f_open, f_write, f_sync, f_close …

#include <zerom2m/compat/types.h> // CString, Vector, Optional

namespace zerom2m::storage
{

using zerom2m::compat::Optional;
using zerom2m::compat::Vector;

// ─── Result type ──────────────────────────────────────────────────────────────

enum class StorageResult : u8 {
    OK = 0,
    NotFound,
    IOError,
    CorruptRecord,   // CRC mismatch on read
    BufferTooSmall,
    InvalidArgument,
    OutOfSpace,
};

// ─── Binary record layout (ContentInstance log) ───────────────────────────────
//
//  Offset  Size  Field
//  0       4     magic   = 0x5A4D3231  ('ZM21')
//  4       8     timestamp  (Unix seconds, big-endian u64)
//  12      4     payloadLen (bytes, big-endian u32)
//  16      4     flags      (reserved, set to 0)
//  20      N     payload    (raw bytes; typically JSON/plain-text content)
//  20+N    4     crc32      (CRC32 of bytes [0 .. 20+N-1])
//  ─────────────────────────────────────────────────────────
//  Total   24+N bytes per record
//
// On power-loss the last record may be incomplete; ReadLatest / ReadByIndex
// validate the CRC and skip truncated records automatically.

static constexpr u32 LOG_RECORD_MAGIC = 0x5A4D3231u;

struct LogRecordHeader {
    u32 magic;       // LOG_RECORD_MAGIC
    u64 timestamp;   // Unix seconds (big-endian in file)
    u32 payloadLen;  // bytes that follow (big-endian in file)
    u32 flags;       // reserved
} __attribute__((packed));

static constexpr u32 LOG_RECORD_HEADER_SIZE = sizeof(LogRecordHeader); // 20
static constexpr u32 LOG_RECORD_CRC_SIZE    = 4;
static constexpr u32 LOG_RECORD_OVERHEAD    = LOG_RECORD_HEADER_SIZE + LOG_RECORD_CRC_SIZE; // 24

// ─── LogStore ─────────────────────────────────────────────────────────────────
//  Manages one append-only log file:  /apps/<ae>/<cnt>/data.log

class LogStore
{
public:
    // path  — full FAT path, e.g. "0:/apps/myapp/temp/data.log"
    // maxInstances — 0 = unlimited; when > 0 the oldest record is
    //                reported as overflow (no automatic deletion is done
    //                to keep the implementation simple; the caller decides)
    explicit LogStore(const char *path, u32 maxInstances = 0);
    ~LogStore();

    // Non-copyable
    LogStore(const LogStore &) = delete;
    LogStore &operator=(const LogStore &) = delete;

    // Open (or create) the log file.  Must be called before any other method.
    StorageResult Open();
    void          Close();

    // Append one ContentInstance payload.
    // timestamp: seconds since epoch (use circle timer or a fixed base).
    StorageResult Append(u64 timestamp, const void *payload, u32 payloadLen);

    // Read the most-recently appended valid record.
    // outPayload must be pre-allocated; *outLen in = capacity, out = actual.
    StorageResult ReadLatest(void *outPayload, u32 *outLen, u64 *outTimestamp = nullptr) const;

    // Read by zero-based index (0 = oldest, Count()-1 = newest).
    StorageResult ReadByIndex(u32 index,
                              void *outPayload, u32 *outLen,
                              u64  *outTimestamp = nullptr) const;

    // Number of valid records currently in the log.
    u32 Count() const { return m_count; }

    // Bytes currently used by the log file.
    u32 ByteSize() const { return m_fileSize; }

    // Flush OS write-cache to SD card (call after every Append or periodically).
    StorageResult Sync();

private:
    StorageResult ScanFile();   // builds m_offsets[] index on Open
    StorageResult SeekAndRead(u32 offset, void *buf, u32 len) const;
    bool          ValidateRecord(const LogRecordHeader &hdr,
                                 const void *payload,
                                 u32 crcFromFile) const;

    char  m_path[128];
    u32   m_maxInstances;
    FIL   m_file;
    bool  m_open{false};

    // In-memory index: byte offsets of each valid record header in the file.
    // We keep up to MAX_INDEX_ENTRIES entries; if Count() > MAX_INDEX_ENTRIES
    // only the most-recent MAX_INDEX_ENTRIES records are accessible by index
    // (ReadLatest always works regardless).
    static constexpr u32 MAX_INDEX_ENTRIES = 1024;
    u32  m_offsets[MAX_INDEX_ENTRIES];
    u32  m_count{0};
    u32  m_fileSize{0};
};

// ─── MetaRecord format ────────────────────────────────────────────────────────
//  Simple text-based key=value store, one attribute per line, terminated by
//  a blank line + CRC32 footer:
//
//    key1=value1\n
//    key2=value2\n
//    \n
//    crc32=XXXXXXXX\n   (hex CRC32 of all preceding bytes incl. the blank line)
//
//  The whole resource is stored in a single flat file under /meta/.
//  Overwrites rewrite the whole file (resources are small; tens of fields).

// ─── MetaStore ────────────────────────────────────────────────────────────────

class MetaStore
{
public:
    // baseDir — e.g. "0:/meta"
    explicit MetaStore(const char *baseDir);

    // Ensure base directory exists.
    StorageResult Init();

    // Write (create or overwrite) a resource record.
    // resourceID — used as filename:  baseDir/<resourceID>.dat
    // pairs      — flat array of alternating key/value CStrings
    StorageResult Write(const char *resourceID,
                        const char * const *keys,
                        const char * const *values,
                        u32 count);

    // Read a resource into key/value pairs.
    // keys[] and values[] must be pre-allocated arrays of `maxPairs` CStrings.
    StorageResult Read(const char *resourceID,
                       char (*keys)[64],
                       char (*values)[256],
                       u32 maxPairs,
                       u32 *outCount) const;

    // Delete a resource record.
    StorageResult Delete(const char *resourceID);

    // Check existence without reading content.
    bool Exists(const char *resourceID) const;

    // List all resource IDs in the store (fills outIDs up to maxCount).
    StorageResult List(char (*outIDs)[64], u32 maxCount, u32 *outCount) const;

private:
    void BuildPath(char *buf, u32 bufLen, const char *resourceID) const;
    static u32 ComputeCRC32(const void *data, u32 len);

    char m_baseDir[64];
};

// ─── StorageManager ───────────────────────────────────────────────────────────
//  Singleton façade.
//  Usage:
//    StorageManager::Get().Init("0:");       // once after FS mount
//    StorageManager::Get().EnsureContainer("myAE", "readings");
//    StorageManager::Get().AppendContent("myAE", "readings", now, buf, len);

class StorageManager
{
public:
    static StorageManager &Get();

    // Must be called once after the FAT32 filesystem is mounted.
    // rootDrive: "0:" (FatFs drive number as string)
    StorageResult Init(const char *rootDrive = "0:");

    // ── ContentInstance log operations ──────────────────────────────────────

    // Create directory structure for <ae>/<container> if it doesn't exist.
    StorageResult EnsureContainer(const char *aeID, const char *cntName);

    // Append a ContentInstance payload to the container log.
    StorageResult AppendContent(const char *aeID, const char *cntName,
                                u64 timestamp,
                                const void *payload, u32 payloadLen);

    // Read latest ContentInstance.
    StorageResult ReadLatest(const char *aeID, const char *cntName,
                             void *outPayload, u32 *outLen,
                             u64  *outTimestamp = nullptr);

    // Read ContentInstance by index (0 = oldest).
    StorageResult ReadByIndex(const char *aeID, const char *cntName,
                              u32 index,
                              void *outPayload, u32 *outLen,
                              u64  *outTimestamp = nullptr);

    // Number of ContentInstances in container.
    u32 ContentCount(const char *aeID, const char *cntName);

    // Flush a container log to SD.
    StorageResult SyncContainer(const char *aeID, const char *cntName);

    // Flush ALL open logs and meta files (call periodically or on shutdown).
    StorageResult SyncAll();

    // ── Mutable resource meta operations ────────────────────────────────────

    StorageResult WriteMeta(const char *resourceID,
                            const char * const *keys,
                            const char * const *values,
                            u32 count);

    StorageResult ReadMeta(const char *resourceID,
                           char (*keys)[64],
                           char (*values)[256],
                           u32 maxPairs,
                           u32 *outCount);

    StorageResult DeleteMeta(const char *resourceID);
    bool          MetaExists(const char *resourceID);

private:
    StorageManager() = default;

    // Find or open a LogStore for the given container.
    // Returns nullptr on error.
    LogStore *GetOrOpenLog(const char *aeID, const char *cntName);

    void BuildLogPath(char *buf, u32 bufLen,
                      const char *aeID, const char *cntName) const;

    static StorageResult EnsureDir(const char *path);

    static constexpr u32 MAX_OPEN_LOGS = 8;

    struct LogEntry {
        char     aeID[32];
        char     cntName[64];
        LogStore *store{nullptr};
        bool     used{false};
    };

    LogEntry  m_logs[MAX_OPEN_LOGS];
    MetaStore *m_meta{nullptr};
    char      m_root[8];   // e.g. "0:"
    bool      m_init{false};
};

} // namespace zerom2m::storage
