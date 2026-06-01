/*
 * onem2m_service.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <zerom2m/compat/vector.h>
#include <zerom2m/config/system_config.h>
#include <zerom2m/onem2m/types/primitives.h>
#include <zerom2m/onem2m/types/resources.h>
#include <zerom2m/sqlite/database.h>

#include <circle/net/netsubsystem.h>
#include <circle/types.h>

namespace zerom2m::onem2m
{

using namespace zerom2m::onem2m::types;
using zerom2m::compat::Vector;
using zerom2m::config::SystemConfig;

class OneM2MService
{
public:
    static OneM2MService &Get()
    {
        static OneM2MService instance;
        return instance;
    }

    OneM2MService(const OneM2MService &)            = delete;
    OneM2MService &operator=(const OneM2MService &) = delete;
    OneM2MService(OneM2MService &&)                 = delete;
    OneM2MService &operator=(OneM2MService &&)      = delete;

    void              Initialize(const SystemConfig &config);
    void              SetNetSubSystem(CNetSubSystem &netSubSystem);
    ResponsePrimitive HandleRequest(const RequestPrimitive &request);

    ResponsePrimitive Create(const RequestPrimitive &request);
    ResponsePrimitive CreateAE(const AE &ae, const RequestPrimitive &req, const CString &target);
    ResponsePrimitive CreateContainer(const Container &con, const RequestPrimitive &req, const CString &target);
    ResponsePrimitive CreateContentInstance(const ContentInstance &cin, const RequestPrimitive &req, const CString &target);
    ResponsePrimitive CreateSubscription(const Subscription &sub, const RequestPrimitive &req, const CString &target);

    ResponsePrimitive Retrieve(const RequestPrimitive &request);
    ResponsePrimitive RetrieveCSE(const RequestPrimitive &request,
                                  const CSEBase       &cse,
                                  const CString       &target);
    ResponsePrimitive RetrieveAE(const RequestPrimitive &request,
                                 const AE            &ae,
                                 const CString       &target);
    ResponsePrimitive RetrieveContainer(const RequestPrimitive &request,
                                       const Container   &con,
                                       const CString     &target);
    ResponsePrimitive RetrieveContentInstance(const RequestPrimitive &request,
                                              const ContentInstance &cin,
                                              const CString         &target);
    ResponsePrimitive RetrieveSubscription(const RequestPrimitive &request,
                                            const Subscription    &sub,
                                            const CString         &target);

    // XXX: Not Implemented.
    ResponsePrimitive Update(const RequestPrimitive &request);
    ResponsePrimitive Delete(const RequestPrimitive &request);

private:
    OneM2MService()  = default;
    ~OneM2MService() = default;

    CNetSubSystem *netSubSystem_{nullptr};

    boolean initialized_    = false;
    u64     nextResourceId_ = 1;

    CString                   spId_;
    zerom2m::sqlite::Database db_;

    CString GetId();
};

} // namespace zerom2m::onem2m