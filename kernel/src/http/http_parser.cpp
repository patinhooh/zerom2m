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
    if (data == nullptr || length == 0) { return ResponseStatus::BadRequest; }

    // We parse in-place; make a writable view of the input buffer.
    char  *buffer    = (char *)data;
    size_t i         = 0;
    size_t lineStart = 0;
    bool   firstLine = true;

    headerCount_ = 0;

    while (i < length) {
        if (buffer[i] == '\r') { buffer[i] = '\0'; }
        if (buffer[i] == '\n') {
            buffer[i]  = '\0';
            char *line = &buffer[lineStart];
            if (line[0] == '\0') {
                // End of headers
                size_t bodyOffset = i + 1;
                if (bodyOffset < length) {
                    request.Body       = (const u8 *)&buffer[bodyOffset];
                    request.BodyLength = length - bodyOffset;
                }
                request.Headers     = headers_;
                request.HeaderCount = headerCount_;
                return ResponseStatus::OK;
            }

            ResponseStatus status = ResponseStatus::OK;
            if (firstLine) {
                status    = ParseRequestLine(line, request);
                firstLine = false;
            } else {
                status = ParseHeaderLine(line, request);
            }

            if (status != ResponseStatus::OK) {
                CLogger::Get()->Write(FromHttpParser, LogWarning, "Parse failed status=%u", status);
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
