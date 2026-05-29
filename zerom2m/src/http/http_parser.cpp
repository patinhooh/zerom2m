/*
 * http_parser.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "http_parser.h"

#include <assert.h>
#include <circle/logger.h>
#include <circle/string.h>
#include <circle/util.h>

namespace zerom2m::http
{

namespace
{
const char FromHttpParser[] = "http_parser";

inline void TrimSpaces(char *&start, char *&end)
{
    while (start < end && (*start == ' ' || *start == '\t')) {
        start++;
    }
    while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
}

inline bool Equals(const char *a, const char *b) { return strcmp(a, b) == 0; }

RequestMethod ParseMethodToken(const char *token)
{
    if (Equals(token, "GET")) return RequestMethod::GET;
    if (Equals(token, "HEAD")) return RequestMethod::HEAD;
    if (Equals(token, "POST")) return RequestMethod::POST;
    if (Equals(token, "PUT")) return RequestMethod::PUT;
    if (Equals(token, "DELETE")) return RequestMethod::DELETE;
    if (Equals(token, "PATCH")) return RequestMethod::PATCH;
    if (Equals(token, "OPTIONS")) return RequestMethod::OPTIONS;
    if (Equals(token, "TRACE")) return RequestMethod::TRACE;
    if (Equals(token, "CONNECT")) return RequestMethod::CONNECT;
    return RequestMethod::RequestMethodUnknown;
}

} // namespace

ResponseStatus HttpParser::Parse(const u8 *data, size_t length, HttpRequest &request)
{
    // CLogger::Get()->Write(FromHttpParser, LogDebug, "Parse: length=%u", (unsigned)length);

    if (data == nullptr || length == 0) { return ResponseStatus::BadRequest; }

    // // Dump raw bytes as hex rows of 16, using only Write's %02X and manual iteration
    // {
    //     const size_t dumpLen = length < 256 ? length : 256;
    //     for (size_t d = 0; d < dumpLen; d += 4) {
    //         // Print 4 bytes at a time with their ascii equivalents
    //         // Avoids any buffer building - just individual Write calls
    //         u8 b0 = d+0 < dumpLen ? data[d+0] : 0;
    //         u8 b1 = d+1 < dumpLen ? data[d+1] : 0;
    //         u8 b2 = d+2 < dumpLen ? data[d+2] : 0;
    //         u8 b3 = d+3 < dumpLen ? data[d+3] : 0;
    //         char c0 = (b0 >= 0x20 && b0 < 0x7F) ? (char)b0 : '.';
    //         char c1 = (b1 >= 0x20 && b1 < 0x7F) ? (char)b1 : '.';
    //         char c2 = (b2 >= 0x20 && b2 < 0x7F) ? (char)b2 : '.';
    //         char c3 = (b3 >= 0x20 && b3 < 0x7F) ? (char)b3 : '.';
    //         CLogger::Get()->Write(FromHttpParser, LogDebug,
    //             "[%03u] %02X %02X %02X %02X  '%c%c%c%c'",
    //             (unsigned)d, b0, b1, b2, b3, c0, c1, c2, c3);
    //         CTimer::Get()->MsDelay(100); // Small delay to avoid overwhelming the log with too many entries at once
    //     }
    // }

    char  *buffer    = (char *)data;
    size_t i         = 0;
    size_t lineStart = 0;
    bool   firstLine = true;

    headerCount_ = 0;

    while (i < length) {
        if (buffer[i] == '\r') {
            // CLogger::Get()->Write(FromHttpParser, LogDebug,
            //     "i=%u CR lineStart=%u", (unsigned)i, (unsigned)lineStart);
            buffer[i] = '\0';
            if (i + 1 < length && data[i + 1] == '\n') {
                i++;
            } else {
                i++;
                lineStart = i;
                continue;
            }
        }

        if (buffer[i] == '\n') {
            buffer[i]  = '\0';
            char *line = &buffer[lineStart];

            // size_t lineLen = i - lineStart;
            // CLogger::Get()->Write(FromHttpParser, LogDebug,
            //     "i=%u LF lineStart=%u lineLen=%u",
            //     (unsigned)i, (unsigned)lineStart, (unsigned)lineLen);

            if (line[0] == '\0') {
                size_t bodyOffset = i + 1;
                // CLogger::Get()->Write(FromHttpParser, LogDebug,
                //     "BlankLine: bodyOffset=%u length=%u remaining=%u",
                //     (unsigned)bodyOffset, (unsigned)length,
                //     bodyOffset <= length ? (unsigned)(length - bodyOffset) : 0u);

                if (bodyOffset < length) {
                    request.Body       = (const u8 *)&buffer[bodyOffset];
                    request.BodyLength = length - bodyOffset;
                    // Print first 4 bytes of body so we can confirm it's JSON
                    // u8 bb0 = request.Body[0];
                    // u8 bb1 = request.BodyLength > 1 ? request.Body[1] : 0;
                    // u8 bb2 = request.BodyLength > 2 ? request.Body[2] : 0;
                    // u8 bb3 = request.BodyLength > 3 ? request.Body[3] : 0;
                    // CLogger::Get()->Write(FromHttpParser, LogDebug,
                    //     "Body: length=%u first4=%02X %02X %02X %02X ('%c%c%c%c')",
                    //     (unsigned)request.BodyLength,
                    //     bb0, bb1, bb2, bb3,
                    //     bb0 >= 0x20 ? (char)bb0 : '.',
                    //     bb1 >= 0x20 ? (char)bb1 : '.',
                    //     bb2 >= 0x20 ? (char)bb2 : '.',
                    //     bb3 >= 0x20 ? (char)bb3 : '.');
                } else {
                    request.Body       = nullptr;
                    request.BodyLength = 0;
                }

                request.Headers     = headers_;
                request.HeaderCount = headerCount_;
                // CLogger::Get()->Write(FromHttpParser, LogDebug,
                //     "Parse OK: headerCount=%u bodyLength=%u",
                //     (unsigned)headerCount_, (unsigned)request.BodyLength);
                return ResponseStatus::OK;
            }

            ResponseStatus status = ResponseStatus::OK;
            if (firstLine) {
                // CLogger::Get()->Write(FromHttpParser, LogDebug, "RequestLine: '%s'", line);
                status    = ParseRequestLine(line, request);
                firstLine = false;
            } else {
                // CLogger::Get()->Write(FromHttpParser, LogDebug,
                //     "Header[%u]: '%s'", (unsigned)headerCount_, line);
                status = ParseHeaderLine(line, request);
            }

            if (status != ResponseStatus::OK) {
                CLogger::Get()->Write(FromHttpParser, LogWarning,
                    "ParseFailed: i=%u status=%u", (unsigned)i, (unsigned)status);
                return status;
            }

            lineStart = i + 1;
        }
        i++;
    }

    return ResponseStatus::BadRequest;
}

ResponseStatus HttpParser::ParseRequestLine(char *line, HttpRequest &request)
{
    // Expected: METHOD SP TARGET SP HTTP/VERSION
    char *method = line;
    char *sp1    = strchr(line, ' ');
    if (!sp1) {
        CLogger::Get()->Write(FromHttpParser, LogWarning, "Request line missing method");
        return ResponseStatus::BadRequest;
    }
    *sp1 = '\0';

    char *target = sp1 + 1;
    char *sp2    = strchr(target, ' ');
    if (!sp2) {
        CLogger::Get()->Write(FromHttpParser, LogWarning, "Request line missing target");
        return ResponseStatus::BadRequest;
    }
    *sp2 = '\0';

    char *version = sp2 + 1;
    if (*version == '\0') {
        CLogger::Get()->Write(FromHttpParser, LogWarning, "Request line missing version");
        return ResponseStatus::BadRequest;
    }

    request.Method = ParseMethodToken(method);
    if (request.Method == RequestMethod::RequestMethodUnknown) {
        CLogger::Get()->Write(FromHttpParser, LogWarning, "Unknown method: %s", method);
        return ResponseStatus::MethodNotAllowed;
    }

    request.Target.Data    = target;
    request.Target.Length  = strlen(target);
    request.Version.Data   = version;
    request.Version.Length = strlen(version);

    // Split target into path and query
    char *q = strchr(target, '?');
    if (q) {
        *q                   = '\0';
        request.Path.Data    = target;
        request.Path.Length  = strlen(target);
        request.Query.Data   = q + 1;
        request.Query.Length = strlen(q + 1);
    } else {
        request.Path.Data    = target;
        request.Path.Length  = strlen(target);
        request.Query.Data   = nullptr;
        request.Query.Length = 0;
    }

    return ResponseStatus::OK;
}

ResponseStatus HttpParser::ParseHeaderLine(char *line, HttpRequest &request)
{
    (void)request;

    if (headerCount_ >= MaxHeaders) {
        CLogger::Get()->Write(FromHttpParser, LogWarning, "Too many headers");
        return ResponseStatus::RequestHeaderFieldsTooLarge;
    }

    char *colon = strchr(line, ':');
    if (!colon) {
        CLogger::Get()->Write(FromHttpParser, LogWarning, "Header missing colon");
        return ResponseStatus::BadRequest;
    }

    char *nameStart  = line;
    char *nameEnd    = colon;
    char *valueStart = colon + 1;
    char *valueEnd   = line + strlen(line);

    TrimSpaces(nameStart, nameEnd);
    TrimSpaces(valueStart, valueEnd);

    *nameEnd  = '\0';
    *valueEnd = '\0';

    headers_[headerCount_].Name.Data    = nameStart;
    headers_[headerCount_].Name.Length  = strlen(nameStart);
    headers_[headerCount_].Value.Data   = valueStart;
    headers_[headerCount_].Value.Length = strlen(valueStart);
    headerCount_++;

    return ResponseStatus::OK;
}

} // namespace zerom2m::http
