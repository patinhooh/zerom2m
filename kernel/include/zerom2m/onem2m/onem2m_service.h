/*
 * onem2m_service.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "zerom2m/compat/collections.h"
#include "zerom2m/onem2m/types/primitives.h"
#include "zerom2m/onem2m/types/resources.h"
#include "zerom2m/types.h"

#include <circle/types.h>

namespace zerom2m::onem2m
{

using namespace zerom2m::onem2m::types;
using namespace zerom2m::compat;

class OneM2MService
{

public:
    OneM2MService()  = default;
    ~OneM2MService() = default;

    void              Initialize();
    ResponsePrimitive HandleRequest(const RequestPrimitive &request);
    ResponsePrimitive Create(const RequestPrimitive &request);
    ResponsePrimitive Retrieve(const RequestPrimitive &request);
    ResponsePrimitive Update(const RequestPrimitive &request);
    ResponsePrimitive Delete(const RequestPrimitive &request);
    ResponsePrimitive Notify(const RequestPrimitive &request);

private:
    bool initialized_ = false;

    // TODO: Add proper way of storing data.
    // Simple in-memory DB abstraction. Stores PrimitiveContent objects representing
    // created resources. This can later be replaced with a real database backend.
    Vector<PrimitiveContent> db_;
    u32                      nextResourceId_ = 1;
};

} // namespace zerom2m::onem2m