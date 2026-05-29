/*
 * storage.cpp
 *
 * ZeroM2M — FAT32 Storage Layer
 * Copyright (C) 2026 ZeroM2M Authors
 * GPL-3.0
 */
#include <zerom2m/storage/storage.h>

#include <circle/util.h>   // memcpy, memset, strlen, strcmp
#include <fatfs/ff.h>      // FatFs

// ─── CRC-32 (ISO 3309 / Ethernet poly 0xEDB88320) ────────────────────────────
// Circle exposes ether_crc() but that uses the Ethernet bit-order.
// We implement a tiny table-less CRC32 here so the storage layer is
// self-contained and matches the standard zip/PNG polynomial.

static u32 crc32_byte(u32 crc, u8 byte)
{
    crc ^= byte;
    for (int i = 0; i < 8; i++)
        crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1u));
    return crc;
}

static u32 crc32(const void *data, u32 len)
{
    const u8 *p = static_cast<const u8 *>(data);
    u32 crc = 0xFFFFFFFFu;
    while (len--) crc = crc32_byte(crc, *p++);
    return crc ^ 0xFFFFFFFFu;
}

// ─── Byte-order helpers (big-endian in file) ──────────────────────────────────

static void store_be32(u8 *dst, u32 v)
{
    dst[0] = (v >> 24) & 0xFF;
    dst[1] = (v >> 16) & 0xFF;
    dst[2] = (v >>  8) & 0xFF;
    dst[3] = (v >>  0) & 0xFF;
}

static void store_be64(u8 *dst, u64 v)
{
    store_be32(dst,     static_cast<u32>(v >> 32));
    store_be32(dst + 4, static_cast<u32>(v & 0xFFFFFFFFu));
}

static u32 load_be32(const u8 *src)
{
    return ((u32)src[0] << 24) | ((u32)src[1] << 16) |
           ((u32)src[2] <<  8) | ((u32)src[3]);
}

// Workaround: redefine to avoid forward-ref issue
static u64 load_be64_impl(const u8 *src)
{
    return (((u64)src[0]) << 56) | (((u64)src[1]) << 48) |
           (((u64)src[2]) << 40) | (((u64)src[3]) << 32) |
           (((u64)src[4]) << 24) | (((u64)src[5]) << 16) |
           (((u64)src[6]) <<  8) | (((u64)src[7]));
}

// ─── Utility: safe string copy ────────────────────────────────────────────────

static void safe_strcpy(char *dst, const char *src, u32 maxLen)
{
    u32 i = 0;
    while (i < maxLen - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static void path_join(char *dst, u32 maxLen, const char *a, const char *b)
{
    u32 la = strlen(a);
    safe_strcpy(dst, a, maxLen);
    if (la < maxLen - 2) {
        if (la > 0 && dst[la-1] != '/') { dst[la] = '/'; dst[la+1] = '\0'; la++; }
        safe_strcpy(dst + la, b, maxLen - la);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// LogStore
// ═══════════════════════════════════════════════════════════════════════════════

namespace zerom2m::storage
{

LogStore::LogStore(const char *path, u32 maxInstances)
    : m_maxInstances(maxInstances)
{
    safe_strcpy(m_path, path, sizeof(m_path));
    memset(&m_file, 0, sizeof(m_file));
    memset(m_offsets, 0, sizeof(m_offsets));
}

LogStore::~LogStore()
{
    Close();
}

StorageResult LogStore::Open()
{
    if (m_open) return StorageResult::OK;

    FRESULT fr = f_open(&m_file, m_path, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
    if (fr != FR_OK) return StorageResult::IOError;

    m_open = true;
    return ScanFile();
}

void LogStore::Close()
{
    if (m_open) {
        f_close(&m_file);
        m_open = false;
    }
}

StorageResult LogStore::Sync()
{
    if (!m_open) return StorageResult::IOError;
    FRESULT fr = f_sync(&m_file);
    return (fr == FR_OK) ? StorageResult::OK : StorageResult::IOError;
}

// Scan the file, building an in-memory offset table of valid records.
StorageResult LogStore::ScanFile()
{
    m_count    = 0;
    m_fileSize = 0;

    // Seek to start
    FRESULT fr = f_lseek(&m_file, 0);
    if (fr != FR_OK) return StorageResult::IOError;

    u8 hdrbuf[LOG_RECORD_HEADER_SIZE];

    for (;;) {
        UINT br = 0;
        FSIZE_t pos = f_tell(&m_file);

        // Read header
        fr = f_read(&m_file, hdrbuf, LOG_RECORD_HEADER_SIZE, &br);
        if (fr != FR_OK || br == 0) break;          // EOF or error → stop
        if (br < LOG_RECORD_HEADER_SIZE) break;      // truncated header

        LogRecordHeader hdr;
        hdr.magic      = load_be32(hdrbuf + 0);
        hdr.timestamp  = load_be64_impl(hdrbuf + 4);
        hdr.payloadLen = load_be32(hdrbuf + 12);
        hdr.flags      = load_be32(hdrbuf + 16);

        if (hdr.magic != LOG_RECORD_MAGIC) break;   // corrupt / end of valid data

        // Read payload + CRC into a temporary buffer (limit to 4 KB on stack)
        // For large payloads we validate by streaming.
        u32 totalData = hdr.payloadLen + LOG_RECORD_CRC_SIZE;
        if (totalData > 64 * 1024) break;           // sanity guard

        // Skip payload, read CRC
        fr = f_lseek(&m_file, static_cast<FSIZE_t>(pos) + LOG_RECORD_HEADER_SIZE + hdr.payloadLen);
        if (fr != FR_OK) break;

        u8 crcbuf[4];
        fr = f_read(&m_file, crcbuf, 4, &br);
        if (fr != FR_OK || br < 4) break;

        // To validate CRC we need to re-read header + payload.
        // We do a lightweight check: recompute CRC of header + payload.
        // Re-seek and stream through.
        fr = f_lseek(&m_file, static_cast<FSIZE_t>(pos));
        if (fr != FR_OK) break;

        u32 runCRC = 0xFFFFFFFFu;
        u32 remaining = LOG_RECORD_HEADER_SIZE + hdr.payloadLen;
        u8  chunk[128];
        bool crcOK = true;
        while (remaining > 0) {
            u32 toRead = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
            fr = f_read(&m_file, chunk, toRead, &br);
            if (fr != FR_OK || br != toRead) { crcOK = false; break; }
            for (u32 i = 0; i < br; i++)
                runCRC = crc32_byte(runCRC, chunk[i]);
            remaining -= br;
        }
        runCRC ^= 0xFFFFFFFFu;

        u32 storedCRC = load_be32(crcbuf);
        if (!crcOK || runCRC != storedCRC) break;   // truncated or corrupt; stop here

        // Record is valid — store its offset
        if (m_count < MAX_INDEX_ENTRIES) {
            m_offsets[m_count] = static_cast<u32>(pos);
        } else {
            // Shift window: drop oldest, keep last MAX_INDEX_ENTRIES-1
            memmove(m_offsets, m_offsets + 1, (MAX_INDEX_ENTRIES - 1) * sizeof(u32));
            m_offsets[MAX_INDEX_ENTRIES - 1] = static_cast<u32>(pos);
        }
        m_count++;

        // Advance past CRC to next record
        fr = f_lseek(&m_file, static_cast<FSIZE_t>(pos) + LOG_RECORD_OVERHEAD + hdr.payloadLen);
        if (fr != FR_OK) break;
    }

    m_fileSize = static_cast<u32>(f_size(&m_file));
    return StorageResult::OK;
}

StorageResult LogStore::Append(u64 timestamp, const void *payload, u32 payloadLen)
{
    if (!m_open) return StorageResult::IOError;
    if (!payload || payloadLen == 0) return StorageResult::InvalidArgument;

    // Seek to end
    FRESULT fr = f_lseek(&m_file, f_size(&m_file));
    if (fr != FR_OK) return StorageResult::IOError;

    FSIZE_t recordStart = f_tell(&m_file);

    // Build header
    u8 hdrbuf[LOG_RECORD_HEADER_SIZE];
    store_be32(hdrbuf + 0,  LOG_RECORD_MAGIC);
    store_be64(hdrbuf + 4,  timestamp);
    store_be32(hdrbuf + 12, payloadLen);
    store_be32(hdrbuf + 16, 0u); // flags

    // Compute CRC over header + payload
    u32 crc = 0xFFFFFFFFu;
    for (u32 i = 0; i < LOG_RECORD_HEADER_SIZE; i++)
        crc = crc32_byte(crc, hdrbuf[i]);
    const u8 *pp = static_cast<const u8 *>(payload);
    for (u32 i = 0; i < payloadLen; i++)
        crc = crc32_byte(crc, pp[i]);
    crc ^= 0xFFFFFFFFu;

    u8 crcbuf[4];
    store_be32(crcbuf, crc);

    // Write header
    UINT bw = 0;
    fr = f_write(&m_file, hdrbuf, LOG_RECORD_HEADER_SIZE, &bw);
    if (fr != FR_OK || bw != LOG_RECORD_HEADER_SIZE) return StorageResult::IOError;

    // Write payload
    fr = f_write(&m_file, payload, payloadLen, &bw);
    if (fr != FR_OK || bw != payloadLen) return StorageResult::IOError;

    // Write CRC
    fr = f_write(&m_file, crcbuf, 4, &bw);
    if (fr != FR_OK || bw != 4) return StorageResult::IOError;

    // Update index
    if (m_count < MAX_INDEX_ENTRIES) {
        m_offsets[m_count] = static_cast<u32>(recordStart);
    } else {
        memmove(m_offsets, m_offsets + 1, (MAX_INDEX_ENTRIES - 1) * sizeof(u32));
        m_offsets[MAX_INDEX_ENTRIES - 1] = static_cast<u32>(recordStart);
    }
    m_count++;
    m_fileSize = static_cast<u32>(f_tell(&m_file));

    return StorageResult::OK;
}

StorageResult LogStore::ReadLatest(void *outPayload, u32 *outLen, u64 *outTimestamp) const
{
    if (m_count == 0) return StorageResult::NotFound;
    u32 idx = (m_count <= MAX_INDEX_ENTRIES) ? (m_count - 1) : (MAX_INDEX_ENTRIES - 1);
    return ReadByIndex(m_count - 1, outPayload, outLen, outTimestamp);
}

StorageResult LogStore::ReadByIndex(u32 index, void *outPayload, u32 *outLen, u64 *outTimestamp) const
{
    if (!m_open || !outPayload || !outLen) return StorageResult::InvalidArgument;
    if (m_count == 0 || index >= m_count) return StorageResult::NotFound;

    // Map logical index → offset array index
    u32 arrIdx;
    if (m_count <= MAX_INDEX_ENTRIES) {
        arrIdx = index;
    } else {
        // Only last MAX_INDEX_ENTRIES are in the array; oldest accessible = m_count - MAX_INDEX_ENTRIES
        u32 firstAccessible = m_count - MAX_INDEX_ENTRIES;
        if (index < firstAccessible) return StorageResult::NotFound;
        arrIdx = index - firstAccessible;
    }

    u32 offset = m_offsets[arrIdx];

    // Seek and read header
    FRESULT fr = f_lseek(const_cast<FIL *>(&m_file), static_cast<FSIZE_t>(offset));
    if (fr != FR_OK) return StorageResult::IOError;

    u8 hdrbuf[LOG_RECORD_HEADER_SIZE];
    UINT br = 0;
    fr = f_read(const_cast<FIL *>(&m_file), hdrbuf, LOG_RECORD_HEADER_SIZE, &br);
    if (fr != FR_OK || br < LOG_RECORD_HEADER_SIZE) return StorageResult::IOError;

    LogRecordHeader hdr;
    hdr.magic      = load_be32(hdrbuf + 0);
    hdr.timestamp  = load_be64_impl(hdrbuf + 4);
    hdr.payloadLen = load_be32(hdrbuf + 12);

    if (hdr.magic != LOG_RECORD_MAGIC) return StorageResult::CorruptRecord;
    if (hdr.payloadLen > *outLen) {
        *outLen = hdr.payloadLen;  // tell caller how much is needed
        return StorageResult::BufferTooSmall;
    }

    // Read payload
    fr = f_read(const_cast<FIL *>(&m_file), outPayload, hdr.payloadLen, &br);
    if (fr != FR_OK || br != hdr.payloadLen) return StorageResult::IOError;

    // Read CRC
    u8 crcbuf[4];
    fr = f_read(const_cast<FIL *>(&m_file), crcbuf, 4, &br);
    if (fr != FR_OK || br < 4) return StorageResult::IOError;

    // Validate CRC
    u32 crc = 0xFFFFFFFFu;
    for (u32 i = 0; i < LOG_RECORD_HEADER_SIZE; i++) crc = crc32_byte(crc, hdrbuf[i]);
    const u8 *pp = static_cast<const u8 *>(outPayload);
    for (u32 i = 0; i < hdr.payloadLen; i++) crc = crc32_byte(crc, pp[i]);
    crc ^= 0xFFFFFFFFu;
    if (crc != load_be32(crcbuf)) return StorageResult::CorruptRecord;

    *outLen = hdr.payloadLen;
    if (outTimestamp) *outTimestamp = hdr.timestamp;
    return StorageResult::OK;
}

// ═══════════════════════════════════════════════════════════════════════════════
// MetaStore
// ═══════════════════════════════════════════════════════════════════════════════

MetaStore::MetaStore(const char *baseDir)
{
    safe_strcpy(m_baseDir, baseDir, sizeof(m_baseDir));
}

StorageResult MetaStore::Init()
{
    FRESULT fr = f_mkdir(m_baseDir);
    if (fr != FR_OK && fr != FR_EXIST) return StorageResult::IOError;
    return StorageResult::OK;
}

void MetaStore::BuildPath(char *buf, u32 bufLen, const char *resourceID) const
{
    // baseDir + "/" + resourceID + ".dat"
    char tmp[96];
    path_join(tmp, sizeof(tmp), m_baseDir, resourceID);
    safe_strcpy(buf, tmp, bufLen - 4);
    strcat(buf, ".dat");
}

u32 MetaStore::ComputeCRC32(const void *data, u32 len)
{
    return crc32(data, len);
}

StorageResult MetaStore::Write(const char *resourceID,
                               const char * const *keys,
                               const char * const *values,
                               u32 count)
{
    if (!resourceID || !keys || !values) return StorageResult::InvalidArgument;

    char path[128];
    BuildPath(path, sizeof(path), resourceID);

    FIL f;
    FRESULT fr = f_open(&f, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) return StorageResult::IOError;

    // Build content into a small working buffer; stream write if it overflows
    // (for bare-metal we keep it simple with a fixed 2 KB buffer)
    static char buf[2048];
    u32 pos = 0;

    auto append_str = [&](const char *s) -> bool {
        u32 len = strlen(s);
        if (pos + len >= sizeof(buf)) return false;
        memcpy(buf + pos, s, len);
        pos += len;
        return true;
    };

    for (u32 i = 0; i < count; i++) {
        if (!append_str(keys[i]))   goto io_err;
        if (!append_str("="))       goto io_err;
        if (!append_str(values[i])) goto io_err;
        if (!append_str("\n"))      goto io_err;
    }
    if (!append_str("\n")) goto io_err;  // blank line separator

    {
        u32 dataCRC = ComputeCRC32(buf, pos);
        char crcLine[32];
        // Manual hex formatting (no sprintf in bare-metal)
        const char hex[] = "0123456789abcdef";
        crcLine[0] = 'c'; crcLine[1] = 'r'; crcLine[2] = 'c';
        crcLine[3] = '3'; crcLine[4] = '2'; crcLine[5] = '=';
        for (int b = 7; b >= 0; b--)
            crcLine[6 + (7 - b)] = hex[(dataCRC >> (b * 4)) & 0xF];
        crcLine[14] = '\n'; crcLine[15] = '\0';
        if (!append_str(crcLine)) goto io_err;
    }

    {
        UINT bw = 0;
        fr = f_write(&f, buf, pos, &bw);
        if (fr != FR_OK || bw != pos) goto io_err;
        f_sync(&f);
        f_close(&f);
        return StorageResult::OK;
    }

io_err:
    f_close(&f);
    return StorageResult::IOError;
}

StorageResult MetaStore::Read(const char *resourceID,
                              char (*keys)[64],
                              char (*values)[256],
                              u32 maxPairs,
                              u32 *outCount) const
{
    if (!resourceID || !keys || !values || !outCount) return StorageResult::InvalidArgument;
    *outCount = 0;

    char path[128];
    BuildPath(path, sizeof(path), resourceID);

    FIL f;
    FRESULT fr = f_open(&f, path, FA_READ);
    if (fr != FR_OK) return StorageResult::NotFound;

    static char buf[2048];
    UINT br = 0;
    fr = f_read(&f, buf, sizeof(buf) - 1, &br);
    f_close(&f);
    if (fr != FR_OK) return StorageResult::IOError;
    buf[br] = '\0';

    // Validate CRC: find the "crc32=" line (last line)
    // The CRC covers everything before the crc32 line.
    const char *crcTag = strstr(buf, "\ncrc32=");
    if (!crcTag) return StorageResult::CorruptRecord;
    crcTag++; // skip '\n'
    u32 dataLen = static_cast<u32>(crcTag - buf);

    // Parse stored CRC hex
    const char *hexStr = crcTag + 6; // after "crc32="
    u32 storedCRC = 0;
    for (int i = 0; i < 8 && hexStr[i]; i++) {
        char c = hexStr[i];
        u8 nibble = (c >= '0' && c <= '9') ? (c - '0') :
                    (c >= 'a' && c <= 'f') ? (c - 'a' + 10) :
                    (c >= 'A' && c <= 'F') ? (c - 'A' + 10) : 0;
        storedCRC = (storedCRC << 4) | nibble;
    }

    if (ComputeCRC32(buf, dataLen) != storedCRC) return StorageResult::CorruptRecord;

    // Parse key=value pairs (stop at blank line)
    char *p = buf;
    while (*p && p < buf + dataLen && *outCount < maxPairs) {
        // Skip blank line
        if (*p == '\n') { p++; continue; }

        char *eq = strchr(p, '=');
        if (!eq) break;
        char *nl = strchr(eq, '\n');
        if (!nl) nl = p + strlen(p);

        u32 kLen = static_cast<u32>(eq - p);
        u32 vLen = static_cast<u32>(nl - eq - 1);

        if (kLen >= 64)  kLen = 63;
        if (vLen >= 256) vLen = 255;

        memcpy(keys[*outCount],   p,      kLen); keys[*outCount][kLen]   = '\0';
        memcpy(values[*outCount], eq + 1, vLen); values[*outCount][vLen] = '\0';
        (*outCount)++;

        p = nl + 1;
    }

    return StorageResult::OK;
}

StorageResult MetaStore::Delete(const char *resourceID)
{
    char path[128];
    BuildPath(path, sizeof(path), resourceID);
    FRESULT fr = f_unlink(path);
    return (fr == FR_OK) ? StorageResult::OK : StorageResult::NotFound;
}

bool MetaStore::Exists(const char *resourceID) const
{
    char path[128];
    BuildPath(path, sizeof(path), resourceID);
    FILINFO fi;
    return f_stat(path, &fi) == FR_OK;
}

StorageResult MetaStore::List(char (*outIDs)[64], u32 maxCount, u32 *outCount) const
{
    if (!outIDs || !outCount) return StorageResult::InvalidArgument;
    *outCount = 0;

    DIR dir;
    FRESULT fr = f_opendir(&dir, m_baseDir);
    if (fr != FR_OK) return StorageResult::IOError;

    FILINFO fi;
    while (*outCount < maxCount) {
        fr = f_readdir(&dir, &fi);
        if (fr != FR_OK || fi.fname[0] == 0) break;
        if (fi.fattrib & AM_DIR) continue;

        // Strip ".dat" extension
        u32 len = strlen(fi.fname);
        if (len > 4 && strcmp(fi.fname + len - 4, ".dat") == 0) {
            len -= 4;
            if (len >= 64) len = 63;
            memcpy(outIDs[*outCount], fi.fname, len);
            outIDs[*outCount][len] = '\0';
            (*outCount)++;
        }
    }
    f_closedir(&dir);
    return StorageResult::OK;
}

// ═══════════════════════════════════════════════════════════════════════════════
// StorageManager
// ═══════════════════════════════════════════════════════════════════════════════

StorageManager &StorageManager::Get()
{
    static StorageManager instance;
    return instance;
}

StorageResult StorageManager::Init(const char *rootDrive)
{
    safe_strcpy(m_root, rootDrive, sizeof(m_root));

    // Create /apps and /meta directories
    char appsDir[32], metaDir[32];
    path_join(appsDir, sizeof(appsDir), rootDrive, "apps");
    path_join(metaDir, sizeof(metaDir), rootDrive, "meta");

    EnsureDir(appsDir);
    EnsureDir(metaDir);

    static MetaStore meta(metaDir);
    m_meta = &meta;
    m_meta->Init();

    memset(m_logs, 0, sizeof(m_logs));
    m_init = true;
    return StorageResult::OK;
}

/*static*/ StorageResult StorageManager::EnsureDir(const char *path)
{
    FRESULT fr = f_mkdir(path);
    if (fr == FR_OK || fr == FR_EXIST) return StorageResult::OK;
    return StorageResult::IOError;
}

void StorageManager::BuildLogPath(char *buf, u32 bufLen,
                                  const char *aeID, const char *cntName) const
{
    // root + "apps/" + aeID + "/" + cntName + "/data.log"
    char tmp[128];
    path_join(tmp, sizeof(tmp), m_root, "apps");
    path_join(buf, bufLen, tmp, aeID);
    path_join(tmp, bufLen, buf, cntName);
    path_join(buf, bufLen, tmp, "data.log");
}

StorageResult StorageManager::EnsureContainer(const char *aeID, const char *cntName)
{
    if (!m_init) return StorageResult::IOError;

    char dir[128];
    path_join(dir, sizeof(dir), m_root, "apps");
    EnsureDir(dir);
    path_join(dir, sizeof(dir), dir, aeID);
    EnsureDir(dir);
    path_join(dir, sizeof(dir), dir, cntName);
    EnsureDir(dir);
    return StorageResult::OK;
}

LogStore *StorageManager::GetOrOpenLog(const char *aeID, const char *cntName)
{
    // Search existing open logs
    for (u32 i = 0; i < MAX_OPEN_LOGS; i++) {
        if (m_logs[i].used &&
            strcmp(m_logs[i].aeID, aeID) == 0 &&
            strcmp(m_logs[i].cntName, cntName) == 0) {
            return m_logs[i].store;
        }
    }

    // Find an empty slot
    s32 slot = -1;
    for (u32 i = 0; i < MAX_OPEN_LOGS; i++) {
        if (!m_logs[i].used) { slot = static_cast<s32>(i); break; }
    }
    if (slot < 0) {
        // Evict oldest (slot 0) — close it
        delete m_logs[0].store;
        memmove(m_logs, m_logs + 1, (MAX_OPEN_LOGS - 1) * sizeof(LogEntry));
        slot = MAX_OPEN_LOGS - 1;
        m_logs[slot].used  = false;
        m_logs[slot].store = nullptr;
    }

    char path[128];
    BuildLogPath(path, sizeof(path), aeID, cntName);

    LogStore *ls = new LogStore(path);
    if (ls->Open() != StorageResult::OK) {
        delete ls;
        return nullptr;
    }

    safe_strcpy(m_logs[slot].aeID,   aeID,    sizeof(m_logs[slot].aeID));
    safe_strcpy(m_logs[slot].cntName, cntName, sizeof(m_logs[slot].cntName));
    m_logs[slot].store = ls;
    m_logs[slot].used  = true;
    return ls;
}

StorageResult StorageManager::AppendContent(const char *aeID, const char *cntName,
                                            u64 timestamp,
                                            const void *payload, u32 payloadLen)
{
    LogStore *ls = GetOrOpenLog(aeID, cntName);
    if (!ls) return StorageResult::IOError;
    StorageResult r = ls->Append(timestamp, payload, payloadLen);
    if (r == StorageResult::OK) ls->Sync(); // f_sync after every write
    return r;
}

StorageResult StorageManager::ReadLatest(const char *aeID, const char *cntName,
                                         void *outPayload, u32 *outLen, u64 *outTimestamp)
{
    LogStore *ls = GetOrOpenLog(aeID, cntName);
    if (!ls) return StorageResult::IOError;
    return ls->ReadLatest(outPayload, outLen, outTimestamp);
}

StorageResult StorageManager::ReadByIndex(const char *aeID, const char *cntName,
                                          u32 index,
                                          void *outPayload, u32 *outLen, u64 *outTimestamp)
{
    LogStore *ls = GetOrOpenLog(aeID, cntName);
    if (!ls) return StorageResult::IOError;
    return ls->ReadByIndex(index, outPayload, outLen, outTimestamp);
}

u32 StorageManager::ContentCount(const char *aeID, const char *cntName)
{
    LogStore *ls = GetOrOpenLog(aeID, cntName);
    return ls ? ls->Count() : 0u;
}

StorageResult StorageManager::SyncContainer(const char *aeID, const char *cntName)
{
    LogStore *ls = GetOrOpenLog(aeID, cntName);
    return ls ? ls->Sync() : StorageResult::IOError;
}

StorageResult StorageManager::SyncAll()
{
    for (u32 i = 0; i < MAX_OPEN_LOGS; i++) {
        if (m_logs[i].used && m_logs[i].store)
            m_logs[i].store->Sync();
    }
    return StorageResult::OK;
}

StorageResult StorageManager::WriteMeta(const char *resourceID,
                                        const char * const *keys,
                                        const char * const *values,
                                        u32 count)
{
    if (!m_meta) return StorageResult::IOError;
    return m_meta->Write(resourceID, keys, values, count);
}

StorageResult StorageManager::ReadMeta(const char *resourceID,
                                       char (*keys)[64],
                                       char (*values)[256],
                                       u32 maxPairs,
                                       u32 *outCount)
{
    if (!m_meta) return StorageResult::IOError;
    return m_meta->Read(resourceID, keys, values, maxPairs, outCount);
}

StorageResult StorageManager::DeleteMeta(const char *resourceID)
{
    if (!m_meta) return StorageResult::IOError;
    return m_meta->Delete(resourceID);
}

bool StorageManager::MetaExists(const char *resourceID)
{
    if (!m_meta) return false;
    return m_meta->Exists(resourceID);
}

} // namespace zerom2m::storage
