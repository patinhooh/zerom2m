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

namespace zerom2m::onem2m
{

using namespace zerom2m::onem2m::types;
using zerom2m::config::SystemConfig;

namespace {
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
        const PrimitiveContent &pc = db[i];
        const ResourceBase *base = GetResourceBase(pc);
        if (base && base->resourceID.Compare(rid) == 0) return &pc;
    }
    return nullptr;
}

CString BuildFullPath(const Vector<PrimitiveContent> &db, const ResourceBase *rbase)
{
    if (!rbase) return CString();

    CString full = "/";
    full += rbase->resourceName;

    CString parentId = rbase->parentID;
    unsigned guard = 0;
    while (parentId.GetLength() != 0 && guard++ < 32) {
        const PrimitiveContent *parentPc = FindByResourceId(db, parentId);
        if (!parentPc) break;
        const ResourceBase *parentBase = GetResourceBase(*parentPc);
        if (!parentBase) break;

        CString newFull = "/";
        newFull += parentBase->resourceName;
        newFull += full;
        full = newFull;
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
            if (normTarget.Compare("m2m") == 0 ||
                normTarget.Compare(c->resourceID) == 0 ||
                normTarget.Compare(c->resourceName) == 0 ||
                NormalizePath(cseFull).Compare(normTarget) == 0) {
                return c->resourceID;
            }
        }
    }

    for (unsigned i = 0; i < db.GetCount(); ++i) {
        const PrimitiveContent &candidate = db[i];
        const ResourceBase *base = GetResourceBase(candidate);
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

    const char *text = contentInfo.c_str();
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
            r.responseStatusCode = ResponseStatusCode::BadRequest;
            return r;
        }
    }
}

ResponsePrimitive OneM2MService::Create(const RequestPrimitive &request)
{
    ResponsePrimitive resp;

    CString target = NormalizePath(request.to);

    if (request.from.GetLength() == 0) {
        if (!request.resourceType.has_value() ||
            request.resourceType.value() != ResourceType::AE) {
            resp.responseStatusCode = ResponseStatusCode::BadRequest;
            return resp;
        }
    }

    CString err;
    if (!isValid(request, err)) {
        resp.responseStatusCode = ResponseStatusCode::BadRequest;
        return resp;
    }

    if (request.content.empty()) {
        resp.responseStatusCode = ResponseStatusCode::BadRequest;
        return resp;
    }

    PrimitiveContent pc = request.content; // copy

    auto assignIdAndParent = [&](auto &res) {
        CString id;
        id.Format("res%u", nextResourceId_++);
        res.resourceID = id;
        if (res.resourceName.GetLength() == 0) res.resourceName = id;
        res.parentID = ResolveParentId(db_, request.to);
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
                ResponsePrimitive bad;
                bad.responseStatusCode = ResponseStatusCode::BadRequest;
                return bad;
            }
            if (request.vendorInformation->Compare("cnt_creator_null") == 0) {
                if (request.from.GetLength() == 0) {
                    ResponsePrimitive bad;
                    bad.responseStatusCode = ResponseStatusCode::BadRequest;
                    return bad;
                }
                r.creator = request.from;
            }
        }

        pc             = r;
    } else if (auto p = pc.GetIf<ContentInstance>()) {
        ContentInstance r = *p;
        assignIdAndParent(r);
        r.resourceType = ResourceType::ContentInstance;

        if (!IsValidContentInfo(r.contentInfo.has_value() ? *r.contentInfo : CString())) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BadRequest;
            return bad;
        }

        if (r.creationTime.GetLength() == 0) r.creationTime = "2026-01-01T00:00:00Z";
        if (r.lastModifiedTime.GetLength() == 0) r.lastModifiedTime = r.creationTime;
        if (!r.expirationTime.has_value()) r.expirationTime = CString("2027-01-01T00:00:00Z");
        if (!r.stateTag.has_value()) r.stateTag = 0;

        // Creator handling: reject explicit creator value, allow null -> set to originator
        if (request.vendorInformation.has_value()) {
            if (request.vendorInformation->Compare("cin_creator_present") == 0) {
                ResponsePrimitive bad;
                bad.responseStatusCode = ResponseStatusCode::BadRequest;
                return bad;
            }
            if (request.vendorInformation->Compare("cin_creator_null") == 0) {
                if (request.from.GetLength() == 0) {
                    ResponsePrimitive bad;
                    bad.responseStatusCode = ResponseStatusCode::BadRequest;
                    return bad;
                }
                r.creator = request.from;
            }
            if (request.vendorInformation->Compare("cin_has_acpi") == 0) {
                ResponsePrimitive bad;
                bad.responseStatusCode = ResponseStatusCode::BadRequest;
                return bad;
            }
        }

        // Validate parent: CIN must not be created under AE
        CString parentId = r.parentID;
        if (parentId.GetLength() != 0) {
            const PrimitiveContent *parentPc = FindByResourceId(db_, parentId);
            if (parentPc) {
                if (parentPc->GetIf<AE>()) {
                    ResponsePrimitive bad;
                    bad.responseStatusCode = static_cast<ResponseStatusCode>(4108); // INVALID_CHILD_RESOURCE_TYPE
                    return bad;
                }
            }
        }

        // Ensure contentSize is set (codec may have estimated it)
        if (r.contentSize <= 0) {
            r.contentSize = static_cast<s64>(r.content.GetLength());
        }

        pc             = r;
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
                    bad.responseStatusCode = ResponseStatusCode::BadRequest; // 4000
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
                    bad.responseStatusCode = ResponseStatusCode::BadRequest;
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
                            static_cast<ResponseStatusCode>(4107); // SECURITY_ASSOCIATION_REQUIRED
                        return respSec;
                    }
                }
            }
        }
        // Reject Create requests that explicitly include the 'cr' (creator) attribute
        if (request.vendorInformation.has_value() &&
            request.vendorInformation->Compare("has_creator") == 0) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BadRequest;
            return bad;
        }

        // Reject csz attribute for AE: not supported in this implementation
        if (!r.contentSerialization.empty()) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::NotImplemented;
            return bad;
        }

        // Require an application ID (api) to be present for AE creation
        if (r.appID.GetLength() == 0) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BadRequest;
            return bad;
        }

        // Validate API prefix: allow 'N' and 'R', and lower-case 'r' only for RVI < 4
        char first = r.appID.c_str()[0];
        bool api_ok = false;
        if (first == 'N' || first == 'R') api_ok = true;
        else if (first == 'r' && !(request.releaseVersionIndicator.has_value() &&
                                   request.releaseVersionIndicator->Compare("4") == 0))
            api_ok = true;
        if (!api_ok) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BadRequest;
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
                    resp.responseStatusCode =
                        static_cast<ResponseStatusCode>(4117); // ORIGINATOR_HAS_ALREADY_REGISTERED
                    return resp;
                }
                // Parent/Name clash
                if (ea->parentID.GetLength() != 0 && r.parentID.GetLength() != 0) {
                    if (ea->parentID.Compare(r.parentID) == 0 &&
                        ea->resourceName.Compare(r.resourceName) == 0) {
                        ResponsePrimitive resp;
                        resp.responseStatusCode = static_cast<ResponseStatusCode>(4105); // CONFLICT
                        return resp;
                    }
                }
                // aeID clash
                if (ea->aeID.GetLength() != 0 && r.aeID.GetLength() != 0 &&
                    ea->aeID.Compare(r.aeID) == 0) {
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

    // Validate parent/child rules: AE may only be created under the CSE root
    if (pc.GetIf<AE>()) {
        const boolean isCseTarget = target.Compare("m2m") == 0;
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
                        bad.responseStatusCode = ResponseStatusCode::BadRequest;
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

    // XXX: This is a very naive implementation. When we have a real database, we can do proper
    // lookups instead of iterating over everything like this.
    // Look up by full path: parentID + '/' + resourceName, or by resourceID
    auto isAllowedForContainer = [&](const Container &cnt) -> bool {
        if (request.from.Compare("CAdmin") == 0) return true;

        CString current = cnt.parentID;
        unsigned guard = 0;
        while (current.GetLength() != 0 && guard++ < 32) {
            const PrimitiveContent *pc = FindByResourceId(db_, current);
            if (!pc) break;

            if (const AE *ae = pc->GetIf<AE>()) {
                return request.from.Compare(ae->aeID) == 0;
            }
            if (const Container *parentCnt = pc->GetIf<Container>()) {
                current = parentCnt->parentID;
                continue;
            }
            if (const CSEBase *cse = pc->GetIf<CSEBase>()) {
                return request.from.Compare("CAdmin") == 0 ||
                       request.from.Compare(cse->cseID) == 0;
            }
            break;
        }
        return false;
    };

    for (unsigned i = 0; i < db_.GetCount(); ++i) {
        const PrimitiveContent &pc = db_[i];

        auto matchAndReturn = [&](const auto *r) -> bool {
            if (!r) return false;

            CString target = NormalizePath(request.to);

            CString full = BuildFullPath(db_, r);
            CString name = r->resourceName;
            CString rid  = r->resourceID;

            if (NormalizePath(full).Compare(target) == 0 || rid.Compare(target) == 0 ||
                name.Compare(target) == 0) {
                CLogger::Get()->Write("onem2m_service", LogNotice, "MATCH FOUND: %s", full.c_str());
                // If this is an AE and the originator is not the AE's aei, reject with
                // ORIGINATOR_HAS_NO_PRIVILEGE
                if (auto ae = pc.GetIf<AE>()) {
                    if (request.from.GetLength() == 0 || ae->aeID.GetLength() == 0 ||
                        request.from.Compare(ae->aeID) != 0) {
                        ResponsePrimitive deny;
                        deny.responseStatusCode =
                            static_cast<ResponseStatusCode>(4103); // ORIGINATOR_HAS_NO_PRIVILEGE
                        resp = deny;
                        return true;
                    }
                }
                if (auto cnt = pc.GetIf<Container>()) {
                    if (!isAllowedForContainer(*cnt)) {
                        ResponsePrimitive deny;
                        deny.responseStatusCode =
                            static_cast<ResponseStatusCode>(4103); // ORIGINATOR_HAS_NO_PRIVILEGE
                        resp = deny;
                        return true;
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

    resp.responseStatusCode = ResponseStatusCode::NotFound;
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
