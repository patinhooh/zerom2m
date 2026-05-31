/*
 * database.h
 *
 * SQLite wrapper for ZeroM2M resources
 */
#pragma once

#include <circle/types.h>
#include <circle/string.h>
#include <zerom2m/onem2m/types/primitives.h>

#include <zerom2m/sqlite/sqlite3.h>

namespace zerom2m::sqlite
{
using zerom2m::onem2m::types::PrimitiveContent;
using zerom2m::onem2m::types::CSEBase;

class Database {
public:
    Database();
    ~Database();

    bool Open(const char *path, CString &err);
    void Close();

    bool InitSchema(CString &err);

    // Save a primitive content (resource) into DB. Uses ri as primary key.
    bool SavePrimitiveContent(const PrimitiveContent &pc, CString &err);

    // Load by target: matches ri, rn or pi/rn. Target is normalized (no leading '/').
    bool LoadPrimitiveContentByTarget(const CString &target, PrimitiveContent &out, CString &err);

    // Get first CSEBase (if any)
    bool GetCSEBase(CSEBase &out, CString &err);
    bool ExistsAEByAEID(const CString &aeid, CString &err);

private:
    sqlite3 *db_ = nullptr;

    // helpers
    static CString VecToPacked(const zerom2m::compat::Vector<CString> &v);
    static void PackedToVec(const CString &s, zerom2m::compat::Vector<CString> &out);
};

} // namespace zerom2m::sqlite
