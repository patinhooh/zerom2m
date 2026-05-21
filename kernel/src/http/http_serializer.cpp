/*
 * http_serializer.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "zerom2m/http/http_serializer.h"
#include "zerom2m/http/http_common.h"

#include <circle/logger.h>
#include <circle/string.h>
#include <circle/util.h>

namespace zerom2m::http
{

namespace
{
const char FromHttpSerializer[] = "http_serializer";
}

/**
 * @brief Map a ResponseStatus to a human-readable reason phrase.
 */
const char *HttpSerializer::StatusReason(ResponseStatus status)
{
    // TODO: This is not a complete mapping of all status codes; add more as needed.
    switch (status) {
        case ResponseStatus::OK:
            return "OK";
        case ResponseStatus::BadRequest:
            return "Bad Request";
        case ResponseStatus::NotFound:
            return "Not Found";
        case ResponseStatus::MethodNotAllowed:
            return "Method Not Allowed";
        case ResponseStatus::ContentTooLarge:
            return "Content Too Large";
        case ResponseStatus::URITooLong:
            return "URI Too Long";
        case ResponseStatus::InternalServerError:
            return "Internal Server Error";
        case ResponseStatus::HTTPVersionNotSupported:
            return "HTTP Version Not Supported";
        default:
            return "Unknown";
    }
}

void HttpSerializer::Serialize(const HttpResponse &response, CString &outHeader)
{
    outHeader.Format("HTTP/1.1 %u %s\r\n", response.Status, StatusReason(response.Status));
    outHeader.Append("Server: " SERVER_NAME "\r\n");

    // Ensure Content-Length is always present. We add it even if caller omitted.
    unsigned contentLength = static_cast<unsigned>(response.BodyLength);

    bool hasContentLength = false;
    for (size_t i = 0; i < response.HeaderCount; ++i) {
        const HttpHeader &h = response.Headers[i];
        if (h.Name.Data && h.Value.Data) {
            const char *contentLengthName    = "Content-Length";
            size_t      contentLengthNameLen = sizeof("Content-Length") - 1;
            if (h.Name.Length == contentLengthNameLen &&
                strncmp(h.Name.Data, contentLengthName, contentLengthNameLen) == 0) {
                hasContentLength = true;
            }

            for (size_t n = 0; n < h.Name.Length; ++n) {
                outHeader += h.Name.Data[n];
            }
            outHeader.Append(": ");
            for (size_t n = 0; n < h.Value.Length; ++n) {
                outHeader += h.Value.Data[n];
            }
            outHeader.Append("\r\n");
        }
    }

    if (!hasContentLength) {
        CString len;
        len.Format("%u", contentLength);
        outHeader.Append("Content-Length: ");
        outHeader.Append(len);
        outHeader.Append("\r\n");
    }

    outHeader.Append("Connection: close\r\n\r\n");
}

} // namespace zerom2m::http
