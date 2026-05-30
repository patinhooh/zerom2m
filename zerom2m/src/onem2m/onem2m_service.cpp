/*
 * onem2m_service.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include <zerom2m/compat/types.h>
#include <zerom2m/config/system_config.h>
#include <zerom2m/onem2m/onem2m_service.h>
#include <zerom2m/onem2m/types/primitives.h>

#include <circle/logger.h>
#include <circle/util.h>

#include <string.h>

namespace zerom2m::onem2m
{

using namespace zerom2m::onem2m::types;
using zerom2m::config::SystemConfig;

namespace
{
CString SliceCString(const char *start, size_t length)
{
    char *buf = new char[length + 1];
    memcpy(buf, start, length);
    buf[length] = '\0';
    CString out(buf);
    delete[] buf;
    return out;
}

bool LooksLikeServiceProviderId(const CString &spid)
{
    const char *text = spid.c_str();
    if (text == nullptr || *text == '\0') return false;
    return strchr(text, '.') != nullptr || strchr(text, '-') != nullptr ||
           strchr(text, ':') != nullptr;
}

bool ParseAddressingPath(const CString   &path,
                         Vector<CString> &segments,
                         bool            &absolute,
                         bool            &networkPrefix,
                         CString         &spid)
{
    segments.clear();
    spid          = CString();
    networkPrefix = false;
    absolute      = false;

    const char *raw = path.c_str();
    if (raw == nullptr || raw[0] != '/') return false;

    const size_t len = path.GetLength();
    if (len > 1 && raw[len - 1] == '/') return false;

    networkPrefix    = len > 2 && raw[1] == '~' && raw[2] == '/';
    bool underscored = len > 2 && raw[1] == '_' && raw[2] == '/';
    absolute         = (len > 1 && raw[1] == '/' && !networkPrefix) || underscored;
    size_t pos       = networkPrefix ? 3 : (absolute ? (underscored ? 3 : 2) : 1);

    if (absolute) {
        size_t start = pos;
        while (raw[pos] != '\0' && raw[pos] != '/') ++pos;
        if (pos == start) return false;

        spid = SliceCString(raw + start, pos - start);

        // Only enforce FQDN-like check for // form; /_/ accepts any non-empty SPID token
        if (!underscored && !LooksLikeServiceProviderId(spid)) return false;

        if (raw[pos] != '/') return false;
        ++pos;
        if (raw[pos] == '\0') return false;
    }

    // For networkPrefix paths, peek at the first segment to detect
    // absolute addressing: /~/spid/cseid/... vs SP-relative: /~/cseid/...
    if (networkPrefix) {
        size_t segStart = pos;
        size_t segEnd   = pos;
        while (raw[segEnd] != '\0' && raw[segEnd] != '/')
            ++segEnd;
        if (segEnd == segStart) return false;

        CString firstSeg = SliceCString(raw + segStart, segEnd - segStart);
        if (LooksLikeServiceProviderId(firstSeg)) {
            // Absolute via /~/: extract SPID and mark as absolute
            spid          = firstSeg;
            absolute      = true;
            networkPrefix = false; // treat remainder as normal after SPID extraction
            pos           = segEnd;
            if (raw[pos] != '/') return false;
            ++pos;
            if (raw[pos] == '\0') return false;
        }
        // else: SP-relative /~/cseid/... — leave pos as-is, parse normally
    }

    while (raw[pos] != '\0') {
        size_t start = pos;
        while (raw[pos] != '\0' && raw[pos] != '/')
            ++pos;
        if (pos == start) return false;

        segments.push_back(SliceCString(raw + start, pos - start));

        if (raw[pos] == '/') {
            ++pos;
            if (raw[pos] == '\0') return false;
        }
    }

    return true;
}

CString CanonicalizeAddressingPath(const CString &path,
                                   const CString &cseName,
                                   const CString &cseId,
                                   const CString &spid,
                                   bool          &valid,
                                   bool          &wrongSpid)
{
    valid     = false;
    wrongSpid = false;

    Vector<CString> segments;
    CString         parsedSpid;
    bool            absolute      = false;
    bool            networkPrefix = false;
    if (!ParseAddressingPath(path, segments, absolute, networkPrefix, parsedSpid)) return CString();

    CString cseIdName = cseId;
    if (cseIdName.GetLength() > 0 && cseIdName.c_str()[0] == '/') {
        cseIdName = CString(cseIdName.c_str() + 1);
    }

    // Validate SPID for absolute addressing
    if (absolute && parsedSpid.GetLength() > 0) {
        CString knownSpid = spid;
        if (knownSpid.Compare(parsedSpid) != 0) {
            wrongSpid = true;
            return CString();
        }
    }

    unsigned start = 0;
    if (absolute) {
        // After SPID, remaining segments are: cseid/rn/... or cseid/ri
        // segments[0] should be cseid, segments[1] the cseName or "-" or a resource
        if (segments.GetCount() < 2) return CString(); // BAD_REQUEST: only cseid, no resource
        if (segments[0].Compare(cseIdName) != 0) return CString();
        start = 1; // skip the cseid segment
    } else if (networkPrefix) {
        // SP-relative: /~/cseid/...
        if (segments.GetCount() == 0) return CString();
        if (segments[0].Compare(cseIdName) != 0) return CString();
        if (segments.GetCount() == 1) return CString(); // BAD_REQUEST: only cseid
        // Unstructured shortcut: /~/cseid/ri (exactly 2 segments, second not cseName or "-")
        if (segments.GetCount() == 2 && segments[1].Compare(cseIdName) != 0 &&
            segments[1].Compare(cseName) != 0 && segments[1].Compare("-") != 0) {
            CString out("/");
            out.Append(segments[1].c_str());
            valid = true;
            return out;
        }
        start = 1; // skip cseid
    } else if (segments.GetCount() >= 2 && segments[0].Compare(cseIdName) == 0 &&
               (segments[1].Compare(cseIdName) == 0 || segments[1].Compare("-") == 0)) {
        start = 1;
    }

    Vector<CString> normalized;
    for (unsigned i = start; i < segments.GetCount(); ++i) {
        if (i == start && segments[i].Compare("-") == 0) {
            normalized.push_back(cseName);
        } else {
            normalized.push_back(segments[i]);
        }
    }

    if (normalized.GetCount() == 0) return CString();

    CString out("/");
    for (unsigned i = 0; i < normalized.GetCount(); ++i) {
        if (i > 0) out.Append("/");
        out.Append(normalized[i].c_str());
    }

    valid = true;
    return out;
}

CString NormalizePath(const CString &path)
{
    if (path.GetLength() > 0 && path.c_str()[0] == '/') return CString(path.c_str() + 1);
    return path;
}

const ResourceBase *GetResourceBase(const PrimitiveContent &pc)
{
    if (const auto *r = pc.GetIf<Container>()) return r;
    if (const auto *r = pc.GetIf<ContentInstance>()) return r;
    if (const auto *r = pc.GetIf<AE>()) return r;
    if (const auto *r = pc.GetIf<Group>()) return r;
    if (const auto *r = pc.GetIf<Subscription>()) return r;
    if (const auto *r = pc.GetIf<AccessControlPolicy>()) return r;
    if (const auto *r = pc.GetIf<CSEBase>()) return r;
    return nullptr;
}

const PrimitiveContent *FindByResourceId(const Vector<PrimitiveContent> &db, const CString &rid)
{
    for (unsigned i = 0; i < db.GetCount(); ++i) {
        const PrimitiveContent &pc   = db[i];
        const ResourceBase     *base = GetResourceBase(pc);
        if (base && base->resourceID.Compare(rid) == 0) return &pc;
    }
    return nullptr;
}

CString BuildFullPath(const Vector<PrimitiveContent> &db, const ResourceBase *rbase)
{
    if (!rbase) return CString();

    CString full = "/";
    full += rbase->resourceName;

    CString  parentId = rbase->parentID;
    unsigned guard    = 0;
    while (parentId.GetLength() != 0 && guard++ < 32) {
        const PrimitiveContent *parentPc = FindByResourceId(db, parentId);
        if (!parentPc) break;
        const ResourceBase *parentBase = GetResourceBase(*parentPc);
        if (!parentBase) break;

        CString newFull = "/";
        newFull += parentBase->resourceName;
        newFull += full;
        full     = newFull;
        parentId = parentBase->parentID;
    }

    return full;
}

CString ResolveParentId(const Vector<PrimitiveContent> &db, const CString &rawTarget)
{
    CString normTarget = NormalizePath(rawTarget);

    if (db.GetCount() > 0) {
        if (const CSEBase *c = db[0].GetIf<CSEBase>()) {
            CString cseFull = BuildFullPath(db, c);
            if (normTarget.Compare("m2m") == 0 || normTarget.Compare(c->resourceID) == 0 ||
                normTarget.Compare(c->resourceName) == 0 ||
                NormalizePath(cseFull).Compare(normTarget) == 0) {
                return c->resourceID;
            }
        }
    }

    for (unsigned i = 0; i < db.GetCount(); ++i) {
        const PrimitiveContent &candidate = db[i];
        const ResourceBase     *base      = GetResourceBase(candidate);
        if (!base) continue;

        CString full = BuildFullPath(db, base);
        if (NormalizePath(full).Compare(normTarget) == 0 ||
            base->resourceID.Compare(normTarget) == 0 ||
            base->resourceName.Compare(normTarget) == 0) {
            return base->resourceID;
        }
    }

    return normTarget;
}

boolean IsValidContentInfo(const CString &contentInfo)
{
    if (contentInfo.GetLength() == 0) return true;

    const char *text  = contentInfo.c_str();
    const char *slash = nullptr;
    const char *colon = nullptr;
    for (const char *p = text; *p != '\0'; ++p) {
        if (*p == '/') {
            if (slash != nullptr) return false;
            slash = p;
        } else if (*p == ':') {
            if (colon != nullptr) return false;
            colon = p;
        }
    }

    if (!slash || !colon || slash > colon) return false;
    if (slash == text || colon == slash + 1 || *(colon + 1) == '\0') return false;
    if (strchr(colon + 1, ':') != nullptr) return false;

    for (const char *p = colon + 1; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') return false;
    }

    int charset = 0;
    for (const char *p = colon + 1; *p != '\0'; ++p) {
        charset = charset * 10 + (*p - '0');
        if (charset > 8) return false;
    }

    return true;
}
} // namespace

void OneM2MService::Initialize(const SystemConfig &config)
{
    CLogger::Get()->Write("onem2m_service", LogNotice, "Initialize() called");

    if (initialized_) return;

    db_.clear();
    nextResourceId_ = 1;

    // set global SPID for addressing validation
    spId_ = config.cse.sp_id;

    CSEBase cse;
    cse.resourceType = ResourceType::CSEBase;
    cse.resourceName = config.cse.resource_name;
    cse.resourceID   = config.cse.resource_id;
    cse.parentID     = "";

    cse.creationTime     = "2026-01-01T00:00:00Z";
    cse.lastModifiedTime = cse.creationTime;

    cse.cseType     = CSEType::IN_CSE;
    cse.cseID       = config.cse.cse_id;
    cse.currentTime = cse.creationTime;
    cse.supportedReleaseVersions.push_back("4");

    // Only advertise the resource types currently supported by this project.
    static const ResourceType kSupportedTypes[] = {
        ResourceType::CSEBase,
        ResourceType::AE,
        ResourceType::Container,
        ResourceType::ContentInstance,
        ResourceType::Subscription,
    };
    for (ResourceType type : kSupportedTypes) {
        cse.supportedResourceType.push_back(type);
    }

    PrimitiveContent pc;
    pc = cse;

    db_.push_back(pc);

    CString msg;
    msg.Format("Inserted CSEBase: rn='%s' ri='%s' pi='%s' db_size=%u",
               cse.resourceName.c_str(),
               cse.resourceID.c_str(),
               cse.parentID.c_str(),
               db_.GetCount());

    CLogger::Get()->Write("onem2m_service", LogNotice, msg);

    initialized_ = true;

    CLogger::Get()->Write("onem2m_service", LogNotice, "Initialize() complete");
}

ResponsePrimitive OneM2MService::HandleRequest(const RequestPrimitive &request)
{
    CString msg;
    msg.Format("HandleRequest op=%d to='%s'", static_cast<int>(request.op), request.to.c_str());
    CLogger::Get()->Write("onem2m_service", LogNotice, msg);

    switch (request.op) {
        case Operation::Create:
            return Create(request);

        case Operation::Retrieve:
            return Retrieve(request);

        case Operation::Update:
            return Update(request);

        case Operation::Delete:
            return Delete(request);

        case Operation::Notify:
            return Notify(request);

        default: {
            ResponsePrimitive r;
            r.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return r;
        }
    }
}

ResponsePrimitive OneM2MService::Create(const RequestPrimitive &request)
{
    ResponsePrimitive resp;

    const CSEBase *cse         = db_.GetCount() > 0 ? db_[0].GetIf<CSEBase>() : nullptr;
    bool           targetValid = false;
    bool           wrongSpid   = false;
    CString        target =
        cse ? CanonicalizeAddressingPath(
                  request.to, cse->resourceName, cse->cseID, spId_, targetValid, wrongSpid)
            : CString();
    if (!targetValid) {
        resp.responseStatusCode =
            wrongSpid ? ResponseStatusCode::NOT_FOUND : ResponseStatusCode::BAD_REQUEST;
        return resp;
    }

    if (request.from.GetLength() == 0) {
        if (!request.resourceType.has_value() || request.resourceType.value() != ResourceType::AE) {
            CLogger::Get()->Write(
                "onem2m_service", LogNotice, "Create request missing 'from' and not creating AE");
            resp.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return resp;
        }
    }

    CString err;
    if (!isValid(request, err)) {
        CString msg;
        msg.Format("Create request invalid: %s", err.c_str());
        CLogger::Get()->Write("onem2m_service", LogNotice, msg);
        resp.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
        return resp;
    }

    if (request.content.empty()) {
        // #1
        CString msg;
        msg.Format("Create request content is empty: %s", err.c_str());
        CLogger::Get()->Write("onem2m_service", LogNotice, msg);
        resp.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
        return resp;
    }

    PrimitiveContent pc = request.content; // copy

    auto assignIdAndParent = [&](auto &res) {
        CString id;
        id.Format("%u", nextResourceId_++);
        res.resourceID = id;
        if (res.resourceName.GetLength() == 0) res.resourceName = id;
        res.parentID = ResolveParentId(db_, target);
    };

    // TODO: This is a bit clunky, but it works for now. We can later add a more elegant way of
    // handling this

    // Try known resource types and assign ids/parent
    if (auto p = pc.GetIf<Container>()) {
        Container r = *p;
        assignIdAndParent(r);
        r.resourceType = ResourceType::Container;

        if (r.creationTime.GetLength() == 0) r.creationTime = "2026-01-01T00:00:00Z";
        if (r.lastModifiedTime.GetLength() == 0) r.lastModifiedTime = r.creationTime;
        if (!r.expirationTime.has_value()) r.expirationTime = CString("2027-01-01T00:00:00Z");
        if (!r.stateTag.has_value()) r.stateTag = 0;

        if (request.vendorInformation.has_value()) {
            if (request.vendorInformation->Compare("cnt_creator_present") == 0) {
                CLogger::Get()->Write(
                    "onem2m_service", LogNotice, "Create Container request with invalid creator");
                ResponsePrimitive bad;
                bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
                return bad;
            }
            if (request.vendorInformation->Compare("cnt_creator_null") == 0) {
                if (request.from.GetLength() == 0) {
                    CLogger::Get()->Write(
                        "onem2m_service",
                        LogNotice,
                        "Create Container request with null creator and missing originator");
                    ResponsePrimitive bad;
                    bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
                    return bad;
                }
                r.creator = request.from;
            }
        }

        pc = r;
    } else if (auto p = pc.GetIf<ContentInstance>()) {
        ContentInstance r = *p;
        assignIdAndParent(r);
        r.resourceType = ResourceType::ContentInstance;

        if (!IsValidContentInfo(r.contentInfo.has_value() ? *r.contentInfo : CString())) {
            CLogger::Get()->Write("onem2m_service",
                                  LogNotice,
                                  "Create ContentInstance request with invalid contentInfo");
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return bad;
        }

        if (r.creationTime.GetLength() == 0) r.creationTime = "2026-01-01T00:00:00Z";
        if (r.lastModifiedTime.GetLength() == 0) r.lastModifiedTime = r.creationTime;
        if (!r.expirationTime.has_value()) r.expirationTime = CString("2027-01-01T00:00:00Z");
        if (!r.stateTag.has_value()) r.stateTag = 0;

        // Creator handling: reject explicit creator value, allow null -> set to originator
        if (request.vendorInformation.has_value()) {
            if (request.vendorInformation->Compare("cin_creator_present") == 0) {
                CLogger::Get()->Write("onem2m_service",
                                      LogNotice,
                                      "Create ContentInstance request with invalid creator");
                ResponsePrimitive bad;
                bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
                return bad;
            }
            if (request.vendorInformation->Compare("cin_creator_null") == 0) {
                if (request.from.GetLength() == 0) {
                    CLogger::Get()->Write(
                        "onem2m_service",
                        LogNotice,
                        "Create ContentInstance request with null creator and missing originator");
                    ResponsePrimitive bad;
                    bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
                    return bad;
                }
                r.creator = request.from;
            }
            if (request.vendorInformation->Compare("cin_has_acpi") == 0) {
                CLogger::Get()->Write("onem2m_service",
                                      LogNotice,
                                      "Create ContentInstance request with invalid acpi");
                ResponsePrimitive bad;
                bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
                return bad;
            }
        }

        // Validate parent: CIN must not be created under AE
        CString parentId = r.parentID;
        if (parentId.GetLength() != 0) {
            const PrimitiveContent *parentPc = FindByResourceId(db_, parentId);
            if (parentPc) {
                if (parentPc->GetIf<AE>()) {
                    CLogger::Get()->Write("onem2m_service",
                                          LogNotice,
                                          "Create ContentInstance request with AE parent");
                    ResponsePrimitive bad;
                    bad.responseStatusCode = ResponseStatusCode::INVALID_CHILD_RESOURCE_TYPE;
                    return bad;
                }
            }
        }

        // Ensure contentSize is set (codec may have estimated it)
        if (r.contentSize <= 0) { r.contentSize = static_cast<s64>(r.content.GetLength()); }

        // Policy checks against parent Container (mbs, mni) and potential eviction.
        if (r.parentID.GetLength() != 0) {
            // locate parent container index
            int parentIdx = -1;
            for (unsigned pi = 0; pi < db_.GetCount(); ++pi) {
                const PrimitiveContent &cand = db_[pi];
                if (const Container *pcnt = cand.GetIf<Container>()) {
                    if (pcnt->resourceID.Compare(r.parentID) == 0) {
                        parentIdx = static_cast<int>(pi);
                        break;
                    }
                }
            }
            if (parentIdx >= 0) {
                const Container *pcnt = db_[parentIdx].GetIf<Container>();
                if (pcnt) {
                    // Enforce max byte size per container: evict oldest CINs until new fits
                    if (pcnt->maxByteSize.has_value()) {
                        s64 mbs          = *pcnt->maxByteSize;
                        s64 currentBytes = pcnt->currentByteSize;
                        CLogger::Get()->Write(
                            "onem2m_service",
                            LogNotice,
                            "MBS check parent='%s' currentBytes=%lld newSize=%lld mbs=%lld",
                            pcnt->resourceID.c_str(),
                            (long long)currentBytes,
                            (long long)r.contentSize,
                            (long long)mbs);
                        if (currentBytes + r.contentSize > mbs) {
                            // collect existing CINs for this container (resourceID and creationTime
                            // and size)
                            struct Cand2 {
                                CString ri;
                                CString ct;
                                s64     size;
                            };
                            Vector<Cand2> cands;
                            for (unsigned ci = 0; ci < db_.GetCount(); ++ci) {
                                const PrimitiveContent &child = db_[ci];
                                if (const ContentInstance *cin = child.GetIf<ContentInstance>()) {
                                    if (cin->parentID.Compare(pcnt->resourceID) == 0) {
                                        Cand2 cc;
                                        cc.ri   = cin->resourceID;
                                        cc.ct   = cin->creationTime;
                                        cc.size = cin->contentSize;
                                        cands.push_back(cc);
                                    }
                                }
                            }
                            // remove oldest until fits or run out
                            // find oldest by CT
                            while (currentBytes + r.contentSize > mbs && cands.GetCount() > 0) {
                                unsigned oldestPos = 0;
                                for (unsigned k = 1; k < cands.GetCount(); ++k) {
                                    if (cands[k].ct.Compare(cands[oldestPos].ct) < 0) oldestPos = k;
                                }
                                CString removeRi   = cands[oldestPos].ri;
                                s64     removeSize = cands[oldestPos].size;
                                CLogger::Get()->Write(
                                    "onem2m_service",
                                    LogNotice,
                                    "Evicting CIN ri='%s' size=%lld to satisfy mbs",
                                    removeRi.c_str(),
                                    (long long)removeSize);

                                // rebuild db_ skipping the to-be-removed CIN (by resourceID)
                                Vector<PrimitiveContent> newdb;
                                for (unsigned x = 0; x < db_.GetCount(); ++x) {
                                    const PrimitiveContent &pcx  = db_[x];
                                    const ContentInstance  *cinx = pcx.GetIf<ContentInstance>();
                                    if (cinx && cinx->resourceID.Compare(removeRi) == 0) continue;
                                    newdb.push_back(db_[x]);
                                }
                                db_ = newdb;

                                currentBytes -= removeSize;

                                // remove entry from cands list
                                Vector<Cand2> newc;
                                for (unsigned ci = 0; ci < cands.GetCount(); ++ci) {
                                    if (ci == oldestPos) continue;
                                    newc.push_back(cands[ci]);
                                }
                                cands = newc;
                            }
                            // After eviction attempts, update parent container counters
                            int updatedParentIdx = -1;
                            for (unsigned pi2 = 0; pi2 < db_.GetCount(); ++pi2) {
                                const PrimitiveContent &cand2 = db_[pi2];
                                if (const Container *pc = cand2.GetIf<Container>()) {
                                    if (pc->resourceID.Compare(pcnt->resourceID) == 0) {
                                        updatedParentIdx = static_cast<int>(pi2);
                                        break;
                                    }
                                }
                            }
                            if (updatedParentIdx >= 0) {
                                Container updated = *db_[updatedParentIdx].GetIf<Container>();
                                // recompute counters
                                s64 sum   = 0;
                                s64 count = 0;
                                for (unsigned ci = 0; ci < db_.GetCount(); ++ci) {
                                    const PrimitiveContent &child = db_[ci];
                                    if (const ContentInstance *cin =
                                            child.GetIf<ContentInstance>()) {
                                        if (cin->parentID.Compare(updated.resourceID) == 0) {
                                            sum += cin->contentSize;
                                            ++count;
                                        }
                                    }
                                }
                                updated.currentNrOfInstances = count;
                                updated.currentByteSize      = sum;
                                db_[updatedParentIdx]        = updated;
                                pcnt         = db_[updatedParentIdx].GetIf<Container>();
                                currentBytes = sum; // update local cumulative size after eviction
                                CLogger::Get()->Write(
                                    "onem2m_service",
                                    LogNotice,
                                    "After eviction parent='%s' cni=%lld cbs=%lld",
                                    updated.resourceID.c_str(),
                                    (long long)updated.currentNrOfInstances,
                                    (long long)updated.currentByteSize);
                            }

                            if (currentBytes + r.contentSize > mbs) {
                                ResponsePrimitive bad;
                                bad.responseStatusCode = ResponseStatusCode::NOT_ACCEPTABLE;
                                return bad;
                            }
                        }
                    }

                    // Enforce max number of instances (evict oldest if necessary)
                    if (pcnt->maxNrOfInstances.has_value()) {
                        s64 desired = *pcnt->maxNrOfInstances;
                        s64 current = pcnt->currentNrOfInstances;
                        if (current + 1 > desired) {
                            // collect existing CINs for this container
                            struct Cand {
                                unsigned idx;
                                CString  ct;
                                s64      size;
                            };
                            Vector<Cand> cands;
                            for (unsigned ci = 0; ci < db_.GetCount(); ++ci) {
                                const PrimitiveContent &child = db_[ci];
                                if (const ContentInstance *cin = child.GetIf<ContentInstance>()) {
                                    if (cin->parentID.Compare(pcnt->resourceID) == 0) {
                                        Cand cc;
                                        cc.idx  = ci;
                                        cc.ct   = cin->creationTime;
                                        cc.size = cin->contentSize;
                                        cands.push_back(cc);
                                    }
                                }
                            }
                            // sort candidates by creationTime asc (oldest first) - simple selection
                            // removal
                            while (pcnt->currentNrOfInstances + 1 > desired &&
                                   cands.GetCount() > 0) {
                                // find oldest index in cands
                                unsigned oldestPos = 0;
                                for (unsigned k = 1; k < cands.GetCount(); ++k) {
                                    if (cands[k].ct.Compare(cands[oldestPos].ct) < 0) oldestPos = k;
                                }
                                unsigned removeDbIdx = cands[oldestPos].idx;

                                // rebuild db_ skipping the to-be-removed CIN
                                Vector<PrimitiveContent> newdb;
                                for (unsigned x = 0; x < db_.GetCount(); ++x) {
                                    if (x == removeDbIdx) continue;
                                    newdb.push_back(db_[x]);
                                }
                                db_ = newdb;

                                // adjust container counters (find parent again)
                                // (we will search and update below after loop; recreate candidate
                                // list) rebuild candidate list for next iteration
                                cands.clear();
                                for (unsigned ci = 0; ci < db_.GetCount(); ++ci) {
                                    const PrimitiveContent &child = db_[ci];
                                    if (const ContentInstance *cin =
                                            child.GetIf<ContentInstance>()) {
                                        if (cin->parentID.Compare(pcnt->resourceID) == 0) {
                                            Cand cc;
                                            cc.idx  = ci;
                                            cc.ct   = cin->creationTime;
                                            cc.size = cin->contentSize;
                                            cands.push_back(cc);
                                        }
                                    }
                                }
                                // update pointer to parent container (its index may have shifted)
                                parentIdx = -1;
                                for (unsigned pi = 0; pi < db_.GetCount(); ++pi) {
                                    const PrimitiveContent &cand = db_[pi];
                                    if (const Container *pc = cand.GetIf<Container>()) {
                                        if (pc->resourceID.Compare(r.parentID) == 0) {
                                            parentIdx = static_cast<int>(pi);
                                            break;
                                        }
                                    }
                                }
                                if (parentIdx >= 0) {
                                    // decrement counters based on the removed CIN
                                    Container updated = *db_[parentIdx].GetIf<Container>();
                                    updated.currentNrOfInstances =
                                        static_cast<s64>(cands.GetCount());
                                    // recompute currentByteSize
                                    s64 sum = 0;
                                    for (unsigned ci = 0; ci < db_.GetCount(); ++ci) {
                                        const PrimitiveContent &child = db_[ci];
                                        if (const ContentInstance *cin =
                                                child.GetIf<ContentInstance>()) {
                                            if (cin->parentID.Compare(updated.resourceID) == 0)
                                                sum += cin->contentSize;
                                        }
                                    }
                                    updated.currentByteSize = sum;
                                    db_[parentIdx]          = updated;
                                    pcnt                    = db_[parentIdx].GetIf<Container>();
                                } else {
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }

        pc = r;
    } else if (auto p = pc.GetIf<AE>()) {
        AE r = *p;
        assignIdAndParent(r);
        r.resourceType = ResourceType::AE;
        // If no aeID present, assign one derived from the resource id and a 'C' prefix
        if (r.aeID.GetLength() == 0) {
            CString aid;
            aid.Format("C%s", r.resourceID.c_str());
            r.aeID = aid;
        }

        // Validate resourceName (rn) characters: allow alnum and -._~ only
        if (r.resourceName.GetLength() != 0) {
            for (size_t ci = 0; ci < r.resourceName.GetLength(); ++ci) {
                char c = r.resourceName.c_str()[ci];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                      c == '-' || c == '.' || c == '_' || c == '~')) {
                    ResponsePrimitive bad;
                    bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
                    return bad;
                }
            }
        }

        // XXX: In a real implementation, these would be set to the current time and calculated
        // based on the request, but for testing we use fixed values.
        if (r.creationTime.GetLength() == 0) r.creationTime = "2026-01-01T00:00:00Z";
        if (r.lastModifiedTime.GetLength() == 0) r.lastModifiedTime = r.creationTime;
        if (!r.expirationTime.has_value()) r.expirationTime = CString("2027-01-01T00:00:00Z");

        // Validate API prefix for RVI 4: API must not be lower-case when RVI == 4
        if (request.releaseVersionIndicator.has_value() &&
            request.releaseVersionIndicator->Compare("4") == 0) {
            if (r.appID.GetLength() != 0) {
                char fc = r.appID.c_str()[0];
                if (fc >= 'a' && fc <= 'z') {
                    ResponsePrimitive bad;
                    bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
                    return bad;
                }
            }
        }
        // If the originator is the CSE ID (without leading '/'), require security association
        if (request.from.GetLength() != 0) {
            // compare to cse id stored in db_[0] if available
            if (db_.GetCount() > 0) {
                if (const CSEBase *c = db_[0].GetIf<CSEBase>()) {
                    CString cseid = c->cseID;
                    if (cseid.GetLength() > 0 && cseid.c_str()[0] == '/')
                        cseid = CString(cseid.c_str() + 1);
                    if (request.from.Compare(cseid) == 0) {
                        ResponsePrimitive respSec;
                        respSec.responseStatusCode =
                            ResponseStatusCode::SECURITY_ASSOCIATION_REQUIRED;
                        return respSec;
                    }
                }
            }
        }
        // Reject Create requests that explicitly include the 'cr' (creator) attribute
        if (request.vendorInformation.has_value() &&
            request.vendorInformation->Compare("has_creator") == 0) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return bad;
        }

        // Reject csz attribute for AE: not supported in this implementation
        if (!r.contentSerialization.empty()) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::NOT_IMPLEMENTED;
            return bad;
        }

        // Require an application ID (api) to be present for AE creation
        if (r.appID.GetLength() == 0) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return bad;
        }

        // Validate API prefix: allow 'N' and 'R', and lower-case 'r' only for RVI < 4
        char first  = r.appID.c_str()[0];
        bool api_ok = false;
        if (first == 'N' || first == 'R') api_ok = true;
        else if (first == 'r' && !(request.releaseVersionIndicator.has_value() &&
                                   request.releaseVersionIndicator->Compare("4") == 0))
            api_ok = true;
        if (!api_ok) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return bad;
        }
        // Duplicate detection: same aeID or same parent + rn -> AlreadyExists
        for (unsigned i = 0; i < db_.GetCount(); ++i) {
            const PrimitiveContent &existing = db_[i];
            if (const AE *ea = existing.GetIf<AE>()) {
                // If the request's originator equals an existing AE's aeID -> already registered
                if (request.from.GetLength() != 0 && ea->aeID.GetLength() &&
                    request.from.Compare(ea->aeID) == 0) {
                    ResponsePrimitive resp;
                    resp.responseStatusCode = ResponseStatusCode::ORIGINATOR_HAS_ALREADY_REGISTERED;
                    return resp;
                }
                // Parent/Name clash
                if (ea->parentID.GetLength() != 0 && r.parentID.GetLength() != 0) {
                    if (ea->parentID.Compare(r.parentID) == 0 &&
                        ea->resourceName.Compare(r.resourceName) == 0) {
                        ResponsePrimitive resp;
                        resp.responseStatusCode = ResponseStatusCode::CONFLICT;
                        return resp;
                    }
                }
                // aeID clash
                if (ea->aeID.GetLength() != 0 && r.aeID.GetLength() != 0 &&
                    ea->aeID.Compare(r.aeID) == 0) {
                    ResponsePrimitive resp;
                    resp.responseStatusCode = ResponseStatusCode::CONFLICT;
                    return resp;
                }
            }
        }
        pc = r;
    } else if (auto p = pc.GetIf<Group>()) {
        Group r = *p;
        assignIdAndParent(r);
        pc = r;
    } else if (auto p = pc.GetIf<Subscription>()) {
        Subscription r = *p;
        assignIdAndParent(r);
        pc = r;
    } else if (auto p = pc.GetIf<AccessControlPolicy>()) {
        AccessControlPolicy r = *p;
        assignIdAndParent(r);
        pc = r;
    } else if (auto p = pc.GetIf<CSEBase>()) {
        CSEBase r = *p;
        assignIdAndParent(r);
        pc = r;
    } else if (auto p = pc.GetIf<RemoteCSE>()) {
        RemoteCSE r = *p;
        assignIdAndParent(r);
        pc = r;
    } else if (auto p = pc.GetIf<MgmtCmd>()) {
        MgmtCmd r = *p;
        assignIdAndParent(r);
        pc = r;
    } else if (auto p = pc.GetIf<ExecInstance>()) {
        ExecInstance r = *p;
        assignIdAndParent(r);
        pc = r;
    } else if (auto p = pc.GetIf<TimeSeries>()) {
        TimeSeries r = *p;
        assignIdAndParent(r);
        pc = r;
    } else if (auto p = pc.GetIf<TimeSeriesInstance>()) {
        TimeSeriesInstance r = *p;
        assignIdAndParent(r);
        pc = r;
    } else if (auto p = pc.GetIf<Schedule>()) {
        Schedule r = *p;
        assignIdAndParent(r);
        pc = r;
    } else if (auto p = pc.GetIf<RequestResource>()) {
        RequestResource r = *p;
        assignIdAndParent(r);
        pc = r;
    } else if (auto p = pc.GetIf<PollingChannel>()) {
        PollingChannel r = *p;
        assignIdAndParent(r);
        pc = r;
    } else if (auto p = pc.GetIf<Node>()) {
        Node r = *p;
        assignIdAndParent(r);
        pc = r;
    } else if (auto p = pc.GetIf<FlexContainer>()) {
        FlexContainer r = *p;
        assignIdAndParent(r);
        pc = r;
    } else {
        resp.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
        return resp;
    }

    // Validate parent/child rules: AE may only be created under the CSE root
    if (pc.GetIf<AE>()) {
        CString cseRoot = "/";
        cseRoot += cse->resourceName;
        const boolean isCseTarget = target.Compare(cseRoot) == 0;
        if (!isCseTarget) {
            // Parent is not the CSE root: find parent resource and ensure it's not an AE
            for (unsigned i = 0; i < db_.GetCount(); ++i) {
                const PrimitiveContent &candidate = db_[i];

                // helper to compute full path of a resource
                auto fullPathOf = [&](const ResourceBase *rbase) -> CString {
                    if (!rbase) return CString();
                    CString parent = rbase->parentID;
                    CString name   = rbase->resourceName;
                    CString full;
                    if (parent.GetLength() != 0) {
                        if (parent.c_str()[parent.GetLength() - 1] == '/') {
                            full = parent;
                            full += name;
                        } else {
                            full = parent;
                            full += "/";
                            full += name;
                        }
                    } else {
                        full = "/";
                        full += name;
                    }
                    return full;
                };

                // check each concrete candidate type
                if (const auto *r = candidate.GetIf<Container>()) {
                    if (NormalizePath(fullPathOf(r)).Compare(target) == 0 ||
                        r->resourceID.Compare(target) == 0 ||
                        r->resourceName.Compare(target) == 0) {
                        // parent is container -> OK
                        break;
                    }
                }
                if (const auto *r = candidate.GetIf<ContentInstance>()) {
                    if (NormalizePath(fullPathOf(r)).Compare(target) == 0 ||
                        r->resourceID.Compare(target) == 0 ||
                        r->resourceName.Compare(target) == 0) {
                        // parent is content instance -> treat as invalid for AE
                        ResponsePrimitive bad;
                        bad.responseStatusCode =
                            static_cast<ResponseStatusCode>(4108); // INVALID_CHILD_RESOURCE_TYPE
                        return bad;
                    }
                }
                if (const auto *r = candidate.GetIf<AE>()) {
                    if (NormalizePath(fullPathOf(r)).Compare(target) == 0 ||
                        r->resourceID.Compare(target) == 0 ||
                        r->resourceName.Compare(target) == 0) {
                        // creating AE under AE -> invalid
                        ResponsePrimitive bad;
                        bad.responseStatusCode =
                            static_cast<ResponseStatusCode>(4108); // INVALID_CHILD_RESOURCE_TYPE
                        return bad;
                    }
                }
                if (const auto *r = candidate.GetIf<Group>()) {
                    if (NormalizePath(fullPathOf(r)).Compare(target) == 0 ||
                        r->resourceID.Compare(target) == 0 ||
                        r->resourceName.Compare(target) == 0) {
                        // parent is group -> OK (not typical but allow)
                        break;
                    }
                }
                if (const auto *r = candidate.GetIf<Subscription>()) {
                    if (NormalizePath(fullPathOf(r)).Compare(target) == 0 ||
                        r->resourceID.Compare(target) == 0 ||
                        r->resourceName.Compare(target) == 0) {
                        // parent is subscription -> invalid
                        ResponsePrimitive bad;
                        bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
                        return bad;
                    }
                }
            }
        }
    }

    CString createMsg;
    createMsg.Format("CREATE storing resource under parent='%s' db_before=%u",
                     request.to.c_str(),
                     db_.GetCount());
    CLogger::Get()->Write("onem2m_service", LogNotice, createMsg);
    // Store in in-memory DB
    db_.push_back(pc);
    // If we created a ContentInstance, update parent Container counters (cni, cbs)
    if (const ContentInstance *newCin = pc.GetIf<ContentInstance>()) {
        // find parent container entry and update
        for (unsigned pi = 0; pi < db_.GetCount(); ++pi) {
            PrimitiveContent &cand = db_[pi];
            if (Container *pcnt = const_cast<Container *>(cand.GetIf<Container>())) {
                if (pcnt->resourceID.Compare(newCin->parentID) == 0) {
                    Container updated = *pcnt;
                    updated.currentNrOfInstances += 1;
                    updated.currentByteSize += newCin->contentSize;
                    db_[pi] = updated;
                    break;
                }
            }
        }
    }
    CLogger::Get()->Write("onem2m_service", LogNotice, "CREATE success: resource inserted");
    resp = makeResponse(request, ResponseStatusCode::CREATED, pc);
    return resp;
}

ResponsePrimitive OneM2MService::Retrieve(const RequestPrimitive &request)
{

    ResponsePrimitive resp;

    const CSEBase *cse         = db_.GetCount() > 0 ? db_[0].GetIf<CSEBase>() : nullptr;
    bool           targetValid = false;
    bool           wrongSpid   = false;
    CString        target =
        cse ? CanonicalizeAddressingPath(
                  request.to, cse->resourceName, cse->cseID, spId_, targetValid, wrongSpid)
            : CString();
    if (!targetValid) {
        resp.responseStatusCode =
            wrongSpid ? ResponseStatusCode::NOT_FOUND : ResponseStatusCode::BAD_REQUEST;
        return resp;
    }

    CString cseRoot = "/";
    cseRoot += cse->resourceName;
    const boolean isCseTarget = target.Compare(cseRoot) == 0;
    if (isCseTarget && request.from.Compare("CAdmin") != 0) {
        resp.responseStatusCode = ResponseStatusCode::ORIGINATOR_HAS_NO_PRIVILEGE;
        return resp;
    }

    // XXX: This is a very naive implementation. When we have a real database, we can do proper
    // lookups instead of iterating over everything like this.
    // Look up by full path: parentID + '/' + resourceName, or by resourceID
    auto isAllowedForContainer = [&](const Container &cnt) -> bool {
        if (request.from.Compare("CAdmin") == 0) return true;

        CString  current = cnt.parentID;
        unsigned guard   = 0;
        while (current.GetLength() != 0 && guard++ < 32) {
            const PrimitiveContent *pc = FindByResourceId(db_, current);
            if (!pc) break;

            if (const AE *ae = pc->GetIf<AE>()) { return request.from.Compare(ae->aeID) == 0; }
            if (const Container *parentCnt = pc->GetIf<Container>()) {
                current = parentCnt->parentID;
                continue;
            }
            if (const CSEBase *cse = pc->GetIf<CSEBase>()) {
                return request.from.Compare("CAdmin") == 0 || request.from.Compare(cse->cseID) == 0;
            }
            break;
        }
        return false;
    };

    for (unsigned i = 0; i < db_.GetCount(); ++i) {
        const PrimitiveContent &pc = db_[i];
        // Debug: log candidate basic info and full path
        if (const ResourceBase *rb = GetResourceBase(pc)) {
            CString fullPath = BuildFullPath(db_, rb);
        }

        auto matchAndReturn = [&](const auto *r) -> bool {
            if (!r) return false;

            CString full = BuildFullPath(db_, r);
            CString name = r->resourceName;
            CString rid  = r->resourceID;

            // Handle container latest/oldest shortcuts: /<cnt>/la and /<cnt>/ol
            if (const Container *cnt = pc.GetIf<Container>()) {
                CString laPath = full;
                laPath += "/la";
                CString olPath = full;
                olPath += "/ol";
                if (laPath.Compare(target) == 0 || olPath.Compare(target) == 0) {
                    // authorization
                    if (!isAllowedForContainer(*cnt)) {
                        ResponsePrimitive deny;
                        deny.responseStatusCode = ResponseStatusCode::ORIGINATOR_HAS_NO_PRIVILEGE;
                        resp                    = deny;
                        return true;
                    }
                    // If this is a discovery request (rcn=ChildResourceReferences), return rrl
                    if (request.resultContent.has_value() &&
                        request.resultContent.value() == ResultContent::ChildResourceReferences) {
                        Vector<ChildResourceRef> rrl;
                        // If disr is set, discovery returns empty list
                        if (!(cnt->disableRetrieval.has_value() && *cnt->disableRetrieval)) {
                            for (unsigned ci = 0; ci < db_.GetCount(); ++ci) {
                                const PrimitiveContent &child = db_[ci];
                                if (const ContentInstance *cin = child.GetIf<ContentInstance>()) {
                                    if (cin->parentID.Compare(cnt->resourceID) == 0) {
                                        ChildResourceRef cref;
                                        cref.name = cin->resourceName;
                                        cref.type = ResourceType::ContentInstance;
                                        // value: CSE-relative URI -> build full path
                                        cref.value = BuildFullPath(db_, cin);
                                        rrl.push_back(cref);
                                    }
                                }
                            }
                        }
                        PrimitiveContent out;
                        out  = rrl;
                        resp = makeResponse(request, ResponseStatusCode::OK, out);
                        return true;
                    }
                    // disr enforcement for LA/OL
                    if (cnt->disableRetrieval.has_value() && *cnt->disableRetrieval) {
                        ResponsePrimitive deny;
                        deny.responseStatusCode = ResponseStatusCode::OPERATION_NOT_ALLOWED;
                        resp                    = deny;
                        return true;
                    }

                    // find matching CINs under this container
                    int bestIdx = -1;
                    for (int ci = 0; ci < static_cast<int>(db_.GetCount()); ++ci) {
                        const PrimitiveContent &child = db_[ci];
                        if (const ContentInstance *cin = child.GetIf<ContentInstance>()) {
                            if (cin->parentID.Compare(cnt->resourceID) == 0) {
                                if (bestIdx < 0) bestIdx = ci;
                                else {
                                    if (laPath.Compare(target) == 0) {
                                        // latest -> pick highest db index (newest)
                                        if (ci > bestIdx) bestIdx = ci;
                                    } else {
                                        // oldest -> pick lowest db index
                                        if (ci < bestIdx) bestIdx = ci;
                                    }
                                }
                            }
                        }
                    }
                    if (bestIdx < 0) {
                        resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
                        return true;
                    }
                    PrimitiveContent out;
                    out  = db_[bestIdx];
                    resp = makeResponse(request, ResponseStatusCode::OK, out);
                    return true;
                }
            }
            if (full.Compare(target) == 0 ||
                NormalizePath(full).Compare(NormalizePath(target)) == 0 ||
                rid.Compare(NormalizePath(target)) == 0 ||
                name.Compare(NormalizePath(target)) == 0) {
                CLogger::Get()->Write("onem2m_service", LogNotice, "MATCH FOUND: %s", full.c_str());
                // If this is a container and a discovery request (rcn=ChildResourceReferences),
                // return the child resource references (possibly empty when disr is set).
                if (auto cnt = pc.GetIf<Container>()) {
                    if (request.resultContent.has_value() &&
                        request.resultContent.value() == ResultContent::ChildResourceReferences) {
                        Vector<ChildResourceRef> rrl;
                        if (!(cnt->disableRetrieval.has_value() && *cnt->disableRetrieval)) {
                            for (unsigned ci = 0; ci < db_.GetCount(); ++ci) {
                                const PrimitiveContent &child = db_[ci];
                                if (const ContentInstance *cin = child.GetIf<ContentInstance>()) {
                                    if (cin->parentID.Compare(cnt->resourceID) == 0) {
                                        ChildResourceRef cref;
                                        cref.name  = cin->resourceName;
                                        cref.type  = ResourceType::ContentInstance;
                                        cref.value = BuildFullPath(db_, cin);
                                        rrl.push_back(cref);
                                    }
                                }
                            }
                        }
                        CLogger::Get()->Write(
                            "onem2m_service",
                            LogNotice,
                            "Discovery rcn=6 for parent='%s' disr=%d rrl_count=%u",
                            cnt->resourceID.c_str(),
                            (int)(cnt->disableRetrieval.has_value() && *cnt->disableRetrieval),
                            (unsigned)rrl.GetCount());
                        PrimitiveContent out;
                        out  = rrl;
                        resp = makeResponse(request, ResponseStatusCode::OK, out);
                        return true;
                    }
                }
                // If this is an AE and the originator is not the AE's aei, reject with
                // ORIGINATOR_HAS_NO_PRIVILEGE
                if (auto ae = pc.GetIf<AE>()) {
                    if (request.from.GetLength() == 0 || ae->aeID.GetLength() == 0 ||
                        request.from.Compare(ae->aeID) != 0) {
                        ResponsePrimitive deny;
                        deny.responseStatusCode = ResponseStatusCode::ORIGINATOR_HAS_NO_PRIVILEGE;
                        resp                    = deny;
                        return true;
                    }
                }
                if (auto cnt = pc.GetIf<Container>()) {
                    if (!isAllowedForContainer(*cnt)) {
                        ResponsePrimitive deny;
                        deny.responseStatusCode = ResponseStatusCode::ORIGINATOR_HAS_NO_PRIVILEGE;
                        resp                    = deny;
                        return true;
                    }
                }
                // If the matched resource is a ContentInstance, enforce parent's disableRetrieval
                if (auto cin = pc.GetIf<ContentInstance>()) {
                    if (cin->parentID.GetLength() != 0) {
                        const PrimitiveContent *parentPc = FindByResourceId(db_, cin->parentID);
                        if (parentPc) {
                            if (const Container *pcnt = parentPc->GetIf<Container>()) {
                                if (pcnt->disableRetrieval.has_value() && *pcnt->disableRetrieval) {
                                    ResponsePrimitive deny;
                                    deny.responseStatusCode =
                                        ResponseStatusCode::OPERATION_NOT_ALLOWED;
                                    resp = deny;
                                    return true;
                                }
                            }
                        }
                    }
                }
                resp = makeResponse(request, ResponseStatusCode::OK, pc);
                return true;
            }
            return false;
        };

        // XXX: This is a bit clunky, but it works for now.
        // check concrete types
        if (matchAndReturn(pc.GetIf<Container>())) return resp;
        if (matchAndReturn(pc.GetIf<ContentInstance>())) return resp;
        if (matchAndReturn(pc.GetIf<AE>())) return resp;
        if (matchAndReturn(pc.GetIf<Group>())) return resp;
        if (matchAndReturn(pc.GetIf<Subscription>())) return resp;
        if (matchAndReturn(pc.GetIf<AccessControlPolicy>())) return resp;
        if (matchAndReturn(pc.GetIf<CSEBase>())) return resp;
        if (matchAndReturn(pc.GetIf<RemoteCSE>())) return resp;
        if (matchAndReturn(pc.GetIf<MgmtCmd>())) return resp;
        if (matchAndReturn(pc.GetIf<ExecInstance>())) return resp;
        if (matchAndReturn(pc.GetIf<TimeSeries>())) return resp;
        if (matchAndReturn(pc.GetIf<TimeSeriesInstance>())) return resp;
        if (matchAndReturn(pc.GetIf<Schedule>())) return resp;
        if (matchAndReturn(pc.GetIf<RequestResource>())) return resp;
        if (matchAndReturn(pc.GetIf<PollingChannel>())) return resp;
        if (matchAndReturn(pc.GetIf<Node>())) return resp;
        if (matchAndReturn(pc.GetIf<FlexContainer>())) return resp;
    }
    CLogger::Get()->Write("onem2m_service", LogWarning, "RETRIEVE failed: resource not found");

    // Fallback: try parent/child resolution for requests like /<cnt>/rn
    CString     normTarget = target;
    const char *nt         = normTarget.c_str();
    int         len        = (int)normTarget.GetLength();
    int         lastSlash  = -1;
    for (int j = len - 1; j >= 0; --j)
        if (nt[j] == '/') {
            lastSlash = j;
            break;
        }
    if (lastSlash >= 0) {
        // split into parentPath / childName
        char *tmp = new char[len + 1];
        strcpy(tmp, nt);
        tmp[lastSlash] = '\0';
        CString parentPath(tmp);
        CString childName(tmp + lastSlash + 1);
        delete[] tmp;

        CLogger::Get()->Write("onem2m_service",
                              LogDebug,
                              "Fallback lookup parent='%s' child='%s'",
                              parentPath.c_str(),
                              childName.c_str());

        // find parent container
        CString          foundParentId;
        const Container *foundCnt = nullptr;
        for (unsigned pi = 0; pi < db_.GetCount(); ++pi) {
            const PrimitiveContent &cand = db_[pi];
            if (const Container *pcnt = cand.GetIf<Container>()) {
                CString full = BuildFullPath(db_, pcnt);
                if (NormalizePath(full).Compare(parentPath) == 0 ||
                    pcnt->resourceID.Compare(parentPath) == 0 ||
                    pcnt->resourceName.Compare(parentPath) == 0) {
                    foundParentId = pcnt->resourceID;
                    foundCnt      = pcnt;
                    break;
                }
            }
        }

        if (foundParentId.GetLength() > 0) {
            // find child CIN with matching parentID and name/ri
            for (unsigned ci = 0; ci < db_.GetCount(); ++ci) {
                const PrimitiveContent &child = db_[ci];
                if (const ContentInstance *cin = child.GetIf<ContentInstance>()) {
                    if (cin->parentID.Compare(foundParentId) == 0 &&
                        (cin->resourceName.Compare(childName) == 0 ||
                         cin->resourceID.Compare(childName) == 0)) {
                        // enforce disr and authorization
                        if (foundCnt && foundCnt->disableRetrieval.has_value() &&
                            *foundCnt->disableRetrieval) {
                            ResponsePrimitive deny;
                            deny.responseStatusCode = ResponseStatusCode::OPERATION_NOT_ALLOWED;
                            return deny;
                        }
                        if (foundCnt && !isAllowedForContainer(*foundCnt)) {
                            ResponsePrimitive deny;
                            deny.responseStatusCode =
                                ResponseStatusCode::ORIGINATOR_HAS_NO_PRIVILEGE;
                            return deny;
                        }
                        PrimitiveContent out = db_[ci];
                        resp                 = makeResponse(request, ResponseStatusCode::OK, out);
                        return resp;
                    }
                }
            }
        }
    }

    resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
    return resp;
}

ResponsePrimitive OneM2MService::Update(const RequestPrimitive &request)
{
    ResponsePrimitive resp;

    const CSEBase *cse         = db_.GetCount() > 0 ? db_[0].GetIf<CSEBase>() : nullptr;
    bool           targetValid = false;
    bool           wrongSpid   = false;
    CString        target =
        cse ? CanonicalizeAddressingPath(
                  request.to, cse->resourceName, cse->cseID, spId_, targetValid, wrongSpid)
            : CString();
    if (!targetValid) {
        resp.responseStatusCode =
            wrongSpid ? ResponseStatusCode::NOT_FOUND : ResponseStatusCode::BAD_REQUEST;
        return resp;
    }

    CString cseRoot = "/";
    cseRoot += cse->resourceName;
    if (target.Compare(cseRoot) == 0) {
        resp.responseStatusCode = ResponseStatusCode::OPERATION_NOT_ALLOWED;
        return resp;
    }

    // XXX: out of scope of this project
    resp.responseStatusCode = ResponseStatusCode::NOT_IMPLEMENTED;
    return resp;
}

ResponsePrimitive OneM2MService::Delete(const RequestPrimitive &request)
{
    ResponsePrimitive resp;

    const CSEBase *cse         = db_.GetCount() > 0 ? db_[0].GetIf<CSEBase>() : nullptr;
    bool           targetValid = false;
    bool           wrongSpid   = false;
    CString        target =
        cse ? CanonicalizeAddressingPath(
                  request.to, cse->resourceName, cse->cseID, spId_, targetValid, wrongSpid)
            : CString();
    if (!targetValid) {
        resp.responseStatusCode =
            wrongSpid ? ResponseStatusCode::NOT_FOUND : ResponseStatusCode::BAD_REQUEST;
        return resp;
    }

    CString cseRoot = "/";
    cseRoot += cse->resourceName;
    if (target.Compare(cseRoot) == 0) {
        resp.responseStatusCode = ResponseStatusCode::OPERATION_NOT_ALLOWED;
        return resp;
    }

    // XXX: out of scope of this project
    resp.responseStatusCode = ResponseStatusCode::NOT_IMPLEMENTED;
    return resp;
}

ResponsePrimitive OneM2MService::Notify(const RequestPrimitive &request)
{
    // TODO: Implement notifications for subscriptions.
    (void)request;
    ResponsePrimitive resp;
    resp.responseStatusCode = ResponseStatusCode::NOT_IMPLEMENTED;
    return resp;
}

} // namespace zerom2m::onem2m
