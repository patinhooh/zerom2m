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
#include <zerom2m/onem2m/types/primitives.h>
#include <zerom2m/onem2m/types/resources.h>

#include <circle/types.h>

namespace zerom2m::onem2m
{

using namespace zerom2m::onem2m::types;
using zerom2m::compat::Vector;

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

    void              Initialize();
    ResponsePrimitive HandleRequest(const RequestPrimitive &request);
    ResponsePrimitive Create(const RequestPrimitive &request);
    ResponsePrimitive Retrieve(const RequestPrimitive &request);
    ResponsePrimitive Update(const RequestPrimitive &request);
    ResponsePrimitive Delete(const RequestPrimitive &request);
    ResponsePrimitive Notify(const RequestPrimitive &request);

private:
    OneM2MService()  = default;
    ~OneM2MService() = default;

    boolean initialized_ = false;

    // TODO: Add proper way of storing data.
    // Simple in-memory DB abstraction. Stores PrimitiveContent objects representing created
    // resources. This will later be replaced with a real database backend.
    // FIXME: We changed this to singleton so it could cause problems with concurrent access. Not
    // the end of the world as task are cooperative, and single threaded.
    Vector<PrimitiveContent> db_;
    u32                      nextResourceId_ = 1;
};

} // namespace zerom2m::onem2m