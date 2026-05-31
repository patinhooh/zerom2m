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
                                      "announceSyncType INTEGER"
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
    "CREATE TABLE IF NOT EXISTS subscription (ri TEXT PRIMARY KEY, enc TEXT, exc INTEGER, nu TEXT, "
    "gpi TEXT, nfu TEXT, bn TEXT, rl TEXT, psn INTEGER, pn INTEGER, nsp INTEGER, ln INTEGER, nct "
    "INTEGER, nec INTEGER, su TEXT, cr TEXT, gn TEXT, acrs TEXT);";

Database::Database() {}

Database::~Database() { Close(); }

bool Database::Open(const char *path, CString &err)
{
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
    // NOTE: WAL is disabled at compile-time (SQLITE_OMIT_WAL).
    // Project uses sqlite3_os_init() stub with Circle VFS.
    // Transactional consistency is maintained via implicit journal (default).
    int   rc;
    char *ze = nullptr;

    // Set journal mode BEFORE pager touches anything
    sqlite3_exec(db_, "PRAGMA journal_mode = OFF;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA page_size = 4096;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous = OFF;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA temp_store = MEMORY;", nullptr, nullptr, nullptr);
    CLogger::Get()->Write("database", LogNotice, "DB opened successfully");
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
CString Database::VecToPacked(const zerom2m::compat::Vector<CString> &v)
{
    CString out;
    for (unsigned i = 0; i < v.GetCount(); ++i) {
        if (i) out += (char)0x1F;
        out += v[i];
    }
    return out;
}

void Database::PackedToVec(const CString &s, zerom2m::compat::Vector<CString> &out)
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
    if (cur.GetLength() > 0) out.push_back(cur);
}

static int bind_text_or_null(sqlite3_stmt *stmt, int idx, const CString &v)
{
    if (v.GetLength() == 0) return sqlite3_bind_null(stmt, idx);
    return sqlite3_bind_text(stmt, idx, v.c_str(), -1, SQLITE_TRANSIENT);
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
    const ResourceBase *base = nullptr;
    CString             ri;
    CString             rn;
    int                 ty = 0;
    CString             pi;

    // helper lambda to insert into resources
    auto insert_resources = [&](const ResourceBase &rbase) -> bool {
        const char *sql =
            "INSERT OR REPLACE INTO resources "
            "(ri,rn,ty,pi,ct,lt,et,labels,acpi,custodian,daci,announceTo,announcedAttribute,"
            "announceSyncType) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
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
        if (rbase.expirationTime.has_value())
            bind_text_or_null(stmt, 7, rbase.expirationTime.value());
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

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            err = CString(sqlite3_errmsg(db_));
            return false;
        }
        return true;
    };

    // switch on kind and insert into type table
    bool ok = true;
    switch (pc.kind()) {
        case PrimitiveContentKind::CSEBase: {
            const CSEBase *r = pc.GetIf<CSEBase>();
            if (!r) {
                ok = false;
                break;
            }
            if (!insert_resources(*r)) {
                ok = false;
                break;
            }
            const char *sql =
                "INSERT OR REPLACE INTO ae (ri, api, aei) VALUES (?,?,?);"; // reuse ae schema for
                                                                            // cse minimal fields
            sqlite3_stmt *stmt = nullptr;
            int           rc   = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                err = CString(sqlite3_errmsg(db_));
                ok  = false;
                break;
            }
            bind_text_or_null(stmt, 1, r->resourceID);
            // store cseID in api field to keep small example; real schema could have cse table
            bind_text_or_null(stmt, 2, CString());
            bind_text_or_null(stmt, 3, r->cseID);
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            if (rc != SQLITE_DONE) {
                err = CString(sqlite3_errmsg(db_));
                ok  = false;
                break;
            }
            break;
        }
        case PrimitiveContentKind::AE: {
            const AE *r = pc.GetIf<AE>();
            if (!r) {
                ok = false;
                break;
            }
            if (!insert_resources(*r)) {
                ok = false;
                break;
            }
            const char *sql =
                "INSERT OR REPLACE INTO ae (ri, api, aei, apn, poa, ontologyRef, nl, csz, regs, "
                "rr, mei, tri, trn, srv, esi, tren) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
            sqlite3_stmt *stmt = nullptr;
            int           rc   = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                err = CString(sqlite3_errmsg(db_));
                ok  = false;
                break;
            }
            bind_text_or_null(stmt, 1, r->resourceID);
            bind_text_or_null(stmt, 2, r->appID);
            bind_text_or_null(stmt, 3, r->aeID);
            bind_text_or_null(stmt, 4, r->appName);
            CString poa = VecToPacked(r->pointOfAccess);
            bind_text_or_null(stmt, 5, poa);
            bind_text_or_null(stmt, 6, r->ontologyRef);
            bind_text_or_null(stmt, 7, r->nodeLink);
            CString csz = VecToPacked(r->contentSerialization);
            bind_text_or_null(stmt, 8, csz);
            if (r->registrationStatus.has_value())
                sqlite3_bind_int(stmt, 9, r->registrationStatus.value());
            else sqlite3_bind_null(stmt, 9);
            if (r->requestReachability.has_value())
                sqlite3_bind_int(stmt, 10, r->requestReachability.value() ? 1 : 0);
            else sqlite3_bind_null(stmt, 10);
            bind_text_or_null(stmt, 11, r->m2mExtID);
            bind_text_or_null(stmt, 12, r->triggerRecipientID);
            if (r->triggerReferenceNumber.has_value())
                sqlite3_bind_int(stmt, 13, r->triggerReferenceNumber.value());
            else sqlite3_bind_null(stmt, 13);
            CString srv = VecToPacked(r->supportedReleaseVersions);
            bind_text_or_null(stmt, 14, srv);
            bind_text_or_null(stmt, 15, r->e2eSecInfo);
            if (r->triggerEnable.has_value())
                sqlite3_bind_int(stmt, 16, r->triggerEnable.value() ? 1 : 0);
            else sqlite3_bind_null(stmt, 16);
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            if (rc != SQLITE_DONE) {
                err = CString(sqlite3_errmsg(db_));
                ok  = false;
                break;
            }
            break;
        }
        case PrimitiveContentKind::Container: {
            const Container *r = pc.GetIf<Container>();
            if (!r) {
                ok = false;
                break;
            }
            if (!insert_resources(*r)) {
                ok = false;
                break;
            }
            const char *sql =
                "INSERT OR REPLACE INTO container (ri, st, cr, mni, mbs, mbis, mia, cni, cbs, "
                "ontologyRef, disr, li, loc) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?);";
            sqlite3_stmt *stmt = nullptr;
            int           rc   = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                err = CString(sqlite3_errmsg(db_));
                ok  = false;
                break;
            }
            bind_text_or_null(stmt, 1, r->resourceID);
            if (r->stateTag.has_value()) sqlite3_bind_int(stmt, 2, r->stateTag.value());
            else sqlite3_bind_null(stmt, 2);
            bind_text_or_null(stmt, 3, r->creator);
            if (r->maxNrOfInstances.has_value())
                sqlite3_bind_int64(stmt, 4, r->maxNrOfInstances.value());
            else sqlite3_bind_null(stmt, 4);
            if (r->maxByteSize.has_value()) sqlite3_bind_int64(stmt, 5, r->maxByteSize.value());
            else sqlite3_bind_null(stmt, 5);
            if (r->maxByteSizePerInstance.has_value())
                sqlite3_bind_int64(stmt, 6, r->maxByteSizePerInstance.value());
            else sqlite3_bind_null(stmt, 6);
            if (r->maxInstanceAge.has_value())
                sqlite3_bind_int64(stmt, 7, r->maxInstanceAge.value());
            else sqlite3_bind_null(stmt, 7);
            sqlite3_bind_int64(stmt, 8, r->currentNrOfInstances);
            sqlite3_bind_int64(stmt, 9, r->currentByteSize);
            bind_text_or_null(stmt, 10, r->ontologyRef);
            if (r->disableRetrieval.has_value())
                sqlite3_bind_int(stmt, 11, r->disableRetrieval.value() ? 1 : 0);
            else sqlite3_bind_null(stmt, 11);
            bind_text_or_null(stmt, 12, r->locationID);
            bind_text_or_null(stmt, 13, r->location);
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            if (rc != SQLITE_DONE) {
                err = CString(sqlite3_errmsg(db_));
                ok  = false;
                break;
            }
            break;
        }
        case PrimitiveContentKind::ContentInstance: {
            const ContentInstance *r = pc.GetIf<ContentInstance>();
            if (!r) {
                ok = false;
                break;
            }
            if (!insert_resources(*r)) {
                ok = false;
                break;
            }
            const char *sql = "INSERT OR REPLACE INTO content_instance (ri, st, cr, cnf, cs, conr, "
                              "con, ontologyRef, dgt, dcnt) VALUES (?,?,?,?,?,?,?,?,?,?);";
            sqlite3_stmt *stmt = nullptr;
            int           rc   = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                err = CString(sqlite3_errmsg(db_));
                ok  = false;
                break;
            }
            bind_text_or_null(stmt, 1, r->resourceID);
            if (r->stateTag.has_value()) sqlite3_bind_int(stmt, 2, r->stateTag.value());
            else sqlite3_bind_null(stmt, 2);
            bind_text_or_null(stmt, 3, r->creator);
            bind_text_or_null(stmt, 4, r->contentInfo);
            sqlite3_bind_int64(stmt, 5, r->contentSize);
            bind_text_or_null(stmt, 6, r->contentRef);
            if (r->content.GetLength() > 0) {
                sqlite3_bind_text(stmt, 7, r->content.c_str(), -1, SQLITE_TRANSIENT);
            } else {
                sqlite3_bind_null(stmt, 7);
            }
            bind_text_or_null(stmt, 8, r->ontologyRef);
            bind_text_or_null(stmt, 9, r->dataGenerationTime);
            if (r->deletionCnt.has_value()) sqlite3_bind_int(stmt, 10, r->deletionCnt.value());
            else sqlite3_bind_null(stmt, 10);
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            if (rc != SQLITE_DONE) {
                err = CString(sqlite3_errmsg(db_));
                ok  = false;
                break;
            }
            break;
        }
        case PrimitiveContentKind::Subscription: {
            const Subscription *r = pc.GetIf<Subscription>();
            if (!r) {
                ok = false;
                break;
            }
            if (!insert_resources(*r)) {
                ok = false;
                break;
            }
            const char   *sql  = "INSERT OR REPLACE INTO subscription (ri, enc, exc, nu, gpi, nfu, "
                                 "bn, rl, psn, pn, nsp, ln, nct, nec, su, cr, gn, acrs) VALUES "
                                 "(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
            sqlite3_stmt *stmt = nullptr;
            int           rc   = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                err = CString(sqlite3_errmsg(db_));
                ok  = false;
                break;
            }
            bind_text_or_null(stmt, 1, r->resourceID);
            // eventNotificationCriteria pack is omitted for brevity; store empty or minimal
            bind_text_or_null(stmt, 2, CString());
            if (r->expirationCounter.has_value())
                sqlite3_bind_int(stmt, 3, r->expirationCounter.value());
            else sqlite3_bind_null(stmt, 3);
            CString nu = VecToPacked(r->notificationURI);
            bind_text_or_null(stmt, 4, nu);
            bind_text_or_null(stmt, 5, r->groupID);
            bind_text_or_null(stmt, 6, r->notificationForwardingURI);
            bind_text_or_null(stmt, 7, CString());
            bind_text_or_null(stmt, 8, CString());
            if (r->preSubscriptionNotify.has_value())
                sqlite3_bind_int(stmt, 9, r->preSubscriptionNotify.value());
            else sqlite3_bind_null(stmt, 9);
            if (r->pendingNotification.has_value())
                sqlite3_bind_int(stmt, 10, r->pendingNotification.value());
            else sqlite3_bind_null(stmt, 10);
            if (r->notificationStoragePriority.has_value())
                sqlite3_bind_int(stmt, 11, r->notificationStoragePriority.value());
            else sqlite3_bind_null(stmt, 11);
            if (r->latestNotify.has_value())
                sqlite3_bind_int(stmt, 12, r->latestNotify.value() ? 1 : 0);
            else sqlite3_bind_null(stmt, 12);
            if (r->notificationContentType.has_value())
                sqlite3_bind_int(stmt, 13, static_cast<int>(r->notificationContentType.value()));
            else sqlite3_bind_null(stmt, 13);
            if (r->notificationEventCat.has_value())
                sqlite3_bind_int(stmt, 14, r->notificationEventCat.value());
            else sqlite3_bind_null(stmt, 14);
            bind_text_or_null(stmt, 15, r->subscriberURI);
            bind_text_or_null(stmt, 16, r->creator);
            bind_text_or_null(stmt, 17, r->groupName);
            bind_text_or_null(stmt, 18, r->associatedCrossResourceSub);
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            if (rc != SQLITE_DONE) {
                err = CString(sqlite3_errmsg(db_));
                ok  = false;
                break;
            }
            break;
        }
        default:
            ok  = false;
            err = "Unsupported PrimitiveContentKind for save";
            break;
    }

    if (!ok) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
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

    const char *sql = "SELECT ri, rn, ty, pi, ct, lt, et, labels FROM resources WHERE ri = ? OR rn "
                      "= ? OR (pi || '/' || rn) = ? LIMIT 1;";
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
    CString ri = CString(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
    int     ty = sqlite3_column_int(stmt, 2);
    sqlite3_finalize(stmt);

    // based on type, query the type table
    switch (static_cast<zerom2m::onem2m::types::ResourceType>(ty)) {
        case ResourceType::AE: {
            const char *q =
                "SELECT api, aei, apn, poa, ontologyRef, nl FROM ae WHERE ri = ? LIMIT 1;";
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
                a.resourceID   = ri;
                a.resourceType = ResourceType::AE;
                a.appID     = CString(reinterpret_cast<const char *>(sqlite3_column_text(s2, 0)));
                a.aeID      = CString(reinterpret_cast<const char *>(sqlite3_column_text(s2, 1)));
                a.appName   = CString(reinterpret_cast<const char *>(sqlite3_column_text(s2, 2)));
                CString poa = CString(reinterpret_cast<const char *>(sqlite3_column_text(s2, 3)));
                PackedToVec(poa, a.pointOfAccess);
                a.ontologyRef = CString(reinterpret_cast<const char *>(sqlite3_column_text(s2, 4)));
                a.nodeLink    = CString(reinterpret_cast<const char *>(sqlite3_column_text(s2, 5)));
                out           = a;
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
                c.resourceID   = ri;
                c.resourceType = ResourceType::Container;
                if (sqlite3_column_type(s2, 0) != SQLITE_NULL)
                    c.stateTag = (u32)sqlite3_column_int(s2, 0);
                c.creator = CString(reinterpret_cast<const char *>(sqlite3_column_text(s2, 1)));
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
                c.ontologyRef = CString(reinterpret_cast<const char *>(sqlite3_column_text(s2, 8)));
                if (sqlite3_column_type(s2, 9) != SQLITE_NULL)
                    c.disableRetrieval = (sqlite3_column_int(s2, 9) != 0);
                c.locationID = CString(reinterpret_cast<const char *>(sqlite3_column_text(s2, 10)));
                c.location   = CString(reinterpret_cast<const char *>(sqlite3_column_text(s2, 11)));
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
                ci.resourceID   = ri;
                ci.resourceType = ResourceType::ContentInstance;
                if (sqlite3_column_type(s2, 0) != SQLITE_NULL)
                    ci.stateTag = (u32)sqlite3_column_int(s2, 0);
                ci.creator = CString(reinterpret_cast<const char *>(sqlite3_column_text(s2, 1)));
                ci.contentInfo =
                    CString(reinterpret_cast<const char *>(sqlite3_column_text(s2, 2)));
                ci.contentSize = (s64)sqlite3_column_int64(s2, 3);
                ci.contentRef = CString(reinterpret_cast<const char *>(sqlite3_column_text(s2, 4)));
                if (sqlite3_column_type(s2, 5) != SQLITE_NULL) {
                    const char *txt = reinterpret_cast<const char *>(sqlite3_column_text(s2, 5));
                    ci.content      = CString(txt ? txt : "");
                }
                ci.ontologyRef =
                    CString(reinterpret_cast<const char *>(sqlite3_column_text(s2, 6)));
                ci.dataGenerationTime =
                    CString(reinterpret_cast<const char *>(sqlite3_column_text(s2, 7)));
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
            const char   *q   = "SELECT nu FROM subscription WHERE ri = ? LIMIT 1;";
            sqlite3_stmt *s2  = nullptr;
            int           rc2 = sqlite3_prepare_v2(db_, q, -1, &s2, nullptr);
            if (rc2 != SQLITE_OK) {
                err = CString(sqlite3_errmsg(db_));
                return false;
            }
            sqlite3_bind_text(s2, 1, ri.c_str(), -1, SQLITE_TRANSIENT);
            rc2 = sqlite3_step(s2);
            if (rc2 == SQLITE_ROW) {
                Subscription s;
                s.resourceID   = ri;
                s.resourceType = ResourceType::Subscription;
                CString nu = CString(reinterpret_cast<const char *>(sqlite3_column_text(s2, 0)));
                PackedToVec(nu, s.notificationURI);
                out = s;
                sqlite3_finalize(s2);
                return true;
            }
            sqlite3_finalize(s2);
            err = "subscription row not found";
            return false;
        }
        case ResourceType::CSEBase: {
            // stored in ae table as minimal entry in Save; read back
            const char   *q   = "SELECT aei FROM ae WHERE ri = ? LIMIT 1;";
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
                c.resourceID   = ri;
                c.resourceType = ResourceType::CSEBase;
                c.cseID = CString(reinterpret_cast<const char *>(sqlite3_column_text(s2, 0)));
                out     = c;
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

bool Database::ExistsAEByAEID(const CString &aeid, CString &err)
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
    sqlite3_bind_text(s, 1, aeid.c_str(), -1, SQLITE_TRANSIENT);
    rc          = sqlite3_step(s);
    bool exists = (rc == SQLITE_ROW);
    sqlite3_finalize(s);
    return exists;
}
