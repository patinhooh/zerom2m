
/*
 * http_handler.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <zerom2m/onem2m/types/primitives.h>
#include <circle/net/netsubsystem.h>

namespace zerom2m::onem2m
{

using zerom2m::onem2m::types::RequestPrimitive;

class IBinding
{
public:
    virtual ~IBinding() = default;

    virtual bool SendNotification(const RequestPrimitive &request, CNetSubSystem *net) = 0;
};

} // namespace zerom2m::onem2m
