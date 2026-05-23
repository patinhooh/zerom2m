/*
 * onem2m_service.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "zerom2m/onem2m/onem2m_service.h"
#include "zerom2m/http/http_common.h"
#include "zerom2m/types.h"

#include "zerom2m/compat/collections.h"
#include "zerom2m/onem2m/types/primitives.h"

#include <circle/logger.h>
#include <circle/util.h>

namespace zerom2m::onem2m
{

using namespace zerom2m::onem2m::types;

void OneM2MService::Initialize()
{
    CLogger::Get()->Write("onem2m_service", LogNotice, "Initialize() called");

    if (initialized_) return;

    db_.clear();
    nextResourceId_ = 1;

    CSEBase cse;
    // TODO: Set more fields here. For now we just set some basic info
    cse.resourceType = ResourceType::CSEBase;
    cse.resourceName = "m2m";
    cse.resourceID   = "/m2m";
    cse.parentID     = "";

    cse.creationTime     = "20240101T000000";
    cse.lastModifiedTime = cse.creationTime;

    cse.cseType = CSEType::IN_CSE;
    cse.cseID   = "/m2m";

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
        res.parentID = request.to;
    };

    // TODO: This is a bit clunky, but it works for now. We can later add a more elegant way of
    // handling this

    // Try known resource types and assign ids/parent
    if (auto p = pc.GetIf<Container>()) {
        Container r = *p;
        assignIdAndParent(r);
        pc = r;
    } else if (auto p = pc.GetIf<ContentInstance>()) {
        ContentInstance r = *p;
        assignIdAndParent(r);
        pc = r;
    } else if (auto p = pc.GetIf<AE>()) {
        AE r = *p;
        assignIdAndParent(r);
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

    // XXX: This is a very naive implementation. When we have a real database, we can do proper
    // lookups instead of iterating over everything like this.
    // Look up by full path: parentID + '/' + resourceName, or by resourceID
    for (unsigned i = 0; i < db_.GetCount(); ++i) {
        const PrimitiveContent &pc = db_[i];

        auto matchAndReturn = [&](const auto *r) -> bool {
            if (!r) return false;

            CString parent = r->parentID;
            CString name   = r->resourceName;
            CString rid    = r->resourceID;

            CString full;

            if (parent.GetLength() != 0) {
                // parent may already end with '/'
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

            if (full.Compare(request.to) == 0 || rid.Compare(request.to) == 0 ||
                name.Compare(request.to) == 0) {
                CLogger::Get()->Write("onem2m_service", LogNotice, "MATCH FOUND: %s", full.c_str());
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
    // XXX: out of scope of this project
    (void)request;
    ResponsePrimitive resp;
    resp.responseStatusCode = ResponseStatusCode::NotImplemented;
    return resp;
}

ResponsePrimitive OneM2MService::Delete(const RequestPrimitive &request)
{
    // XXX: out of scope of this project
    (void)request;
    ResponsePrimitive resp;
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
