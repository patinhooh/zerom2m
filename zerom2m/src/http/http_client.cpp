/*
 * http_client.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include <zerom2m/http/http_client.h>

#include <assert.h>
#include <circle/logger.h>
#include <circle/net/in.h>
#include <circle/net/socket.h>
#include <circle/string.h>
#include <circle/util.h>

#include <string.h>

namespace zerom2m::http
{

namespace
{
const char FromHttpClient[] = "http_client";
const char UserAgent[]      = "ZeroM2M-HttpClient/1.0";

} // namespace

HttpClient::HttpClient(CNetSubSystem    *netSubSystem,
                       const CIPAddress &serverIP,
                       u16               serverPort,
                       const char       *serverName,
                       unsigned          timeoutSeconds)
    : netSubSystem_(netSubSystem)
    , serverIP_(serverIP)
    , serverPort_(serverPort)
    , serverName_()
    , timeoutSeconds_(timeoutSeconds)
{
    if (serverName != nullptr) { serverName_ = serverName; }
}

HttpClient::~HttpClient(void) { netSubSystem_ = nullptr; }

void HttpClient::SetTimeoutSeconds(unsigned timeoutSeconds) { timeoutSeconds_ = timeoutSeconds; }

bool HttpClient::Get(const char *path, HttpResponse &response)
{
    HttpRequest request;
    request.Method = RequestMethod::GET;
    request.Path   = {path, path != nullptr ? strlen(path) : 0};
    return Request(request, response);
}

bool HttpClient::Post(const char *path, const u8 *body, size_t bodyLength, HttpResponse &response)
{
    HttpRequest request;
    request.Method     = RequestMethod::POST;
    request.Path       = {path, path != nullptr ? strlen(path) : 0};
    request.Body       = body;
    request.BodyLength = bodyLength;
    return Request(request, response);
}

bool HttpClient::Request(RequestMethod     method,
                         const char       *path,
                         const HttpHeader *headers,
                         size_t            headerCount,
                         const u8         *body,
                         size_t            bodyLength,
                         HttpResponse     &response)
{
    HttpRequest request;
    request.Method      = method;
    request.Path        = {path, path != nullptr ? strlen(path) : 0};
    request.Headers     = headers;
    request.HeaderCount = headerCount;
    request.Body        = body;
    request.BodyLength  = bodyLength;
    return Request(request, response);
}

bool HttpClient::Request(const HttpRequest &request, HttpResponse &response)
{
    response = HttpResponse();

    if (netSubSystem_ == nullptr) {
        response.Status = ResponseStatus::ServiceUnavailable;
        return false;
    }

    if (request.Method == RequestMethodUnknown) {
        response.Status = ResponseStatus::MethodNotAllowed;
        return false;
    }

    CString host = serverName_;
    if (host.GetLength() == 0) { serverIP_.Format(&host); }

    CString header;
    HttpCodec::SerializeRequest(request, header, static_cast<const char *>(host), UserAgent);

    CSocket *socket = new CSocket(netSubSystem_, IPPROTO_TCP);
    if (socket == nullptr) {
        response.Status = ResponseStatus::ServiceUnavailable;
        return false;
    }

    if (timeoutSeconds_ > 0) {
        socket->SetOptionReceiveTimeout(timeoutSeconds_ * 1000000);
        socket->SetOptionSendTimeout(timeoutSeconds_ * 1000000);
    }

    if (socket->Connect(serverIP_, serverPort_) < 0) {
        delete socket;
        response.Status = ResponseStatus::ServiceUnavailable;
        return false;
    }

    if (socket->Send((const char *)header, header.GetLength(), 0) < 0) {
        delete socket;
        response.Status = ResponseStatus::BadGateway;
        return false;
    }

    if (request.Body != nullptr && request.BodyLength > 0 &&
        socket->Send(request.Body, static_cast<unsigned>(request.BodyLength), 0) < 0) {
        delete socket;
        response.Status = ResponseStatus::BadGateway;
        return false;
    }

    const size_t receiveCapacity = MAX_CONTENT_SIZE + 4096;
    u8          *buffer          = new u8[receiveCapacity];
    if (buffer == nullptr) {
        delete socket;
        response.Status = ResponseStatus::ServiceUnavailable;
        return false;
    }

    // Zero the buffer to ensure any string operations are safe even when
    // the peer closes the connection mid-read or recv returns an error.
    memset(buffer, 0, receiveCapacity);

    size_t totalReceived = 0;
    while (totalReceived < receiveCapacity) {
        int n = socket->Receive(reinterpret_cast<char *>(buffer + totalReceived),
                                static_cast<unsigned>(receiveCapacity - totalReceived),
                                0);
        if (n < 0) {
            CLogger::Get()->Write(FromHttpClient, LogWarning, "Receive failed (n<0)");
            if (totalReceived == 0) {
                delete[] buffer;
                delete socket;
                response.Status = ResponseStatus::GatewayTimeout;
                return false;
            }

            // If we've already received some bytes, stop receiving and try
            // to parse whatever we have. This tolerates transient recv
            // errors (e.g., non-blocking EAGAIN) that occur after the peer
            // has sent data and closed the connection.
            // Ensure the buffer is null-terminated at the actual received
            // length so subsequent parsing that relies on C string
            // functions does not read beyond the valid region.
            if (totalReceived < receiveCapacity) {
                buffer[totalReceived] = 0;
            } else {
                buffer[receiveCapacity - 1] = 0;
            }
            break;
        }

        if (n == 0) { break; }

        totalReceived += static_cast<size_t>(n);
    }

    delete socket;

    bool ok = HttpCodec::ParseResponse(buffer, totalReceived, response);
    delete[] buffer;

    if (!ok) {
        response.Status = ResponseStatus::BadGateway;
        response.ClearBody();
        return false;
    }

    return true;
}

bool HttpClient::RequestHeadersOnly(const HttpRequest &request, HttpResponse &response)
{
    response = HttpResponse();

    if (netSubSystem_ == nullptr) {
        response.Status = ResponseStatus::ServiceUnavailable;
        return false;
    }

    CString host = serverName_;
    if (host.GetLength() == 0) { serverIP_.Format(&host); }

    CString header;
    HttpCodec::SerializeRequest(request, header, static_cast<const char *>(host), UserAgent);

    CSocket *socket = new CSocket(netSubSystem_, IPPROTO_TCP);
    if (socket == nullptr) {
        response.Status = ResponseStatus::ServiceUnavailable;
        return false;
    }

    if (timeoutSeconds_ > 0) {
        socket->SetOptionReceiveTimeout(timeoutSeconds_ * 1000000);
        socket->SetOptionSendTimeout(timeoutSeconds_ * 1000000);
    }

    if (socket->Connect(serverIP_, serverPort_) < 0) {
        delete socket;
        response.Status = ResponseStatus::ServiceUnavailable;
        return false;
    }

    if (socket->Send((const char *)header, header.GetLength(), 0) < 0) {
        delete socket;
        response.Status = ResponseStatus::BadGateway;
        return false;
    }

    if (request.Body != nullptr && request.BodyLength > 0 &&
        socket->Send(request.Body, static_cast<unsigned>(request.BodyLength), 0) < 0) {
        delete socket;
        response.Status = ResponseStatus::BadGateway;
        return false;
    }

    const size_t receiveCapacity = 4096; // headers should fit in small buffer
    u8          *buffer          = new u8[receiveCapacity];
    if (buffer == nullptr) {
        delete socket;
        response.Status = ResponseStatus::ServiceUnavailable;
        return false;
    }

    size_t totalReceived  = 0;
    bool   foundHeaderEnd = false;
    while (totalReceived < receiveCapacity) {
        int n = socket->Receive(reinterpret_cast<char *>(buffer + totalReceived),
                                static_cast<unsigned>(receiveCapacity - totalReceived),
                                0);
        if (n < 0) {
            CLogger::Get()->Write(FromHttpClient, LogWarning, "Receive failed (n<0)");
            if (totalReceived == 0) {
                delete[] buffer;
                delete socket;
                response.Status = ResponseStatus::GatewayTimeout;
                return false;
            }
            break;
        }
        if (n == 0) break;
        totalReceived += static_cast<size_t>(n);

        // scan for header terminator
        for (size_t i = 0; i + 3 < totalReceived; ++i) {
            if (buffer[i] == '\r' && buffer[i + 1] == '\n' && buffer[i + 2] == '\r' &&
                buffer[i + 3] == '\n') {
                foundHeaderEnd = true;
                totalReceived  = i + 4; // limit to header bytes only
                break;
            }
        }
        if (foundHeaderEnd) break;
    }

    delete socket;

    // Null-terminate a copy for safe parsing
    if (totalReceived >= receiveCapacity) totalReceived = receiveCapacity - 1;
    buffer[totalReceived] = '\0';

    // Parse status line: "HTTP/1.x <status> ..."
    char *buf     = reinterpret_cast<char *>(buffer);
    char *lineEnd = strstr(buf, "\r\n");
    if (!lineEnd) lineEnd = strchr(buf, '\n');
    if (lineEnd) {
        *lineEnd = '\0';
        char *sp = strchr(buf, ' ');
        if (sp) {
            char *statusText = sp + 1;
            char *sp2        = strchr(statusText, ' ');
            if (sp2) *sp2 = '\0';
            char         *end    = nullptr;
            unsigned long status = strtoul(statusText, &end, 10);
            if (!(end == statusText || (end != nullptr && *end != '\0'))) {
                response.Status = static_cast<ResponseStatus>(status);
            } else {
                response.Status = ResponseStatus::UnknownResponseStatus;
            }
        }
    }

    delete[] buffer;
    return true;
}

} // namespace zerom2m::http