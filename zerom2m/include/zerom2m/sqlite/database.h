/*
 * database.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <circle/string.h>
#include <circle/types.h>
#include <zerom2m/onem2m/types/primitives.h>

#include <zerom2m/sqlite/sqlite3.h>

namespace zerom2m::sqlite
{
using namespace zerom2m::onem2m::types;

class Database
{
public:
    Database();
    ~Database();

    bool Open(const char *path, CString &err);
    void Close();

    bool InitSchema();

    // Save a primitive content (resource) into DB. Uses ri as primary key.
    bool SavePrimitiveContent(const PrimitiveContent &pc, CString &err);
    bool SaveCSE(const CSEBase &cse, CString &err);
    bool SaveAE(const AE &ae, CString &err);
    bool SaveContainer(const Container &cnt, CString &err);
    bool SaveContentInstance(const ContentInstance &cin, CString &err);
    bool SaveSubscription(const Subscription &sub, CString &err);

    // Load by target: matches ri, rn or pi/rn. Target is normalized (no leading '/').
    bool LoadPrimitiveContentByTarget(const CString &target, PrimitiveContent &out, CString &err);

    // Get first CSEBase (if any)
    bool GetCSEBase(CSEBase &out, CString &err);
    bool GetPathByRI(const CString &ri, CString &out, CString &err);
    bool ExistsAEbyAEID(const CString &aeID, CString &err);
    bool ExistsResourceByParentAndName(const CString &pi, const CString &rn, CString &err);

    bool DeleteDatabaseFile(CString path);

private:
    sqlite3 *db_ = nullptr;

    // helpers
    static CString VecToPacked(const zerom2m::compat::Vector<CString> &v);
    static void    PackedToVec(const CString &s, zerom2m::compat::Vector<CString> &out);
    bool           InsertResources(const zerom2m::onem2m::types::ResourceBase &rbase, CString &err);
    CString        GetFullPathByRI(const CString &ri);
};

} // namespace zerom2m::sqlite
