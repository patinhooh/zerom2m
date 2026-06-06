/*
 * light_example.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include <zerom2m/http/http_client.h>
#include <zerom2m/http/types.h>
#include <zerom2m/onem2m/onem2m_service.h>
#include <zerom2m/onem2m/types/resources.h>
#include <zerom2m/tasks/light_example.h>

#include <circle/actled.h>
#include <circle/logger.h>
#include <circle/sched/scheduler.h>
#include <circle/sched/task.h>

namespace zerom2m::tasks
{

using namespace zerom2m::onem2m::types;
using namespace zerom2m::http;
using zerom2m::onem2m::OneM2MService;

LightExample::LightExample(CScheduler     *scheduler,
                           NetworkManager *net,
                           HttpAdapter    *httpAdapter,
                           SystemConfig   *config,
                           CActLED        *led,
                           bool            initialState)
    : P2PNode(scheduler, net, httpAdapter)
    , scheduler_(scheduler)
    , net_(net)
    , httpAdapter_(httpAdapter)
    , config_(config)
    , led_(led)
    , state_(initialState)
{
    // Setting task name
    SetName("light_example");

    // Create AE resource for this light example
    AE ae;
    ae.resourceName = "light";
    ae.appID        = "RLightExample";
    ae.labels.Append("example");
    ae.labels.Append("light");
    ae.supportedReleaseVersions.Append("4");

    CreateAE(ae, config_->cse.cse_id);

    // Create Container for the light state
    Container cnt;
    cnt.resourceName = "state";
    cnt.labels.Append("example");
    cnt.labels.Append("state_container");
    cnt.creator = ae.appID;
    CString cntParent;
    cntParent.Format("%s/%s", config_->cse.cse_id.c_str(), ae.resourceName.c_str());

    CreateContainer(cnt, cntParent);

    // Create initial ContentInstance for the light state
    SetState(initialState);

    ContentInstance ci;
    ci.resourceName = "state_instance";
    ci.creator      = ae.appID;
    ci.labels.Append("example");
    ci.labels.Append("state_instance");
    // For simplicity, we just use a text string as content.
    // For more complex content, use SerDe logic to support structured content.
    ci.contentInfo = "text/plain:1";
    ci.content     = state_ ? "ON" : "OFF";
    CString cinParent;
    cinParent.Format("%s/%s", cntParent.c_str(), cnt.resourceName.c_str());

    CreateContentInstance(ci, cinParent);
}

LightExample::~LightExample()
{
    scheduler_ = nullptr;
    config_    = nullptr;
    led_       = nullptr;
}

void LightExample::SetState(bool on)
{
    state_ = on;

    if (led_ == nullptr) return;
    if (state_) led_->On();
    else led_->Off();
}

ResponsePrimitive LightExample::OnVerification(const Notification &sgn)
{
    (void)sgn;
    ResponsePrimitive resp;
    resp.from               = "RLightExample";
    resp.responseStatusCode = ResponseStatusCode::OK;

    CLogger::Get()->Write(
        "light_example", LogNotice, "Subscription verification request: responding OK");
    // Handle subscription verification request if needed.
    // For simplicity, we just ignore it here and respond with OK.
    return resp;
}

ResponsePrimitive LightExample::OnData(const Notification &sgn)
{
    ResponsePrimitive resp;
    resp.from = "RLightExample";

    if (!sgn.notificationEvent.has_value() || !sgn.notificationEvent->representation) {
        CLogger::Get()->Write(
            "light_example", LogWarning, "Received notification with no content, ignoring");
        resp.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
        return resp;
    }

    if (auto cin = sgn.notificationEvent->representation->GetIf<ContentInstance>()) {
        CLogger::Get()->Write("light_example",
                              LogNotice,
                              "Received notification for new ContentInstance: %s",
                              cin->content.c_str());

        // Change the light state based on the content.
        SetState(cin->content.Compare("ON") == 0);
        resp.responseStatusCode = ResponseStatusCode::OK;
        return resp;
    }

    CLogger::Get()->Write("light_example",
                          LogWarning,
                          "Received notification with unsupported content type %u, ignoring",
                          (unsigned)sgn.notificationEvent->representation->kind());

    resp.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
    return resp;
}

void LightExample::Run()
{
    scheduler_->MsSleep(1000); // Wait for the system to stabilize before starting

    // Subscribe to Switch state changes
    Subscription sub;
    sub.resourceName = "light_subscription";
    sub.creator      = "RLightExample";
    sub.eventNotificationCriteria.notificationEventType.Append(
        NotificationEventType::CreateOfDirectChildResource);
    sub.eventNotificationCriteria.childResourceType.Append(ResourceType::ContentInstance);

    CString url;
    // XXX: If you are doing this with vm without bridged networking, you might need to hardcode the
    // IP address of the host machine here instead of using net_->GetIP()
    url.Format("http://%s:%u/", net_->GetIP().c_str(), config_->http.port);
    sub.notificationURI.Append(url);
    sub.subscriberURI = url;

    CString subParent;
    subParent.Format("%s/switch/state", config_->cse.cse_id.c_str());

    // XXX: For simplicity, we hardcode the IP and port of the switch example here.
    // But you could add this to the config file and read it from there if needed.
    // For example
    // const u8 ip[4] = {127, 0, 0, 1};
    // const u16 switchPort = 80;
    const u8 ip[4]      = {0, 0, 0, 0};
    auto     switchIP   = CIPAddress(ip);
    u16      switchPort = 0;

    assert(ip[0] != 0 && "Set IP for the light example");
    assert(switchPort != 0 && "Set port for the light example");

    // Blocks until subscription is created
    SendSubscription(sub, subParent, switchIP, switchPort);

    // Run loop
    while (true) {
        scheduler_->MsSleep(1000);
        scheduler_->Yield();
        // As light does not change on its own, we don't need to do anything here. Just sleep and
        // wait for notifications. We just make sure the LED state is correct
    }
}

} // namespace zerom2m::tasks
