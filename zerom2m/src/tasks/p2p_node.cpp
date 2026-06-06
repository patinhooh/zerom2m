/*
 * p2p_node.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include <zerom2m/tasks/p2p_node.h>

#include <circle/logger.h>

#include <assert.h>

namespace zerom2m::tasks
{

namespace
{
const char FromP2PNode[] = "p2pnode";
}

P2PNode::P2PNode(CScheduler *scheduler, NetworkManager *net, HttpAdapter *httpAdapter)
    : CTask()
    , scheduler_(scheduler)
    , net_(net)
    , httpAdapter_(httpAdapter)
{
    while (!OneM2MService::Get().IsInitialized()) {
        CLogger::Get()->Write(
            FromP2PNode, LogWarning, "OneM2MService not initialized yet, retrying in 1s");
        scheduler_->MsSleep(1000);
    }
    CLogger::Get()->Write(FromP2PNode, LogNotice, "OneM2MService is initialized, starting setup");
    initialized_ = true;
}

P2PNode::~P2PNode() { scheduler_ = nullptr; }

void P2PNode::CreateAE(AE ae, CString parentPath)
{
    // Required for registration
    assert(ae.appID.GetLength() > 0);
    assert(ae.resourceName.GetLength() > 0);

    if (ae.supportedReleaseVersions.empty()) { ae.supportedReleaseVersions.Append("4"); }

    RequestPrimitive req;
    req.op                = Operation::Create;
    req.to                = parentPath;
    req.from              = ae.appID;
    req.requestIdentifier = "req_ae_create";
    req.resourceType      = ResourceType::AE;
    req.content           = ae;

    auto resp = OneM2MService::Get().Create(req);

    // We expect to the AE to exists later, so we dont get this rsc we halt. So you can debug
    // registration issues.
    assert(resp.responseStatusCode == ResponseStatusCode::CREATED ||
           resp.responseStatusCode == ResponseStatusCode::CONFLICT);
}

void P2PNode::CreateContainer(Container cnt, CString parentPath)
{
    // Required for registration
    assert(cnt.resourceName.GetLength() > 0);
    assert(cnt.creator.has_value() && cnt.creator.value().GetLength() > 0);

    RequestPrimitive req;
    req.op                = Operation::Create;
    req.to                = parentPath;
    req.from              = cnt.creator.value();
    req.requestIdentifier = "req_cnt_create";
    req.resourceType      = ResourceType::ContentInstance;
    req.content           = cnt;

    // We expect to the Container to exists later, so we dont get this rsc we halt. So you can debug
    // registration issues.
    auto resp = OneM2MService::Get().Create(req);
    assert(resp.responseStatusCode == ResponseStatusCode::CREATED ||
           resp.responseStatusCode == ResponseStatusCode::CONFLICT);
}

void P2PNode::CreateContentInstance(ContentInstance ci, CString parentPath)
{
    // Required for registration
    assert(ci.resourceName.GetLength() > 0);
    assert(ci.creator.has_value() && ci.creator.value().GetLength() > 0);

    RequestPrimitive req;
    req.op                = Operation::Create;
    req.to                = parentPath;
    req.from              = ci.creator.value();
    req.requestIdentifier = "req_ci_create";
    req.resourceType      = ResourceType::ContentInstance;
    req.content           = ci;

    OneM2MService::Get().Create(req);
}

ResponsePrimitive P2PNode::OnNotification(const Notification &sgn)
{
    ResponsePrimitive resp;
    resp.responseStatusCode = ResponseStatusCode::OK;

    if (sgn.verificationRequest.has_value() && sgn.verificationRequest.value()) {
        CLogger::Get()->Write(FromP2PNode, LogNotice, "Received subscription verification request");
        return OnVerification(sgn);
    }
    return OnData(sgn);
}

void P2PNode::SendSubscription(Subscription sub,
                               CString      parentPath,
                               CIPAddress   remoteAddr,
                               u16          remotePort)
{
    // Required for registration
    assert(sub.resourceName.GetLength() > 0);
    assert(sub.creator.has_value() && sub.creator.value().GetLength() > 0);

    RequestPrimitive prim;
    prim.op                = Operation::Create;
    prim.to                = parentPath;
    prim.from              = sub.creator.value();
    prim.requestIdentifier = "req_sub";
    prim.resourceType      = ResourceType::Subscription;
    prim.content           = sub;

    HttpRequest httpReq = httpAdapter_->encodeRequest(prim);

    auto client = HttpClient(&net_->GetNetSubSystem(), remoteAddr, remotePort, SERVER_NAME, 1);

    bool done = false;
    while (!done) {

        HttpResponse resp;
        client.Request(httpReq, resp);

        if (resp.Status == ResponseStatus::OK || resp.Status == ResponseStatus::Created ||
            resp.Status == ResponseStatus::Conflict) {
            CLogger::Get()->Write(
                FromP2PNode, LogNotice, "Subscription created: %u", (unsigned)resp.Status);
            done = true;

        } else {
            CLogger::Get()->Write(FromP2PNode,
                                  LogWarning,
                                  "Subscription failed (%u), retrying...",
                                  (unsigned)resp.Status);
        }
        scheduler_->MsSleep(5000);
        scheduler_->Yield();
    }
}

} // namespace zerom2m::tasks
