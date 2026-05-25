/*
 * codec.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <zerom2m/compat/types.h>
#include <zerom2m/onem2m/types/primitives.h>
#include <zerom2m/onem2m/types/resources.h>

namespace zerom2m::serde
{

using namespace zerom2m::onem2m::types;

/**
 * @brief ICodec defines the interface for serialising and deserialising oneM2M primitives and
 * resources.
 */
class ICodec
{
public:
    virtual ~ICodec() = default;

    /**
     * @brief Deserialize a raw body into the content field of an already-populated
     * RequestPrimitive (op/to/fr/rqi are filled in by the binding layer).
     * @param input Raw request body (e.g. JSON string)
     * @param output Partially-populated RequestPrimitive to fill in (pc field only)
     * @return true on success, false on failure (malformed input, unsupported content type etc.)
     */
    virtual boolean DeserializeRequestBody(const CString    &input,
                                           RequestPrimitive &output) const = 0;

    /**
     * @brief Serialize a single resource (any concrete type, accessed via its base)
     * into a wire-format string.
     * @param input Resource to serialize
     * @param output Serialized string
     * @return true on success, false on failure
     */
    virtual boolean SerializeResource(const ResourceBase &input, CString &output) const = 0;

    /**
     * @brief Serialize whatever is stored inside a PrimitiveContent union.
     * @param input PrimitiveContent to serialize
     * @param output Serialized string
     * @return true on success, false on failure
     */
    virtual boolean SerializePrimitiveContent(const PrimitiveContent &input,
                                              CString                &output) const = 0;

    /**
     * @brief Serialize a full ResponsePrimitive envelope (status + pc + headers).
     * @param input ResponsePrimitive to serialize
     * @param output Serialized string
     * @return true on success, false on failure
     */
    virtual boolean SerializeResponsePrimitive(const ResponsePrimitive &input,
                                               CString                 &output) const = 0;

    /**
     * @brief Deserialize a full RequestPrimitive envelope (all primitive fields + pc).
     * @param input Raw request body (e.g. JSON string)
     * @param output Partially-populated RequestPrimitive to fill in
     * @return true on success, false on failure
     */
    virtual boolean DeserializeRequestPrimitive(const CString    &input,
                                                RequestPrimitive &output) const = 0;
};

} // namespace zerom2m::serde