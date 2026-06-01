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
