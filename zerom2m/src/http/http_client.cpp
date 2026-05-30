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

HttpClient::HttpClient(CNetSubSystem *netSubSystem,
                       const CIPAddress &serverIP,
                       u16 serverPort,
                       const char *serverName,
                       unsigned timeoutSeconds)
    : netSubSystem_(netSubSystem)
    , serverIP_(serverIP)
    , serverPort_(serverPort)
    , serverName_()
    , timeoutSeconds_(timeoutSeconds)
{
    if (serverName != nullptr) {
        serverName_ = serverName;
    }
}

HttpClient::~HttpClient(void)
{
    netSubSystem_ = nullptr;
}

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

bool HttpClient::Request(RequestMethod method,
                         const char *path,
                         const HttpHeader *headers,
                         size_t headerCount,
                         const u8 *body,
                         size_t bodyLength,
                         HttpResponse &response)
{
    HttpRequest request;
    request.Method      = method;
    request.Path        = {path, path != nullptr ? strlen(path) : 0};
    request.Headers     = headers;
    request.HeaderCount  = headerCount;
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
    if (host.GetLength() == 0) {
        serverIP_.Format(&host);
    }

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
            break;
        }

        if (n == 0) {
            break;
        }

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

} // namespace zerom2m::http