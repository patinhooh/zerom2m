/*
 * p2p_node.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <zerom2m/http/http_client.h>
#include <zerom2m/http/types.h>
#include <zerom2m/kernel/network_manager.h>
#include <zerom2m/onem2m/bindings/http/http_adapter.h>
#include <zerom2m/onem2m/onem2m_service.h>
#include <zerom2m/onem2m/types/resources.h>

#include <circle/sched/scheduler.h>
#include <circle/sched/task.h>

namespace zerom2m::tasks
{

using namespace zerom2m::onem2m::types;
using namespace zerom2m::http;
using zerom2m::kernel::NetworkManager;
using zerom2m::onem2m::INotificationHandler;
using zerom2m::onem2m::OneM2MService;
using zerom2m::onem2m::bindings::http::HttpAdapter;

class P2PNode : public CTask, public INotificationHandler
{
public:
    P2PNode(CScheduler *scheduler, NetworkManager *net, HttpAdapter *httpAdapter);

    virtual ~P2PNode();

    /**
     * @brief Handles incoming notifications for this node's subscriptions.
     *
     * Override this method to handle incoming notifications. By default, this will route the
     * notification to either OnData or OnVerification based on the content of the notification.
     */
    virtual ResponsePrimitive OnNotification(const Notification &sgn);

    /**
     * @brief Main loop of the P2P node task.
     */
    virtual void Run() = 0;

    bool isInitialized() const { return initialized_; }

protected:
    // Sub class hooks

    /**
     * @brief Handle incoming content notifications.
     *
     * Called when a new notification arrives for this node's subscriptions.
     *
     * @param sgn The notification structure.
     * @return ResponsePrimitive indicating the result of processing the notification.
     */
    virtual ResponsePrimitive OnData(const Notification &sgn) = 0;

    /**
     * @brief Handle subscription verification requests.
     *
     * Called when a subscription verification request arrives for this node's subscriptions.
     * By default, this returns a simple OK response. Override this method to implement custom
     * verification logic if needed.
     *
     * @param sgn The notification structure containing the verification request.
     * @return ResponsePrimitive indicating the result of the verification.
     */
    virtual ResponsePrimitive OnVerification(const Notification &sgn) = 0;

    // Helper methods

    /** for sending requests
     * @brief Create an AE resource in this node.
     *
     * @param ae The AE resource to create.
     * @param parentPath The path on the base where the AE should be created.
     */
    void CreateAE(AE ae, CString parentPath);

    /**
     * @brief Create a Container resource in this node.
     *
     * @param cnt The Container resource to create.
     * @param parentPath The path on the base where the Container should be created.
     */
    void CreateContainer(Container cnt, CString parentPath);

    /**
     * @brief Create a ContentInstance resource in this node.
     *
     * @param ci The ContentInstance resource to create.
     * @param parentPath The path on the base where the ContentInstance should be created.
     */
    void CreateContentInstance(ContentInstance ci, CString parentPath);

    /**
     * @brief Send a subscription request to other OneM2M nodes.
     *
     * This should be run inside the Run() loop after initialization.
     * Will block till the subscription is successfully created.
     *
     * @param sub The subscription resource to create on the remote node.
     * @param parentPath The path on the remote node where the subscription should be created.
     * @param remoteAddr The IP address of the remote node.
     * @param remotePort The port of the remote node.
     */
    void
    SendSubscription(Subscription sub, CString parentPath, CIPAddress remoteAddr, u16 remotePort);

    // Accessors

    CScheduler     *Scheduler() { return scheduler_; }
    NetworkManager *Net() { return net_; }

private:
    CScheduler     *scheduler_;
    NetworkManager *net_;
    HttpAdapter    *httpAdapter_;

    bool initialized_ = false;
};

} // namespace zerom2m::tasks
