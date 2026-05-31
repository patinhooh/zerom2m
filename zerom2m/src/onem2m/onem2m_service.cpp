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
#include <zerom2m/kernel/paths.h>
#include <zerom2m/onem2m/onem2m_service.h>
#include <zerom2m/onem2m/types/primitives.h>

#include <circle/logger.h>
#include <circle/util.h>

namespace zerom2m::onem2m
{

using namespace zerom2m::onem2m::types;
using zerom2m::config::SystemConfig;

namespace
{
CString NormalizePath(const CString &path)
{
    if (path.GetLength() > 0 && path.c_str()[0] == '/') return CString(path.c_str() + 1);
    return path;
}
} // namespace

void OneM2MService::Initialize(const SystemConfig &config)
{
    CLogger::Get()->Write("onem2m_service", LogNotice, "Initialize() called");

    if (initialized_) return;

    // Open (or create) the SQLite database on the FAT32 volume.
    // The path uses the FatFs drive prefix set up in kernel.cpp.
    CString dbErr;
    if (!db_.Open(DB_PATH, dbErr)) {
        CLogger::Get()->Write("onem2m_service", LogError, "DB open failed: %s", dbErr.c_str());
        return;
    }

    if (!db_.InitSchema()) {
        CLogger::Get()->Write("onem2m_service", LogError, "DB schema init failed");
        return;
    }

    // If a CSEBase already exists in the DB we are resuming from persistent
    // storage — skip seeding.
    {
        CSEBase existing;
        CString lookupErr;
        if (db_.GetCSEBase(existing, lookupErr)) {
            CLogger::Get()->Write("onem2m_service",
                                  LogNotice,
                                  "CSEBase already in DB (ri='%s'), skipping seed",
                                  existing.resourceID.c_str());
            initialized_ = true;
            return;
        }
    }

    nextResourceId_ = 1;

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

    CString saveErr;
    if (!db_.SavePrimitiveContent(pc, saveErr)) {
        CLogger::Get()->Write(
            "onem2m_service", LogError, "Failed to save CSEBase: %s", saveErr.c_str());
        return;
    }

    CLogger::Get()->Write("onem2m_service",
                          LogNotice,
                          "Inserted CSEBase: rn='%s' ri='%s' pi='%s'",
                          cse.resourceName.c_str(),
                          cse.resourceID.c_str(),
                          cse.parentID.c_str());

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
            r.responseStatusCode = ResponseStatusCode::BadRequest;
            return r;
        }
    }
}

ResponsePrimitive OneM2MService::Create(const RequestPrimitive &request)
{
    ResponsePrimitive resp;

    CString target = NormalizePath(request.to);

    CString err;
    if (!isValid(request, err)) {
        resp.responseStatusCode = ResponseStatusCode::BadRequest;
        return resp;
    }

    if (request.content.empty()) {
        resp.responseStatusCode = ResponseStatusCode::BadRequest;
        return resp;
    }

    PrimitiveContent pc = request.content;

    auto assignIdAndParent = [&](auto &res) {
        CString id;
        id.Format("res%u", nextResourceId_++);
        res.resourceID = id;
        if (res.resourceName.GetLength() == 0) res.resourceName = id;
        if (request.to.GetLength() > 0 && request.to.c_str()[0] == '/')
            res.parentID = CString(request.to.c_str() + 1);
        else res.parentID = request.to;
    };

    if (auto p = pc.GetIf<Container>()) {
        Container r = *p;
        assignIdAndParent(r);
        r.resourceType = ResourceType::Container;
        pc             = r;
    } else if (auto p = pc.GetIf<ContentInstance>()) {
        ContentInstance r = *p;
        assignIdAndParent(r);
        r.resourceType = ResourceType::ContentInstance;
        pc             = r;
    } else if (auto p = pc.GetIf<AE>()) {
        AE r = *p;
        assignIdAndParent(r);
        r.resourceType = ResourceType::AE;

        if (r.aeID.GetLength() == 0) {
            CString aid;
            aid.Format("C%s", r.resourceID.c_str());
            r.aeID = aid;
        }

        // Validate resourceName characters: allow alnum and -._~ only
        if (r.resourceName.GetLength() != 0) {
            for (size_t ci = 0; ci < r.resourceName.GetLength(); ++ci) {
                char c = r.resourceName.c_str()[ci];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                      c == '-' || c == '.' || c == '_' || c == '~')) {
                    ResponsePrimitive bad;
                    bad.responseStatusCode = ResponseStatusCode::BadRequest;
                    return bad;
                }
            }
        }

        if (r.creationTime.GetLength() == 0) r.creationTime = "2026-01-01T00:00:00Z";
        if (r.lastModifiedTime.GetLength() == 0) r.lastModifiedTime = r.creationTime;
        if (!r.expirationTime.has_value()) r.expirationTime = CString("2027-01-01T00:00:00Z");

        // Validate API prefix for RVI 4
        if (request.releaseVersionIndicator.has_value() &&
            request.releaseVersionIndicator->Compare("4") == 0) {
            if (r.appID.GetLength() != 0) {
                char fc = r.appID.c_str()[0];
                if (fc >= 'a' && fc <= 'z') {
                    ResponsePrimitive bad;
                    bad.responseStatusCode = ResponseStatusCode::BadRequest;
                    return bad;
                }
            }
        }

        // Reject if originator equals the CSE ID (security association required)
        if (request.from.GetLength() != 0) {
            CSEBase cse;
            CString cseErr;
            if (db_.GetCSEBase(cse, cseErr)) {
                CString cseid = cse.cseID;
                if (cseid.GetLength() > 0 && cseid.c_str()[0] == '/')
                    cseid = CString(cseid.c_str() + 1);
                if (request.from.Compare(cseid) == 0) {
                    ResponsePrimitive respSec;
                    respSec.responseStatusCode =
                        static_cast<ResponseStatusCode>(4107); // SECURITY_ASSOCIATION_REQUIRED
                    return respSec;
                }
            }
        }

        // Reject 'creator' attribute
        if (request.vendorInformation.has_value() &&
            request.vendorInformation->Compare("has_creator") == 0) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BadRequest;
            return bad;
        }

        // Reject csz (contentSerialization)
        if (!r.contentSerialization.empty()) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::NotImplemented;
            return bad;
        }

        // Require appID (api)
        if (r.appID.GetLength() == 0) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BadRequest;
            return bad;
        }

        // Validate API prefix: allow 'N', 'R', and 'r' only for RVI < 4
        char first  = r.appID.c_str()[0];
        bool api_ok = (first == 'N' || first == 'R');
        if (!api_ok && first == 'r' &&
            !(request.releaseVersionIndicator.has_value() &&
              request.releaseVersionIndicator->Compare("4") == 0))
            api_ok = true;
        if (!api_ok) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BadRequest;
            return bad;
        }

        // Duplicate detection via DB
        CString dupErr;
        if (db_.ExistsAEByAEID(r.aeID, dupErr)) {
            ResponsePrimitive resp;
            resp.responseStatusCode =
                static_cast<ResponseStatusCode>(4117); // ORIGINATOR_HAS_ALREADY_REGISTERED
            return resp;
        }

        // Parent/Name clash: check if another AE already has same pi + rn
        {
            // We load by the full path pi/rn; if found it's a conflict
            CString fullPath;
            fullPath = r.parentID;
            fullPath += "/";
            fullPath += r.resourceName;
            PrimitiveContent existing;
            CString          loadErr;
            if (db_.LoadPrimitiveContentByTarget(NormalizePath(fullPath), existing, loadErr)) {
                if (existing.GetIf<AE>()) {
                    ResponsePrimitive resp;
                    resp.responseStatusCode = static_cast<ResponseStatusCode>(4105); // CONFLICT
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
        resp.responseStatusCode = ResponseStatusCode::Unsupported;
        return resp;
    }

    // Validate parent/child rules for AE: may only be created under the CSE root
    if (pc.GetIf<AE>()) {
        const boolean isCseTarget = target.Compare("m2m") == 0;
        if (!isCseTarget) {
            // Find parent resource in DB and check its type
            PrimitiveContent parentPc;
            CString          parentErr;
            if (db_.LoadPrimitiveContentByTarget(target, parentPc, parentErr)) {
                if (parentPc.GetIf<ContentInstance>()) {
                    ResponsePrimitive bad;
                    bad.responseStatusCode = static_cast<ResponseStatusCode>(4108);
                    return bad;
                }
                if (parentPc.GetIf<AE>()) {
                    ResponsePrimitive bad;
                    bad.responseStatusCode = static_cast<ResponseStatusCode>(4108);
                    return bad;
                }
                if (parentPc.GetIf<Subscription>()) {
                    ResponsePrimitive bad;
                    bad.responseStatusCode = ResponseStatusCode::BadRequest;
                    return bad;
                }
            }
        }
    }

    CLogger::Get()->Write("onem2m_service",
                          LogNotice,
                          "CREATE storing resource under parent='%s'",
                          request.to.c_str());

    CString saveErr;
    if (!db_.SavePrimitiveContent(pc, saveErr)) {
        CLogger::Get()->Write(
            "onem2m_service", LogError, "CREATE DB save failed: %s", saveErr.c_str());
        resp.responseStatusCode = ResponseStatusCode::InternalServerError;
        return resp;
    }

    CLogger::Get()->Write("onem2m_service", LogNotice, "CREATE success: resource inserted");
    resp = makeResponse(request, ResponseStatusCode::Created, pc);
    return resp;
}

ResponsePrimitive OneM2MService::Retrieve(const RequestPrimitive &request)
{
    ResponsePrimitive resp;

    const boolean isCseTarget = request.to.Compare("/m2m") == 0 || request.to.Compare("m2m") == 0;

    if (isCseTarget && request.from.Compare("CAdmin") != 0) {
        resp.responseStatusCode = ResponseStatusCode::LinkedSubscriptionNotExist;
        return resp;
    }

    CString          target = NormalizePath(request.to);
    PrimitiveContent pc;
    CString          err;

    if (!db_.LoadPrimitiveContentByTarget(target, pc, err)) {
        CLogger::Get()->Write("onem2m_service", LogWarning, "RETRIEVE failed: %s", err.c_str());
        resp.responseStatusCode = ResponseStatusCode::NotFound;
        return resp;
    }

    // If this is an AE, the originator must match the AE's aeID
    if (const AE *ae = pc.GetIf<AE>()) {
        if (request.from.GetLength() == 0 || ae->aeID.GetLength() == 0 ||
            request.from.Compare(ae->aeID) != 0) {
            resp.responseStatusCode =
                static_cast<ResponseStatusCode>(4103); // ORIGINATOR_HAS_NO_PRIVILEGE
            return resp;
        }
    }

    CLogger::Get()->Write(
        "onem2m_service", LogNotice, "RETRIEVE found resource for target='%s'", target.c_str());
    resp = makeResponse(request, ResponseStatusCode::OK, pc);
    return resp;
}

ResponsePrimitive OneM2MService::Update(const RequestPrimitive &request)
{
    ResponsePrimitive resp;

    if (request.to.Compare("/m2m") == 0 || request.to.Compare("m2m") == 0) {
        resp.responseStatusCode = ResponseStatusCode::OperationNotAllowed;
        return resp;
    }

    // XXX: out of scope of this project
    resp.responseStatusCode = ResponseStatusCode::NotImplemented;
    return resp;
}

ResponsePrimitive OneM2MService::Delete(const RequestPrimitive &request)
{
    ResponsePrimitive resp;

    if (request.to.Compare("/m2m") == 0 || request.to.Compare("m2m") == 0) {
        resp.responseStatusCode = ResponseStatusCode::OperationNotAllowed;
        return resp;
    }

    // XXX: out of scope of this project
    resp.responseStatusCode = ResponseStatusCode::NotImplemented;
    return resp;
}

ResponsePrimitive OneM2MService::Notify(const RequestPrimitive &request)
{
    // TODO: Implement notifications for subscriptions.
    (void)request;
    ResponsePrimitive resp;
    resp.responseStatusCode = ResponseStatusCode::NotImplemented;
    return resp;
}

} // namespace zerom2m::onem2m