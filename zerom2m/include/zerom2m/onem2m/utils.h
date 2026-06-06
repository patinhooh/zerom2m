/*
 * utils.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <zerom2m/onem2m/types/primitives.h>

namespace zerom2m::onem2m
{

using namespace zerom2m::onem2m::types;

const ResourceBase *GetResourceBase(const PrimitiveContent &pc);

CString NormalizePath(const CString &path);

bool isValidResourceName(CString rn);

bool isValidRequest(const RequestPrimitive &req, CString &errMsg);

ResponsePrimitive makeResponse(const RequestPrimitive &req,
                               ResponseStatusCode      rsc,
                               PrimitiveContent        content = PrimitiveContent{});

} // namespace zerom2m::onem2m
