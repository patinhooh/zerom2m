/*
 * serde.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include "codec.h"

#include <zerom2m/compat/types.h>
#include <zerom2m/onem2m/types/primitives.h>
#include <zerom2m/onem2m/types/resources.h>

#include <circle/string.h>

namespace zerom2m::serde
{

using namespace zerom2m::onem2m::types;
using namespace zerom2m::compat;

/**
 * @brief SerDe is the main entry point for serialisation and deserialisation of
 * oneM2M primitives and resources. It acts as a facade that routes to the appropriate
 * ICodec implementation based on the MIME type.
 */
class SerDe
{
public:
    static SerDe &Get()
    {
        static SerDe instance;
        return instance;
    }

    SerDe(const SerDe &)            = delete;
    SerDe &operator=(const SerDe &) = delete;
    SerDe(SerDe &&)                 = delete;
    SerDe &operator=(SerDe &&)      = delete;

    /**
     * @brief Deserialize a raw body into the content field of an already-populated
     * RequestPrimitive (op/to/fr/rqi are filled in by the binding layer).
     * @param input Raw request body (e.g. JSON string)
     * @param mimeType MIME type of the input (e.g. "application/json")
     * @param output Partially-populated RequestPrimitive to fill in (pc field only)
     * @return true on success, false on failure
     */
    boolean DeserializeRequestBody(const CString    &input,
                                   const CString    &mimeType,
                                   RequestPrimitive &output) const;

    /**
     * @brief Deserialize a full RequestPrimitive envelope (all primitive fields + pc).
     * @param input Raw request body (e.g. JSON string)
     * @param mimeType MIME type of the input (e.g. "application/json")
     * @param output Partially-populated RequestPrimitive to fill in
     * @return true on success, false on failure
     */
    boolean DeserializeRequestPrimitive(const CString    &input,
                                        const CString    &mimeType,
                                        RequestPrimitive &output) const;

    /**
     * @brief Serialize a single resource (any concrete type, accessed via its base)
     * into a wire-format string.
     * @param input Resource to serialize
     * @param mimeType MIME type to serialize as (e.g. "application/json")
     * @param output Serialized string
     * @return true on success, false on failure
     */
    boolean
    SerializeResource(const ResourceBase &input, const CString &mimeType, CString &output) const;

    /**
     * @brief Serialize whatever is stored inside a PrimitiveContent union.
     * @param input PrimitiveContent to serialize
     * @param mimeType MIME type to serialize as (e.g. "application/json")
     * @param output Serialized string
     * @return true on success, false on failure
     */
    boolean SerializePrimitiveContent(const PrimitiveContent &input,
                                      const CString          &mimeType,
                                      CString                &output) const;

    /**
     * @brief Serialize a full ResponsePrimitive envelope (status + pc + headers).
     * @param input ResponsePrimitive to serialize
     * @param mimeType MIME type to serialize as (e.g. "application/json")
     * @param output Serialized string
     * @return true on success, false on failure
     */
    boolean SerializeResponsePrimitive(const ResponsePrimitive &input,
                                       const CString           &mimeType,
                                       CString                 &output) const;

private:
    SerDe() = default;

    CString NormalizeMimeType(const CString &mimeType) const;
    boolean IsJsonMime(const CString &mimeType) const;
    boolean IsXmlMime(const CString &mimeType) const;

    /**
     * @brief Get the codec for a given normalized MIME type.
     * @param normalizedMime The normalized MIME type.
     * @return Pointer to the codec, or nullptr if not found.
     */
    const ICodec *CodecForMime(const CString &normalizedMime) const;
};

} // namespace zerom2m::serde