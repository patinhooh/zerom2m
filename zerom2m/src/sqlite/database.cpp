/*
 * database.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "zerom2m/sqlite/database.h"

#include <circle/logger.h>
#include <circle/string.h>
#include <fatfs/ff.h>
#include <zerom2m/onem2m/types/resources.h>

using namespace zerom2m::sqlite;
using namespace zerom2m::onem2m::types;

static const char *kCreateResources = "CREATE TABLE IF NOT EXISTS resources ("
                                      "ri TEXT PRIMARY KEY,"
                                      "rn TEXT,"
                                      "ty INTEGER,"
                                      "pi TEXT,"
                                      "ct TEXT,"
                                      "lt TEXT,"
                                      "et TEXT,"
                                      "labels TEXT,"
                                      "acpi TEXT,"
                                      "custodian TEXT,"
                                      "daci TEXT,"
                                      "announceTo TEXT,"
                                      "announcedAttribute TEXT,"
                                      "announceSyncType INTEGER,"
                                      "path TEXT" // EXTRA column for efficient lookup
                                      ");";

static const char *kCreateCSE = "CREATE TABLE IF NOT EXISTS cse ("
                                "ri  TEXT PRIMARY KEY,"
                                "cst INTEGER," // cseType
                                "csi TEXT,"    // cseID
                                "srt TEXT,"    // supportedResourceType (packed list of integers)
                                "srv TEXT,"    // supportedReleaseVersions (packed list)
                                "ctm TEXT,"    // currentTime
                                "poa TEXT,"    // pointOfAccess (packed list)
                                "ncp TEXT,"    // notifCongestionPolicy
                                "nl  TEXT,"    // nodeLink
                                "esi TEXT"     // e2eSecInfo
                                ");";

static const char *kCreateAE =
    "CREATE TABLE IF NOT EXISTS ae (ri TEXT PRIMARY KEY, api TEXT, aei TEXT, apn TEXT, poa TEXT, "
    "ontologyRef TEXT, nl TEXT, csz TEXT, regs INTEGER, rr INTEGER, mei TEXT, tri TEXT, trn "
    "INTEGER, srv TEXT, esi TEXT, tren INTEGER);";

static const char *kCreateContainer =
    "CREATE TABLE IF NOT EXISTS container (ri TEXT PRIMARY KEY, st INTEGER, cr TEXT, mni INTEGER, "
    "mbs INTEGER, mbis INTEGER, mia INTEGER, cni INTEGER, cbs INTEGER, ontologyRef TEXT, disr "
    "INTEGER, li TEXT, loc TEXT);";

static const char *kCreateContentInstance =
    "CREATE TABLE IF NOT EXISTS content_instance (ri TEXT PRIMARY KEY, st INTEGER, cr TEXT, cnf "
    "TEXT, cs INTEGER, conr TEXT, con BLOB, ontologyRef TEXT, dgt TEXT, dcnt INTEGER);";

static const char *kCreateSubscription =
    "CREATE TABLE IF NOT EXISTS subscription ("
    "ri TEXT PRIMARY KEY, "
    // EventNotificationCriteria (enc_)
    "enc_cb TEXT, enc_ca TEXT, enc_ms TEXT, enc_us TEXT, enc_sts INTEGER, enc_stb INTEGER, "
    "enc_eb TEXT, enc_ea TEXT, enc_labels TEXT, enc_sa INTEGER, enc_sb INTEGER, "
    "enc_net TEXT, enc_chty TEXT, enc_atr TEXT, enc_fu INTEGER, enc_cfq TEXT, enc_cfs TEXT, enc_md "
    "INTEGER, "
    // RateLimit (rtl_)
    "rtl_max INTEGER, rtl_tw INTEGER, "
    // BatchNotify (btn_)
    "btn_num INTEGER, btn_dur TEXT, "
    // Outros campos originais
    "exc INTEGER, nu TEXT, gpi TEXT, nfu TEXT, psn INTEGER, pn INTEGER, nsp INTEGER, "
    "ln INTEGER, nct INTEGER, nec INTEGER, su TEXT, cr TEXT, crp INTEGER,gn TEXT, acrs TEXT);";

Database::Database() {}

Database::~Database() { Close(); }

bool Database::Open(const char *path, CString &err)
{
    CLogger::Get()->Write("database", LogNotice, "Opening DB at path: %s", path);

    // If the file exists but is zero bytes, remove it so SQLite can create a valid DB.
    {
        FILINFO finfo;
        FRESULT fres = f_stat(path, &finfo);
        if (fres == FR_OK && finfo.fsize == 0) {
            CLogger::Get()->Write("database", LogNotice, "Removing zero-length DB file: %s", path);
            f_unlink(path);
        }
    }

    if (sqlite3_initialize() != SQLITE_OK) {
        err = "Failed to initialize SQLite";
        db_ = nullptr;
        return false;
    }
    if (sqlite3_open(path, &db_) != SQLITE_OK) {
        err = CString(sqlite3_errmsg(db_));
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }
    // Set journal mode and synchronous BEFORE pager touches anything
    // Use DELETE journal and FULL synchronous to ensure SQLite calls xSync
    // (our circleSync calls `f_sync`) on commits so data reaches the media.
    sqlite3_exec(db_, "PRAGMA journal_mode = DELETE;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA page_size = 4096;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous = FULL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA temp_store = MEMORY;", nullptr, nullptr, nullptr);
    return true;
}

void Database::Close()
{
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::InitSchema()
{
    if (!db_) {
        CLogger::Get()->Write("database", LogError, "DB not open");
        return false;
    }
    char *zErr = nullptr;
    int   rc;
    rc = sqlite3_exec(db_, kCreateResources, nullptr, nullptr, &zErr);
    if (rc != SQLITE_OK) {
        CLogger::Get()->Write("database", LogError, "Failed to create resources table: %s", zErr);
        sqlite3_free(zErr);
        return false;
    }
    rc = sqlite3_exec(db_, kCreateCSE, nullptr, nullptr, &zErr);
    if (rc != SQLITE_OK) {
        CLogger::Get()->Write("database", LogError, "Failed to create cse table: %s", zErr);
        sqlite3_free(zErr);
        return false;
    }
    rc = sqlite3_exec(db_, kCreateAE, nullptr, nullptr, &zErr);
    if (rc != SQLITE_OK) {
        CLogger::Get()->Write("database", LogError, "Failed to create ae table: %s", zErr);
        sqlite3_free(zErr);
        return false;
    }
    rc = sqlite3_exec(db_, kCreateContainer, nullptr, nullptr, &zErr);
    if (rc != SQLITE_OK) {
        CLogger::Get()->Write("database", LogError, "Failed to create container table: %s", zErr);
        sqlite3_free(zErr);
        return false;
    }
    rc = sqlite3_exec(db_, kCreateContentInstance, nullptr, nullptr, &zErr);
    if (rc != SQLITE_OK) {
        CLogger::Get()->Write(
            "database", LogError, "Failed to create content instance table: %s", zErr);
        sqlite3_free(zErr);
        return false;
    }
    rc = sqlite3_exec(db_, kCreateSubscription, nullptr, nullptr, &zErr);
    if (rc != SQLITE_OK) {
        CLogger::Get()->Write(
            "database", LogError, "Failed to create subscription table: %s", zErr);
        sqlite3_free(zErr);
        return false;
    }
    return true;
}

// lightweight packer: use unit separator 0x1F between entries
CString Database::VecToPacked(const Vector<CString> &v)
{
    CString out;
    for (unsigned i = 0; i < v.GetCount(); ++i) {
        if (i) out += (char)0x1F;
        out += v[i];
    }
    return out;
}

void Database::PackedToVec(const CString &s, Vector<CString> &out)
{
    out.clear();
    CString cur;
    for (size_t i = 0; i < s.GetLength(); ++i) {
        char c = s.c_str()[i];
        if (c == (char)0x1F) {
            out.push_back(cur);
            cur = CString();
        } else {
            char buf[2] = {c, '\0'};
            cur += buf;
        }
    }
    if (s.GetLength() > 0) out.push_back(cur);
}

template <typename T> CString EnumVecToPacked(const Vector<T> &v)
{
    CString out;

    for (unsigned i = 0; i < v.GetCount(); ++i) {
        if (i) out += (char)0x1F;
        CString format;
        format.Format("%u", static_cast<u32>(v[i]));
        out += format;
    }

    return out;
}

template <typename T> void PackedToEnumVec(const CString &s, Vector<T> &out)
{
    out.clear();
    CString cur;

    for (size_t i = 0; i < s.GetLength(); ++i) {
        char c = s.c_str()[i];

        if (c == (char)0x1F) {
            if (cur.GetLength() != 0) {
                out.push_back(static_cast<T>(atoi(cur.c_str())));
                cur = CString();
            }
        } else {
            char buf[2] = {c, '\0'};
            cur += buf;
        }
    }

    if (cur.GetLength() != 0) out.push_back(static_cast<T>(atoi(cur.c_str())));
}

static int bind_text_or_null(sqlite3_stmt *stmt, int idx, const CString &v)
{
    if (v.GetLength() == 0) return sqlite3_bind_null(stmt, idx);
    return sqlite3_bind_text(stmt, idx, v.c_str(), -1, SQLITE_TRANSIENT);
}

static CString column_text_or_empty(sqlite3_stmt *stmt, int idx)
{
    const unsigned char *txt = sqlite3_column_text(stmt, idx);
    if (!txt) return CString();
    return CString(reinterpret_cast<const char *>(txt));
}

static void packed_to_vec_local(const CString &s, Vector<CString> &out)
{
    out.clear();
    CString cur;
    for (size_t i = 0; i < s.GetLength(); ++i) {
        char c = s.c_str()[i];
        if (c == (char)0x1F) {
            out.push_back(cur);
            cur = CString();
        } else {
            char buf[2] = {c, '\0'};
            cur += buf;
        }
    }
    if (s.GetLength() > 0) out.push_back(cur);
}

static void apply_common_resource_base(ResourceBase  &r,
                                       const CString &ri,
                                       const CString &rn,
                                       const CString &pi,
                                       const CString &ct,
                                       const CString &lt,
                                       const CString &et,
                                       const CString &labels,
                                       const CString &acpi,
                                       const CString &custodian,
                                       const CString &daci,
                                       const CString &announceTo,
                                       const CString &announcedAttribute,
                                       bool           hasAnnounceSyncType,
                                       int            announceSyncType)
{
    r.resourceID       = ri;
    r.resourceName     = rn;
    r.parentID         = pi;
    r.creationTime     = ct;
    r.lastModifiedTime = lt;
    if (et.GetLength() > 0) r.expirationTime = et;
    packed_to_vec_local(labels, r.labels);
    packed_to_vec_local(acpi, r.accessControlPolicyIDs);
    if (custodian.GetLength() > 0) r.custodian = custodian;
    packed_to_vec_local(daci, r.dynamicAuthConsultIDs);
    if (announceTo.GetLength() > 0) r.announceTo = announceTo;
    packed_to_vec_local(announcedAttribute, r.announcedAttribute);
    if (hasAnnounceSyncType) r.announceSyncType = static_cast<u8>(announceSyncType);
}

static int bind_text_or_null(sqlite3_stmt *stmt, int idx, const Optional<CString> &v)
{
    if (!v.has_value() || v.value().GetLength() == 0) return sqlite3_bind_null(stmt, idx);

    return sqlite3_bind_text(stmt, idx, v.value().c_str(), -1, SQLITE_TRANSIENT);
}

bool Database::SavePrimitiveContent(const PrimitiveContent &pc, CString &err)
{
    if (!db_) {
        err = "DB not open";
        return false;
    }

    // Begin transaction
    char *zErr = nullptr;
    int   rc   = sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, &zErr);
    if (rc != SQLITE_OK) {
        if (zErr) {
            err = CString(zErr);
            sqlite3_free(zErr);
        }
        return false;
    }

    // Extract base fields from resource (we need to inspect by kind and get ResourceBase fields)
    CString ri;
    CString rn;
    CString pi;

    // Based on kind, insert into type table
    bool ok = true;
    if (auto *r = pc.GetIf<CSEBase>()) ok = SaveCSE(*r, err);
    else if (auto *r = pc.GetIf<AE>()) ok = SaveAE(*r, err);
    else if (auto *r = pc.GetIf<Container>()) ok = SaveContainer(*r, err);
    else if (auto *r = pc.GetIf<ContentInstance>()) ok = SaveContentInstance(*r, err);
    else if (auto *r = pc.GetIf<Subscription>()) ok = SaveSubscription(*r, err);
    else {
        ok  = false;
        err = "Unsupported ResourceType for save";
    }

    if (!ok) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}

bool Database::InsertResources(const ResourceBase &rbase, CString &err)
{
    const char *sql =
        "INSERT OR REPLACE INTO resources "
        "(ri,rn,ty,pi,ct,lt,et,labels,acpi,custodian,daci,announceTo,announcedAttribute,"
        "announceSyncType,path) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
    sqlite3_stmt *stmt = nullptr;
    int           rc   = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }
    bind_text_or_null(stmt, 1, rbase.resourceID);
    bind_text_or_null(stmt, 2, rbase.resourceName);
    sqlite3_bind_int(
        stmt,
        3,
        static_cast<int>(rbase.resourceType.has_value() ? (int)rbase.resourceType.value() : 0));
    bind_text_or_null(stmt, 4, rbase.parentID);
    bind_text_or_null(stmt, 5, rbase.creationTime);
    bind_text_or_null(stmt, 6, rbase.lastModifiedTime);
    if (rbase.expirationTime.has_value()) bind_text_or_null(stmt, 7, rbase.expirationTime.value());
    else sqlite3_bind_null(stmt, 7);
    CString lbl = VecToPacked(rbase.labels);
    bind_text_or_null(stmt, 8, lbl);
    CString acpi = VecToPacked(rbase.accessControlPolicyIDs);
    bind_text_or_null(stmt, 9, acpi);
    bind_text_or_null(stmt, 10, rbase.custodian);
    CString daci = VecToPacked(rbase.dynamicAuthConsultIDs);
    bind_text_or_null(stmt, 11, daci);
    bind_text_or_null(stmt, 12, rbase.announceTo);
    CString aa = VecToPacked(rbase.announcedAttribute);
    bind_text_or_null(stmt, 13, aa);
    if (rbase.announceSyncType.has_value())
        sqlite3_bind_int(stmt, 14, rbase.announceSyncType.value());
    else sqlite3_bind_null(stmt, 14);

    CString path;
    if (rbase.parentID.GetLength() == 0) {
        path += rbase.resourceName;
    } else {
        CString parentPath = GetFullPathByRI(rbase.parentID);
        if (parentPath.GetLength() == 0) {
            err = "Parent path not found";
            return false;
        }
        path = parentPath;

        if (path.GetLength() > 1) { path += "/"; }

        path += rbase.resourceName;
    }
    bind_text_or_null(stmt, 15, path);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }
    return true;
}

bool Database::SaveCSE(const CSEBase &cse, CString &err)
{
    if (!db_) {
        err = "DB not open";
        return false;
    }
    if (!InsertResources(cse, err)) return false;

    const char *sql = "INSERT OR REPLACE INTO cse (ri, cst, csi, srt, srv, ctm, poa, ncp, nl, esi) "
                      "VALUES (?,?,?,?,?,?,?,?,?,?);";
    sqlite3_stmt *stmt = nullptr;
    int           rc   = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }

    bind_text_or_null(stmt, 1, cse.resourceID);
    sqlite3_bind_int(stmt, 2, static_cast<int>(cse.cseType));
    bind_text_or_null(stmt, 3, cse.cseID);

    // supportedResourceType: pack as integers separated by 0x1F
    {
        Vector<CString> srtStrs;
        for (unsigned i = 0; i < cse.supportedResourceType.GetCount(); ++i) {
            CString s;
            s.Format("%d", static_cast<int>(cse.supportedResourceType[i]));
            srtStrs.push_back(s);
        }
        CString srt = VecToPacked(srtStrs);
        bind_text_or_null(stmt, 4, srt);
    }

    CString srv = VecToPacked(cse.supportedReleaseVersions);
    bind_text_or_null(stmt, 5, srv);
    bind_text_or_null(stmt, 6, cse.currentTime);
    CString poa = VecToPacked(cse.pointOfAccess);
    bind_text_or_null(stmt, 7, poa);
    bind_text_or_null(stmt, 8, cse.notifCongestionPolicy);
    bind_text_or_null(stmt, 9, cse.nodeLink);
    bind_text_or_null(stmt, 10, cse.e2eSecInfo);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }
    return true;
}

bool Database::SaveAE(const AE &ae, CString &err)
{
    if (!db_) {
        err = "DB not open";
        return false;
    }
    if (!InsertResources(ae, err)) return false;
    const char *sql =
        "INSERT OR REPLACE INTO ae (ri, api, aei, apn, poa, ontologyRef, nl, csz, regs, rr, mei, "
        "tri, trn, srv, esi, tren) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
    sqlite3_stmt *stmt = nullptr;
    int           rc   = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }
    bind_text_or_null(stmt, 1, ae.resourceID);
    bind_text_or_null(stmt, 2, ae.appID);
    bind_text_or_null(stmt, 3, ae.aeID);
    bind_text_or_null(stmt, 4, ae.appName);
    CString poa = VecToPacked(ae.pointOfAccess);
    bind_text_or_null(stmt, 5, poa);
    bind_text_or_null(stmt, 6, ae.ontologyRef);
    bind_text_or_null(stmt, 7, ae.nodeLink);
    CString csz = VecToPacked(ae.contentSerialization);
    bind_text_or_null(stmt, 8, csz);
    if (ae.registrationStatus.has_value()) sqlite3_bind_int(stmt, 9, ae.registrationStatus.value());
    else sqlite3_bind_null(stmt, 9);
    if (ae.requestReachability.has_value())
        sqlite3_bind_int(stmt, 10, ae.requestReachability.value() ? 1 : 0);
    else sqlite3_bind_null(stmt, 10);
    bind_text_or_null(stmt, 11, ae.m2mExtID);
    bind_text_or_null(stmt, 12, ae.triggerRecipientID);
    if (ae.triggerReferenceNumber.has_value())
        sqlite3_bind_int(stmt, 13, ae.triggerReferenceNumber.value());
    else sqlite3_bind_null(stmt, 13);
    CString srv = VecToPacked(ae.supportedReleaseVersions);
    bind_text_or_null(stmt, 14, srv);
    bind_text_or_null(stmt, 15, ae.e2eSecInfo);
    if (ae.triggerEnable.has_value()) sqlite3_bind_int(stmt, 16, ae.triggerEnable.value() ? 1 : 0);
    else sqlite3_bind_null(stmt, 16);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }
    return true;
}

bool Database::SaveContainer(const Container &cnt, CString &err)
{
    if (!db_) {
        err = "DB not open";
        return false;
    }
    if (!InsertResources(cnt, err)) return false;
    const char   *sql  = "INSERT OR REPLACE INTO container (ri, st, cr, mni, mbs, mbis, mia, cni, "
                         "cbs, ontologyRef, disr, li, loc) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?);";
    sqlite3_stmt *stmt = nullptr;
    int           rc   = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }
    bind_text_or_null(stmt, 1, cnt.resourceID);
    if (cnt.stateTag.has_value()) sqlite3_bind_int(stmt, 2, cnt.stateTag.value());
    else sqlite3_bind_null(stmt, 2);
    bind_text_or_null(stmt, 3, cnt.creator);
    if (cnt.maxNrOfInstances.has_value()) sqlite3_bind_int64(stmt, 4, cnt.maxNrOfInstances.value());
    else sqlite3_bind_null(stmt, 4);
    if (cnt.maxByteSize.has_value()) sqlite3_bind_int64(stmt, 5, cnt.maxByteSize.value());
    else sqlite3_bind_null(stmt, 5);
    if (cnt.maxByteSizePerInstance.has_value())
        sqlite3_bind_int64(stmt, 6, cnt.maxByteSizePerInstance.value());
    else sqlite3_bind_null(stmt, 6);
    if (cnt.maxInstanceAge.has_value()) sqlite3_bind_int64(stmt, 7, cnt.maxInstanceAge.value());
    else sqlite3_bind_null(stmt, 7);
    sqlite3_bind_int64(stmt, 8, cnt.currentNrOfInstances);
    sqlite3_bind_int64(stmt, 9, cnt.currentByteSize);
    bind_text_or_null(stmt, 10, cnt.ontologyRef);
    if (cnt.disableRetrieval.has_value())
        sqlite3_bind_int(stmt, 11, cnt.disableRetrieval.value() ? 1 : 0);
    else sqlite3_bind_null(stmt, 11);
    bind_text_or_null(stmt, 12, cnt.locationID);
    bind_text_or_null(stmt, 13, cnt.location);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }
    return true;
}

bool Database::SaveContentInstance(const ContentInstance &cin, CString &err)
{
    if (!db_) {
        err = "DB not open";
        return false;
    }
    if (!InsertResources(cin, err)) return false;
    const char   *sql  = "INSERT OR REPLACE INTO content_instance (ri, st, cr, cnf, cs, conr, con, "
                         "ontologyRef, dgt, dcnt) VALUES (?,?,?,?,?,?,?,?,?,?);";
    sqlite3_stmt *stmt = nullptr;
    int           rc   = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }
    bind_text_or_null(stmt, 1, cin.resourceID);
    if (cin.stateTag.has_value()) sqlite3_bind_int(stmt, 2, cin.stateTag.value());
    else sqlite3_bind_null(stmt, 2);
    bind_text_or_null(stmt, 3, cin.creator);
    bind_text_or_null(stmt, 4, cin.contentInfo);
    sqlite3_bind_int64(stmt, 5, cin.contentSize);
    bind_text_or_null(stmt, 6, cin.contentRef);
    if (cin.content.GetLength() > 0)
        sqlite3_bind_text(stmt, 7, cin.content.c_str(), -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 7);
    bind_text_or_null(stmt, 8, cin.ontologyRef);
    bind_text_or_null(stmt, 9, cin.dataGenerationTime);
    if (cin.deletionCnt.has_value()) sqlite3_bind_int(stmt, 10, cin.deletionCnt.value());
    else sqlite3_bind_null(stmt, 10);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }
    return true;
}

bool Database::SaveSubscription(const Subscription &sub, CString &err)
{
    if (!db_) {
        err = "DB not open";
        return false;
    }

    if (!InsertResources(sub, err)) return false;

    const char *sql = "INSERT OR REPLACE INTO subscription ("
                      "ri, "
                      "enc_cb, enc_ca, enc_ms, enc_us, enc_sts, enc_stb, "
                      "enc_eb, enc_ea, enc_labels, enc_sa, enc_sb, "
                      "enc_net, enc_chty, enc_atr, enc_fu, enc_cfq, enc_cfs, enc_md, "
                      "rtl_max, rtl_tw, "
                      "btn_num, btn_dur, "
                      "exc, nu, gpi, nfu, psn, pn, nsp, ln, nct, nec, su, cr, crp, gn, acrs"
                      ") VALUES ("
                      "?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?"
                      ");";

    sqlite3_stmt *stmt = nullptr;
    int           rc   = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }

    int i = 1;

    // -------------------------
    // ri
    // -------------------------
    bind_text_or_null(stmt, i++, sub.resourceID);

    // -------------------------
    // enc (EventNotificationCriteria)
    // -------------------------
    const auto &enc = sub.eventNotificationCriteria;

    bind_text_or_null(stmt, i++, enc.createdBefore);
    bind_text_or_null(stmt, i++, enc.createdAfter);
    bind_text_or_null(stmt, i++, enc.modifiedSince);
    bind_text_or_null(stmt, i++, enc.unmodifiedSince);

    if (enc.stateTagSmaller.has_value()) sqlite3_bind_int(stmt, i++, *enc.stateTagSmaller);
    else sqlite3_bind_null(stmt, i++);

    if (enc.stateTagBigger.has_value()) sqlite3_bind_int(stmt, i++, *enc.stateTagBigger);
    else sqlite3_bind_null(stmt, i++);

    bind_text_or_null(stmt, i++, enc.expireBefore);
    bind_text_or_null(stmt, i++, enc.expireAfter);

    bind_text_or_null(stmt, i++, VecToPacked(enc.labels));

    if (enc.sizeAbove.has_value()) sqlite3_bind_int64(stmt, i++, *enc.sizeAbove);
    else sqlite3_bind_null(stmt, i++);

    if (enc.sizeBelow.has_value()) sqlite3_bind_int64(stmt, i++, *enc.sizeBelow);
    else sqlite3_bind_null(stmt, i++);

    bind_text_or_null(stmt, i++, EnumVecToPacked(enc.notificationEventType));
    bind_text_or_null(stmt, i++, EnumVecToPacked(enc.childResourceType));
    bind_text_or_null(stmt, i++, VecToPacked(enc.attributeList));

    if (enc.filterUsage.has_value())
        sqlite3_bind_int(stmt, i++, static_cast<int>(*enc.filterUsage));
    else sqlite3_bind_null(stmt, i++);

    bind_text_or_null(stmt, i++, enc.contentFilterQuery);
    bind_text_or_null(stmt, i++, enc.contentFilterSyntax);

    if (enc.missingData.has_value()) sqlite3_bind_int(stmt, i++, *enc.missingData ? 1 : 0);
    else sqlite3_bind_null(stmt, i++);

    // -------------------------
    // rtl (RateLimit)
    // -------------------------
    if (sub.rateLimit.has_value()) {
        if (sub.rateLimit->maxNrOfNotify.has_value())
            sqlite3_bind_int(stmt, i++, *sub.rateLimit->maxNrOfNotify);
        else sqlite3_bind_null(stmt, i++);

        if (sub.rateLimit->timeWindow.has_value())
            sqlite3_bind_int(stmt, i++, *sub.rateLimit->timeWindow);
        else sqlite3_bind_null(stmt, i++);
    } else {
        sqlite3_bind_null(stmt, i++);
        sqlite3_bind_null(stmt, i++);
    }

    // -------------------------
    // btn (BatchNotify)
    // -------------------------
    if (sub.batchNotify.has_value()) {
        sqlite3_bind_int(stmt, i++, sub.batchNotify->number);
        bind_text_or_null(stmt, i++, sub.batchNotify->duration);
    } else {
        sqlite3_bind_null(stmt, i++);
        sqlite3_bind_null(stmt, i++);
    }

    // -------------------------
    // remaining Subscription fields
    // -------------------------
    if (sub.expirationCounter.has_value()) sqlite3_bind_int(stmt, i++, *sub.expirationCounter);
    else sqlite3_bind_null(stmt, i++);

    CString nu = VecToPacked(sub.notificationURI);
    bind_text_or_null(stmt, i++, nu);

    bind_text_or_null(stmt, i++, sub.groupID);
    bind_text_or_null(stmt, i++, sub.notificationForwardingURI);

    if (sub.preSubscriptionNotify.has_value())
        sqlite3_bind_int(stmt, i++, *sub.preSubscriptionNotify);
    else sqlite3_bind_null(stmt, i++);

    if (sub.pendingNotification.has_value()) sqlite3_bind_int(stmt, i++, *sub.pendingNotification);
    else sqlite3_bind_null(stmt, i++);

    if (sub.notificationStoragePriority.has_value())
        sqlite3_bind_int(stmt, i++, *sub.notificationStoragePriority);
    else sqlite3_bind_null(stmt, i++);

    if (sub.latestNotify.has_value()) sqlite3_bind_int(stmt, i++, *sub.latestNotify ? 1 : 0);
    else sqlite3_bind_null(stmt, i++);

    if (sub.notificationContentType.has_value())
        sqlite3_bind_int(stmt, i++, static_cast<int>(*sub.notificationContentType));
    else sqlite3_bind_null(stmt, i++);

    if (sub.notificationEventCat.has_value())
        sqlite3_bind_int(stmt, i++, *sub.notificationEventCat);
    else sqlite3_bind_null(stmt, i++);

    bind_text_or_null(stmt, i++, sub.subscriberURI);
    bind_text_or_null(stmt, i++, sub.creator);
    if (sub.creatorProvided.has_value())
        sqlite3_bind_int(stmt, i++, sub.creatorProvided.value() ? 1 : 0);
    else sqlite3_bind_null(stmt, i++);
    bind_text_or_null(stmt, i++, sub.groupName);
    bind_text_or_null(stmt, i++, sub.associatedCrossResourceSub);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }

    return true;
}

inline CString column_text_or_null(sqlite3_stmt *stmt, int &i)
{
    if (sqlite3_column_type(stmt, i) == SQLITE_NULL) {
        ++i;
        return CString();
    }
    return column_text_or_empty(stmt, i++);
}

// Minimal loader: find resource by ri, rn or pi/rn and populate PrimitiveContent for supported
// types
bool Database::LoadPrimitiveContentByTarget(const CString    &target,
                                            PrimitiveContent &out,
                                            CString          &err)
{
    if (!db_) {
        err = "DB not open";
        return false;
    }

    const char *sql =
        "SELECT ri, rn, ty, pi, ct, lt, et, labels, acpi, custodian, daci, announceTo, "
        "announcedAttribute, announceSyncType "
        "FROM resources "
        "WHERE ri = ? OR rn = ? OR path = ? "
        "LIMIT 1;";
    sqlite3_stmt *stmt = nullptr;
    int           rc   = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }
    sqlite3_bind_text(stmt, 1, target.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, target.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, target.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        err = "not found";
        return false;
    }
    CString ri                 = column_text_or_empty(stmt, 0);
    CString rn                 = column_text_or_empty(stmt, 1);
    int     ty                 = sqlite3_column_int(stmt, 2);
    CString pi                 = column_text_or_empty(stmt, 3);
    CString ct                 = column_text_or_empty(stmt, 4);
    CString lt                 = column_text_or_empty(stmt, 5);
    CString et                 = column_text_or_empty(stmt, 6);
    CString labels             = column_text_or_empty(stmt, 7);
    CString acpi               = column_text_or_empty(stmt, 8);
    CString custodian          = column_text_or_empty(stmt, 9);
    CString daci               = column_text_or_empty(stmt, 10);
    CString announceTo         = column_text_or_empty(stmt, 11);
    CString announcedAttribute = column_text_or_empty(stmt, 12);
    bool    hasAst             = (sqlite3_column_type(stmt, 13) != SQLITE_NULL);
    int     ast                = hasAst ? sqlite3_column_int(stmt, 13) : 0;

    sqlite3_finalize(stmt);

    // based on type, query the type table
    switch (static_cast<ResourceType>(ty)) {
        case ResourceType::AE: {

            const char   *q   = "SELECT api, aei, apn, poa, ontologyRef, nl, csz, regs, rr, mei, "
                                "tri, trn, srv, esi, tren "
                                "FROM ae "
                                "WHERE ri = ? LIMIT 1;";
            sqlite3_stmt *s2  = nullptr;
            int           rc2 = sqlite3_prepare_v2(db_, q, -1, &s2, nullptr);
            if (rc2 != SQLITE_OK) {
                err = CString(sqlite3_errmsg(db_));
                return false;
            }
            sqlite3_bind_text(s2, 1, ri.c_str(), -1, SQLITE_TRANSIENT);
            rc2 = sqlite3_step(s2);
            if (rc2 == SQLITE_ROW) {
                AE a;
                apply_common_resource_base(a,
                                           ri,
                                           rn,
                                           pi,
                                           ct,
                                           lt,
                                           et,
                                           labels,
                                           acpi,
                                           custodian,
                                           daci,
                                           announceTo,
                                           announcedAttribute,
                                           hasAst,
                                           ast);
                a.resourceType = ResourceType::AE;
                a.appID        = column_text_or_empty(s2, 0);
                a.aeID         = column_text_or_empty(s2, 1);
                if (sqlite3_column_type(s2, 2) != SQLITE_NULL)
                    a.appName = column_text_or_empty(s2, 2);
                CString poa = column_text_or_empty(s2, 3);
                PackedToVec(poa, a.pointOfAccess);
                if (sqlite3_column_type(s2, 4) != SQLITE_NULL)
                    a.ontologyRef = column_text_or_empty(s2, 4);
                if (sqlite3_column_type(s2, 5) != SQLITE_NULL)
                    a.nodeLink = column_text_or_empty(s2, 5);
                CString csz = column_text_or_empty(s2, 6);
                PackedToVec(csz, a.contentSerialization);
                if (sqlite3_column_type(s2, 7) != SQLITE_NULL)
                    a.registrationStatus = (u8)sqlite3_column_int(s2, 7);

                a.requestReachability = (sqlite3_column_type(s2, 8) != SQLITE_NULL)
                                            ? (sqlite3_column_int(s2, 8) != 0)
                                            : false;
                if (sqlite3_column_type(s2, 9) != SQLITE_NULL)
                    a.m2mExtID = column_text_or_empty(s2, 9);
                if (sqlite3_column_type(s2, 10) != SQLITE_NULL)
                    a.triggerRecipientID = column_text_or_empty(s2, 10);
                if (sqlite3_column_type(s2, 11) != SQLITE_NULL)
                    a.triggerReferenceNumber = (u32)sqlite3_column_int(s2, 11);
                CString srv = column_text_or_empty(s2, 12);
                PackedToVec(srv, a.supportedReleaseVersions);
                a.e2eSecInfo = column_text_or_empty(s2, 13);
                if (sqlite3_column_type(s2, 14) != SQLITE_NULL)
                    a.triggerEnable = (sqlite3_column_int(s2, 14) != 0);
                out = a;
                sqlite3_finalize(s2);
                return true;
            }
            sqlite3_finalize(s2);
            err = "ae row not found";
            return false;
        }
        case ResourceType::Container: {
            const char *q = "SELECT st, cr, mni, mbs, mbis, mia, cni, cbs, ontologyRef, disr, li, "
                            "loc FROM container WHERE ri = ? LIMIT 1;";
            sqlite3_stmt *s2  = nullptr;
            int           rc2 = sqlite3_prepare_v2(db_, q, -1, &s2, nullptr);
            if (rc2 != SQLITE_OK) {
                err = CString(sqlite3_errmsg(db_));
                return false;
            }
            sqlite3_bind_text(s2, 1, ri.c_str(), -1, SQLITE_TRANSIENT);
            rc2 = sqlite3_step(s2);
            if (rc2 == SQLITE_ROW) {
                Container c;
                apply_common_resource_base(c,
                                           ri,
                                           rn,
                                           pi,
                                           ct,
                                           lt,
                                           et,
                                           labels,
                                           acpi,
                                           custodian,
                                           daci,
                                           announceTo,
                                           announcedAttribute,
                                           hasAst,
                                           ast);
                c.resourceType = ResourceType::Container;
                if (sqlite3_column_type(s2, 0) != SQLITE_NULL)
                    c.stateTag = (u32)sqlite3_column_int(s2, 0);
                CString creator = column_text_or_empty(s2, 1);
                if (creator.GetLength() != 0) c.creator = creator;
                if (sqlite3_column_type(s2, 2) != SQLITE_NULL)
                    c.maxNrOfInstances = (s64)sqlite3_column_int64(s2, 2);
                if (sqlite3_column_type(s2, 3) != SQLITE_NULL)
                    c.maxByteSize = (s64)sqlite3_column_int64(s2, 3);
                if (sqlite3_column_type(s2, 4) != SQLITE_NULL)
                    c.maxByteSizePerInstance = (s64)sqlite3_column_int64(s2, 4);
                if (sqlite3_column_type(s2, 5) != SQLITE_NULL)
                    c.maxInstanceAge = (s64)sqlite3_column_int64(s2, 5);
                c.currentNrOfInstances = (s64)sqlite3_column_int64(s2, 6);
                c.currentByteSize      = (s64)sqlite3_column_int64(s2, 7);
                c.ontologyRef          = column_text_or_empty(s2, 8);
                if (sqlite3_column_type(s2, 9) != SQLITE_NULL)
                    c.disableRetrieval = (sqlite3_column_int(s2, 9) != 0);
                c.locationID = column_text_or_empty(s2, 10);
                c.location   = column_text_or_empty(s2, 11);
                out          = c;
                sqlite3_finalize(s2);
                return true;
            }
            sqlite3_finalize(s2);
            err = "container row not found";
            return false;
        }
        case ResourceType::ContentInstance: {
            const char   *q   = "SELECT st, cr, cnf, cs, conr, con, ontologyRef, dgt, dcnt FROM "
                                "content_instance WHERE ri = ? LIMIT 1;";
            sqlite3_stmt *s2  = nullptr;
            int           rc2 = sqlite3_prepare_v2(db_, q, -1, &s2, nullptr);
            if (rc2 != SQLITE_OK) {
                err = CString(sqlite3_errmsg(db_));
                return false;
            }
            sqlite3_bind_text(s2, 1, ri.c_str(), -1, SQLITE_TRANSIENT);
            rc2 = sqlite3_step(s2);
            if (rc2 == SQLITE_ROW) {
                ContentInstance ci;
                apply_common_resource_base(ci,
                                           ri,
                                           rn,
                                           pi,
                                           ct,
                                           lt,
                                           et,
                                           labels,
                                           acpi,
                                           custodian,
                                           daci,
                                           announceTo,
                                           announcedAttribute,
                                           hasAst,
                                           ast);
                ci.resourceType = ResourceType::ContentInstance;
                if (sqlite3_column_type(s2, 0) != SQLITE_NULL)
                    ci.stateTag = (u32)sqlite3_column_int(s2, 0);

                CString creator = column_text_or_empty(s2, 1);
                if (creator.GetLength() != 0) ci.creator = creator;
                ci.contentInfo = column_text_or_empty(s2, 2);
                ci.contentSize = (s64)sqlite3_column_int64(s2, 3);
                ci.contentRef  = column_text_or_empty(s2, 4);
                if (sqlite3_column_type(s2, 5) != SQLITE_NULL) {
                    ci.content = column_text_or_empty(s2, 5);
                }
                ci.ontologyRef = column_text_or_empty(s2, 6);
                CString dgt    = column_text_or_empty(s2, 7);
                if (dgt.GetLength() != 0) ci.dataGenerationTime = dgt;

                if (sqlite3_column_type(s2, 8) != SQLITE_NULL)
                    ci.deletionCnt = (u32)sqlite3_column_int(s2, 8);
                out = ci;
                sqlite3_finalize(s2);
                return true;
            }
            sqlite3_finalize(s2);
            err = "content_instance row not found";
            return false;
        }
        case ResourceType::Subscription: {
            const char *q = "SELECT "
                            "enc_cb, enc_ca, enc_ms, enc_us, enc_sts, enc_stb, "
                            "enc_eb, enc_ea, enc_labels, enc_sa, enc_sb, "
                            "enc_net, enc_chty, enc_atr, enc_fu, enc_cfq, enc_cfs, enc_md, "
                            "rtl_max, rtl_tw, "
                            "btn_num, btn_dur, "
                            "exc, nu, gpi, nfu, psn, pn, nsp, ln, nct, nec, su, cr, crp, gn, acrs "
                            "FROM subscription WHERE ri = ? LIMIT 1;";

            sqlite3_stmt *s2  = nullptr;
            int           rc2 = sqlite3_prepare_v2(db_, q, -1, &s2, nullptr);

            if (rc2 != SQLITE_OK) {
                err = CString(sqlite3_errmsg(db_));
                return false;
            }

            sqlite3_bind_text(s2, 1, ri.c_str(), -1, SQLITE_TRANSIENT);

            rc2 = sqlite3_step(s2);
            if (rc2 != SQLITE_ROW) {
                sqlite3_finalize(s2);
                err = "subscription row not found";
                return false;
            }

            Subscription s;

            apply_common_resource_base(s,
                                       ri,
                                       rn,
                                       pi,
                                       ct,
                                       lt,
                                       et,
                                       labels,
                                       acpi,
                                       custodian,
                                       daci,
                                       announceTo,
                                       announcedAttribute,
                                       hasAst,
                                       ast);

            s.resourceType = ResourceType::Subscription;

            int i = 0;

            // -------------------------
            // enc
            // -------------------------
            auto &enc = s.eventNotificationCriteria;

            enc.createdBefore   = column_text_or_null(s2, i);
            enc.createdAfter    = column_text_or_null(s2, i);
            enc.modifiedSince   = column_text_or_null(s2, i);
            enc.unmodifiedSince = column_text_or_null(s2, i);

            enc.stateTagSmaller = sqlite3_column_type(s2, i) != SQLITE_NULL
                                      ? Optional<s32>(sqlite3_column_int(s2, i++))
                                      : (++i, Optional<s32>{});

            enc.stateTagBigger = sqlite3_column_type(s2, i) != SQLITE_NULL
                                     ? Optional<s32>(sqlite3_column_int(s2, i++))
                                     : (++i, Optional<s32>{});

            enc.expireBefore = column_text_or_null(s2, i);
            enc.expireAfter  = column_text_or_null(s2, i);

            PackedToVec(column_text_or_empty(s2, i++), enc.labels);

            enc.sizeAbove = sqlite3_column_type(s2, i) != SQLITE_NULL
                                ? Optional<s64>(sqlite3_column_int64(s2, i++))
                                : (++i, Optional<s64>{});

            enc.sizeBelow = sqlite3_column_type(s2, i) != SQLITE_NULL
                                ? Optional<s64>(sqlite3_column_int64(s2, i++))
                                : (++i, Optional<s64>{});

            PackedToEnumVec(column_text_or_empty(s2, i++), enc.notificationEventType);
            PackedToEnumVec(column_text_or_empty(s2, i++), enc.childResourceType);
            PackedToVec(column_text_or_empty(s2, i++), enc.attributeList);

            if (sqlite3_column_type(s2, i) != SQLITE_NULL)
                enc.filterUsage = (FilterUsage)sqlite3_column_int(s2, i++);
            else i++;

            enc.contentFilterQuery  = column_text_or_null(s2, i);
            enc.contentFilterSyntax = column_text_or_null(s2, i);

            enc.missingData = sqlite3_column_type(s2, i) != SQLITE_NULL
                                  ? Optional<boolean>(sqlite3_column_int(s2, i++) != 0)
                                  : (++i, Optional<boolean>{});

            // -------------------------
            // VALIDATION (enc sanity)
            // -------------------------
            if (enc.stateTagSmaller.has_value() && enc.stateTagBigger.has_value() &&
                *enc.stateTagSmaller > *enc.stateTagBigger) {
                sqlite3_finalize(s2);
                err = "invalid state tag range in subscription";
                return false;
            }

            // -------------------------
            // rtl
            // -------------------------
            if (sqlite3_column_type(s2, i) != SQLITE_NULL ||
                sqlite3_column_type(s2, i + 1) != SQLITE_NULL) {
                RateLimit rl;

                if (sqlite3_column_type(s2, i) != SQLITE_NULL)
                    rl.maxNrOfNotify = sqlite3_column_int(s2, i++);
                else i++;

                if (sqlite3_column_type(s2, i) != SQLITE_NULL)
                    rl.timeWindow = sqlite3_column_int(s2, i++);
                else i++;

                if (rl.maxNrOfNotify.has_value() && *rl.maxNrOfNotify < 0) {
                    sqlite3_finalize(s2);
                    err = "invalid rate limit maxNrOfNotify";
                    return false;
                }

                if (rl.timeWindow.has_value() && *rl.timeWindow <= 0) {
                    sqlite3_finalize(s2);
                    err = "invalid rate limit timeWindow";
                    return false;
                }

                s.rateLimit = rl;
            } else {
                i += 2;
            }

            // -------------------------
            // btn
            // -------------------------
            if (sqlite3_column_type(s2, i) != SQLITE_NULL) {
                BatchNotify bn;

                bn.number = sqlite3_column_int(s2, i++);
                if (bn.number < 0) {
                    sqlite3_finalize(s2);
                    err = "invalid batch notify number";
                    return false;
                }

                bn.duration   = column_text_or_null(s2, i);
                s.batchNotify = bn;
            } else {
                i += 2;
            }

            // -------------------------
            // base fields
            // -------------------------
            s.expirationCounter = sqlite3_column_type(s2, i) != SQLITE_NULL
                                      ? Optional<s32>(sqlite3_column_int(s2, i++))
                                      : (++i, Optional<s32>{});

            PackedToVec(column_text_or_empty(s2, i++), s.notificationURI);

            s.groupID                   = column_text_or_null(s2, i);
            s.notificationForwardingURI = column_text_or_null(s2, i);

            s.preSubscriptionNotify =
                sqlite3_column_type(s2, i) != SQLITE_NULL ? sqlite3_column_int(s2, i++) : (++i, 0);

            s.pendingNotification = sqlite3_column_type(s2, i) != SQLITE_NULL
                                        ? (u8)sqlite3_column_int(s2, i++)
                                        : (++i, 0);

            s.notificationStoragePriority =
                sqlite3_column_type(s2, i) != SQLITE_NULL ? sqlite3_column_int(s2, i++) : (++i, 0);

            s.latestNotify = sqlite3_column_type(s2, i) != SQLITE_NULL
                                 ? sqlite3_column_int(s2, i++) != 0
                                 : (++i, false);

            s.notificationContentType = sqlite3_column_type(s2, i) != SQLITE_NULL
                                            ? (NotificationContentType)sqlite3_column_int(s2, i++)
                                            : (++i, NotificationContentType{});

            s.notificationEventCat = sqlite3_column_type(s2, i) != SQLITE_NULL
                                         ? (u8)sqlite3_column_int(s2, i++)
                                         : (++i, 0);

            s.subscriberURI = column_text_or_null(s2, i);
            s.creator       = column_text_or_null(s2, i);

            s.creatorProvided = (sqlite3_column_type(s2, i) != SQLITE_NULL)
                                    ? (sqlite3_column_int(s2, i) != 0)
                                    : false;
            i++;
            s.groupName                  = column_text_or_null(s2, i);
            s.associatedCrossResourceSub = column_text_or_null(s2, i);

            sqlite3_finalize(s2);

            out = s;
            return true;
        }
        case ResourceType::CSEBase: {
            const char *q =
                "SELECT cst, csi, srt, srv, ctm, poa, ncp, nl, esi FROM cse WHERE ri = ? LIMIT 1;";
            sqlite3_stmt *s2  = nullptr;
            int           rc2 = sqlite3_prepare_v2(db_, q, -1, &s2, nullptr);
            if (rc2 != SQLITE_OK) {
                err = CString(sqlite3_errmsg(db_));
                return false;
            }
            sqlite3_bind_text(s2, 1, ri.c_str(), -1, SQLITE_TRANSIENT);
            rc2 = sqlite3_step(s2);
            if (rc2 == SQLITE_ROW) {
                CSEBase c;
                apply_common_resource_base(c,
                                           ri,
                                           rn,
                                           pi,
                                           ct,
                                           lt,
                                           et,
                                           labels,
                                           acpi,
                                           custodian,
                                           daci,
                                           announceTo,
                                           announcedAttribute,
                                           hasAst,
                                           ast);
                c.resourceType = ResourceType::CSEBase;
                c.cseType      = static_cast<CSEType>(sqlite3_column_int(s2, 0));
                c.cseID        = column_text_or_empty(s2, 1);

                // supportedResourceType: unpack integers back to ResourceType
                {
                    Vector<CString> srtStrs;
                    PackedToVec(column_text_or_empty(s2, 2), srtStrs);
                    for (unsigned i = 0; i < srtStrs.GetCount(); ++i)
                        c.supportedResourceType.push_back(
                            static_cast<ResourceType>(atoi(srtStrs[i].c_str())));
                }

                PackedToVec(column_text_or_empty(s2, 3), c.supportedReleaseVersions);
                c.currentTime = column_text_or_empty(s2, 4);
                PackedToVec(column_text_or_empty(s2, 5), c.pointOfAccess);
                if (sqlite3_column_type(s2, 6) != SQLITE_NULL)
                    c.notifCongestionPolicy = column_text_or_empty(s2, 6);
                if (sqlite3_column_type(s2, 7) != SQLITE_NULL)
                    c.nodeLink = column_text_or_empty(s2, 7);
                if (sqlite3_column_type(s2, 8) != SQLITE_NULL)
                    c.e2eSecInfo = column_text_or_empty(s2, 8);

                out = c;
                sqlite3_finalize(s2);
                return true;
            }
            sqlite3_finalize(s2);
            err = "csebase row not found";
            return false;
        }
        default:
            err = "unsupported resource type in DB loader";
            return false;
    }
    return false;
}

// Returns the resource IDs of all direct children of the resource identified
// by `target` (resolved by ri, rn, or pi/rn — same rules as
// LoadPrimitiveContentByTarget).
//
// If `childrenKind` is ResourceType::None the filter is omitted and
// all child types are returned.  Otherwise only children whose `ty` column
// matches the given kind are included.
//
// On success returns true and populates `out` with the `ri` values; the
// caller can pass each entry to LoadPrimitiveContentByTarget to get the full
// resource.  Returns false (with `err` set) only on a hard DB error; an
// empty result set is not an error.
bool Database::LoadPrimitiveContentChildren(const CString   &target,
                                            ResourceType     childrenType,
                                            Vector<CString> &out,
                                            CString         &err)
{
    if (!db_) {
        err = "DB not open";
        return false;
    }

    out.clear();

    // Step 1: resolve the parent ri from the target string.
    // We reuse the same three-way match used by LoadPrimitiveContentByTarget.
    const char *resolveSql = "SELECT ri FROM resources "
                             "WHERE ri = ? OR rn = ? OR path = ? "
                             "LIMIT 1;";

    sqlite3_stmt *resolveStmt = nullptr;
    int           rc          = sqlite3_prepare_v2(db_, resolveSql, -1, &resolveStmt, nullptr);
    if (rc != SQLITE_OK) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(resolveStmt, 1, target.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(resolveStmt, 2, target.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(resolveStmt, 3, target.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(resolveStmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(resolveStmt);
        err = "parent not found";
        return false;
    }

    CString parentRi = CString(reinterpret_cast<const char *>(sqlite3_column_text(resolveStmt, 0)));
    sqlite3_finalize(resolveStmt);

    // Step 2: fetch children.
    // With kind filter:    SELECT ri FROM resources WHERE pi = ? AND ty = ?
    // Without kind filter: SELECT ri FROM resources WHERE pi = ?
    sqlite3_stmt *childStmt = nullptr;

    if (childrenType == ResourceType::None) {
        const char *sql = "SELECT ri FROM resources WHERE pi = ?;";
        rc              = sqlite3_prepare_v2(db_, sql, -1, &childStmt, nullptr);
        if (rc != SQLITE_OK) {
            err = CString(sqlite3_errmsg(db_));
            return false;
        }
        sqlite3_bind_text(childStmt, 1, parentRi.c_str(), -1, SQLITE_TRANSIENT);
    } else {
        const char *sql = "SELECT ri FROM resources WHERE pi = ? AND ty = ?;";
        rc              = sqlite3_prepare_v2(db_, sql, -1, &childStmt, nullptr);
        if (rc != SQLITE_OK) {
            err = CString(sqlite3_errmsg(db_));
            return false;
        }
        sqlite3_bind_text(childStmt, 1, parentRi.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(childStmt, 2, static_cast<u32>(childrenType));
    }

    while ((rc = sqlite3_step(childStmt)) == SQLITE_ROW) {
        const char *riText = reinterpret_cast<const char *>(sqlite3_column_text(childStmt, 0));
        if (riText) out.push_back(CString(riText));
    }

    sqlite3_finalize(childStmt);

    if (rc != SQLITE_DONE) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }

    return true;
}

// Returns the ri of the most recently inserted child of `target` with the
// given `childrenKind`.  Useful for fetching the latest ContentInstance of a
// Container, for example.
//
// Returns true and sets `outRi` on success, false (with `err`) if the parent
// is not found or no matching child exists.
bool Database::LoadLatestChild(const CString &target,
                               ResourceType   childrenType,
                               CString       &outRi,
                               CString       &err)
{
    if (!db_) {
        err = "DB not open";
        return false;
    }

    // Resolve parent ri (same three-way match as everywhere else)
    const char *resolveSql = "SELECT ri FROM resources "
                             "WHERE ri = ? OR rn = ? OR path = ? "
                             "LIMIT 1;";

    sqlite3_stmt *resolveStmt = nullptr;
    int           rc          = sqlite3_prepare_v2(db_, resolveSql, -1, &resolveStmt, nullptr);
    if (rc != SQLITE_OK) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(resolveStmt, 1, target.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(resolveStmt, 2, target.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(resolveStmt, 3, target.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(resolveStmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(resolveStmt);
        err = "parent not found";
        return false;
    }

    CString parentRi = CString(reinterpret_cast<const char *>(sqlite3_column_text(resolveStmt, 0)));
    sqlite3_finalize(resolveStmt);

    // Fetch the most recently inserted child of the requested type
    const char *sql =
        "SELECT ri FROM resources WHERE pi = ? AND ty = ? ORDER BY rowid DESC LIMIT 1;";

    sqlite3_stmt *stmt = nullptr;
    rc                 = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, parentRi.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, static_cast<int>(childrenType));

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        err = "no child of that type found";
        return false;
    }

    outRi = CString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
    sqlite3_finalize(stmt);
    return true;
}

bool Database::LoadOldestChild(const CString &target,
                               ResourceType   childrenType,
                               CString       &outRi,
                               CString       &err)
{
    if (!db_) {
        err = "DB not open";
        return false;
    }

    // Resolve parent ri (same three-way match as everywhere else)
    const char *resolveSql = "SELECT ri FROM resources "
                             "WHERE ri = ? OR rn = ? OR path = ? "
                             "LIMIT 1;";

    sqlite3_stmt *resolveStmt = nullptr;
    int           rc          = sqlite3_prepare_v2(db_, resolveSql, -1, &resolveStmt, nullptr);
    if (rc != SQLITE_OK) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(resolveStmt, 1, target.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(resolveStmt, 2, target.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(resolveStmt, 3, target.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(resolveStmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(resolveStmt);
        err = "parent not found";
        return false;
    }

    CString parentRi = CString(reinterpret_cast<const char *>(sqlite3_column_text(resolveStmt, 0)));
    sqlite3_finalize(resolveStmt);

    // Fetch the most recently inserted child of the requested type
    const char *sql =
        "SELECT ri FROM resources WHERE pi = ? AND ty = ? ORDER BY rowid ASC LIMIT 1;";

    sqlite3_stmt *stmt = nullptr;
    rc                 = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, parentRi.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, static_cast<int>(childrenType));

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        err = "no child of that type found";
        return false;
    }

    outRi = CString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
    sqlite3_finalize(stmt);
    return true;
}

bool Database::GetCSEBase(CSEBase &out, CString &err)
{
    PrimitiveContent pc;
    if (!LoadPrimitiveContentByTarget(CString("m2m"), pc, err)) return false;
    if (auto c = pc.GetIf<CSEBase>()) {
        out = *c;
        return true;
    }
    err = "CSEBase not found";
    return false;
}

bool Database::GetPathByRI(const CString &ri, CString &out, CString &err)
{
    const char   *q  = "SELECT path FROM resources WHERE ri = ? LIMIT 1;";
    sqlite3_stmt *s  = nullptr;
    int           rc = sqlite3_prepare_v2(db_, q, -1, &s, nullptr);
    if (rc != SQLITE_OK) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }
    sqlite3_bind_text(s, 1, ri.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(s);
    if (rc == SQLITE_ROW) {
        out = column_text_or_empty(s, 0);
        sqlite3_finalize(s);
        return true;
    }
    sqlite3_finalize(s);
    err = "not found";
    return false;
}

bool Database::GetContainerCurrentSize(const CString &ri, s64 &out, CString &err)
{
    const char   *q  = "SELECT SUM(c.cs) "
                       "FROM resources r "
                       "JOIN content_instance c ON r.ri = c.ri "
                       "WHERE r.pi = ? AND r.ty = ?;";
    sqlite3_stmt *s  = nullptr;
    int           rc = sqlite3_prepare_v2(db_, q, -1, &s, nullptr);
    if (rc != SQLITE_OK) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }
    sqlite3_bind_text(s, 1, ri.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s, 2, static_cast<int>(ResourceType::ContentInstance));
    rc = sqlite3_step(s);
    if (rc == SQLITE_ROW) {
        out = sqlite3_column_int64(s, 0);
        sqlite3_finalize(s);
        return true;
    }
    sqlite3_finalize(s);
    err = "not found";
    return false;
}

bool Database::ExistsResourceByParentAndName(const CString &pi, const CString &rn, CString &err)
{
    if (!db_) {
        err = "DB not open";
        return false;
    }
    const char   *q  = "SELECT 1 FROM resources WHERE pi = ? AND rn = ? LIMIT 1;";
    sqlite3_stmt *s  = nullptr;
    int           rc = sqlite3_prepare_v2(db_, q, -1, &s, nullptr);
    if (rc != SQLITE_OK) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }
    sqlite3_bind_text(s, 1, pi.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, rn.c_str(), -1, SQLITE_TRANSIENT);
    rc          = sqlite3_step(s);
    bool exists = (rc == SQLITE_ROW);
    sqlite3_finalize(s);
    return exists;
}

bool Database::ExistsAEbyAEID(const CString &aeID, CString &err)
{
    if (!db_) {
        err = "DB not open";
        return false;
    }
    const char   *q  = "SELECT 1 FROM ae WHERE aei = ? LIMIT 1;";
    sqlite3_stmt *s  = nullptr;
    int           rc = sqlite3_prepare_v2(db_, q, -1, &s, nullptr);
    if (rc != SQLITE_OK) {
        err = CString(sqlite3_errmsg(db_));
        return false;
    }
    sqlite3_bind_text(s, 1, aeID.c_str(), -1, SQLITE_TRANSIENT);
    rc          = sqlite3_step(s);
    bool exists = (rc == SQLITE_ROW);
    sqlite3_finalize(s);
    return exists;
}

CString Database::GetFullPathByRI(const CString &ri)
{
    const char *sql = "WITH RECURSIVE path(ri, pi, rn, fullpath) AS ("
                      "  SELECT ri, pi, rn, rn "
                      "  FROM resources "
                      "  WHERE ri = ? "
                      "  UNION ALL "
                      "  SELECT p.ri, p.pi, p.rn, p.rn || '/' || path.fullpath "
                      "  FROM resources p "
                      "  JOIN path ON path.pi = p.ri "
                      ") "
                      "SELECT fullpath "
                      "FROM path "
                      "WHERE pi IS NULL OR pi = '' "
                      "LIMIT 1;";

    sqlite3_stmt *stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return CString();

    sqlite3_bind_text(stmt, 1, ri.c_str(), -1, SQLITE_TRANSIENT);

    CString result;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *s = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        if (s) result = CString(s);
    }

    sqlite3_finalize(stmt);
    return result;
}

bool Database::DeleteDatabaseFile(CString path) { return (f_unlink(path.c_str()) == 0); }
