/*
 * serde.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "json/json_codec.h"

#include <zerom2m/onem2m/types/enums.h>
#include <zerom2m/serde/serde.h>

#include <circle/logger.h>

namespace zerom2m::serde
{

using namespace zerom2m::onem2m::types;
using zerom2m::serde::json::JsonCodec;

// MIME utilities
CString SerDe::NormalizeMimeType(const CString &mimeType) const
{
    const char *raw = mimeType.c_str();
    if (!raw) return CString{};

    const char  *semi = strchr(raw, ';');
    const size_t len  = semi ? static_cast<size_t>(semi - raw) : strlen(raw);

    CString out;
    for (size_t i = 0; i < len; ++i) {
        char c = raw[i];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') out += c;
    }
    return out;
}

boolean SerDe::IsJsonMime(const CString &mimeType) const
{
    const CString n = NormalizeMimeType(mimeType);
    return n.Compare(mime::JSON) == 0 || n.Compare(mime::JSON_M2M) == 0;
}

boolean SerDe::IsXmlMime(const CString &mimeType) const
{
    const CString n = NormalizeMimeType(mimeType);
    return n.Compare(mime::XML) == 0 || n.Compare(mime::XML_M2M) == 0;
}

// Codec registry
const ICodec *SerDe::CodecForMime(const CString &normalized) const
{
    if (normalized.Compare(mime::JSON) == 0 || normalized.Compare(mime::JSON_M2M) == 0)
        return &JsonCodec::Get();

    return nullptr;
}

boolean SerDe::DeserializeRequestBody(const CString    &input,
                                      const CString    &mimeType,
                                      RequestPrimitive &output) const
{
    const ICodec *codec = CodecForMime(NormalizeMimeType(mimeType));
    if (!codec) {
        CLogger::Get()->Write("SerDe", LogWarning, "No codec for MIME type");
        return false;
    }
    return codec->DeserializeRequestBody(input, output);
}

boolean SerDe::DeserializeRequestPrimitive(const CString    &input,
                                           const CString    &mimeType,
                                           RequestPrimitive &output) const
{
    const ICodec *codec = CodecForMime(NormalizeMimeType(mimeType));
    if (!codec) {
        CLogger::Get()->Write("SerDe", LogWarning, "No codec for MIME type");
        return false;
    }
    return codec->DeserializeRequestPrimitive(input, output);
}

boolean SerDe::DeserializeResponseBody(const CString     &input,
                                       const CString     &mimeType,
                                       ResponsePrimitive &output) const
{
    const ICodec *codec = CodecForMime(NormalizeMimeType(mimeType));
    if (!codec) return false;
    return codec->DeserializeResponseBody(input, output);
}

boolean SerDe::DeserializeResponsePrimitive(const CString     &input,
                                            const CString     &mimeType,
                                            ResponsePrimitive &output) const
{
    const ICodec *codec = CodecForMime(NormalizeMimeType(mimeType));
    if (!codec) return false;
    return codec->DeserializeResponsePrimitive(input, output);
}

boolean
SerDe::SerializeResource(const ResourceBase &input, const CString &mimeType, CString &output) const
{
    const ICodec *codec = CodecForMime(NormalizeMimeType(mimeType));
    if (!codec) return false;
    return codec->SerializeResource(input, output);
}

boolean SerDe::SerializePrimitiveContent(const PrimitiveContent &input,
                                         const CString          &mimeType,
                                         CString                &output) const
{
    const ICodec *codec = CodecForMime(NormalizeMimeType(mimeType));
    if (!codec) return false;
    return codec->SerializePrimitiveContent(input, output);
}

boolean SerDe::SerializeResponsePrimitive(const ResponsePrimitive &input,
                                          const CString           &mimeType,
                                          CString                 &output) const
{
    const ICodec *codec = CodecForMime(NormalizeMimeType(mimeType));
    if (!codec) return false;
    return codec->SerializeResponsePrimitive(input, output);
}

} // namespace zerom2m::serde