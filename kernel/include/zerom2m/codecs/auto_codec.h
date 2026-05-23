/*
 * auto_codec.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include "zerom2m/onem2m/types/primitives.h"
#include "zerom2m/onem2m/types/resources.h"

#include <circle/string.h>

// TODO: This is just a very basic implementation to get something working.

namespace zerom2m::codecs
{

using namespace zerom2m::onem2m::types;

class AutoCodec
{
public:
    AutoCodec() = default;

    CString NormalizeMimeType(const CString &mimeType) const;
    bool    IsJsonMime(const CString &mimeType) const;

    bool DeserializeRequestBody(const CString    &body,
                                const CString    &mimeType,
                                RequestPrimitive &prim) const;

    CString SerializeResource(const ResourceBase &resource, const CString &mimeType) const;
    CString SerializePrimitiveContent(const PrimitiveContent &content,
                                      const CString          &mimeType) const;
};

} // namespace zerom2m::codecs
