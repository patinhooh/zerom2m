/*
 * http_daemon.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "http_parser.h"
#include "http_serializer.h"

#include <zerom2m/http/http_daemon.h>

#include <assert.h>
#include <circle/logger.h>
#include <circle/net/in.h>
#include <circle/netdevice.h>
#include <circle/string.h>
#include <circle/sysconfig.h>
#include <circle/util.h>

#define HTTPD_STACK_SIZE TASK_STACK_SIZE

namespace zerom2m::http
{

static const char FromHttpDaemon[] = "http_daemon";

unsigned HttpDaemon::instanceCount_ = 0;

HttpDaemon::HttpDaemon(CNetSubSystem *netSubSystem,
                       IHttpHandler  *handler,
                       CSocket       *socket,
                       unsigned       maxContentSize,
                       u16            port,
                       unsigned       timeoutSeconds,
                       unsigned       maxClients)
    : CTask(HTTPD_STACK_SIZE)
    , netSubSystem_(netSubSystem)
    , socket_(socket)
    , handler_(handler)
    , maxContentSize_(maxContentSize)
    , port_(port)
    , timeoutSeconds_(timeoutSeconds)
    , maxClients_(maxClients)
{
    instanceCount_++;

    if (socket_ == nullptr) {
        SetName(FromHttpDaemon);
    } else {
        CString TaskName;
        TaskName.Format("httpd@%lp", this);
        SetName(TaskName);
    }
}

HttpDaemon::~HttpDaemon(void)
{
    assert(socket_ == 0);
    netSubSystem_ = nullptr;
    handler_      = nullptr;
    instanceCount_--;
}

void HttpDaemon::Run(void)
{
    if (socket_ == 0) {
        CString ip;
        netSubSystem_->GetConfig()->GetIPAddress()->Format(&ip);
        CLogger::Get()->Write(
            FromHttpDaemon, LogNotice, "listening at http://%s:%u/", (const char *)ip, port_);
        Listener();
    } else {
        Worker();
    }
}

void HttpDaemon::WriteAccessLog(const CIPAddress &remoteIP,
                                RequestMethod     requestMethod,
                                const char       *requestURI,
                                ResponseStatus    status,
                                unsigned          contentLength)
{
    assert(requestURI != 0);

    CString IPString;
    remoteIP.Format(&IPString);

    const char *method;

    switch (requestMethod) {
        case GET:
            method = "GET";
            break;
        case HEAD:
            method = "HEAD";
            break;
        case POST:
            method = "POST";
            break;
        case PUT:
            method = "PUT";
            break;
        case DELETE:
            method = "DELETE";
            break;
        case PATCH:
            method = "PATCH";
            break;
        case OPTIONS:
            method = "OPTIONS";
            break;
        case TRACE:
            method = "TRACE";
            break;
        case CONNECT:
            method = "CONNECT";
            break;
        default:
            method = "UNKNOWN";
            break;
    }

    CLogger::Get()->Write(FromHttpDaemon,
                          LogDebug,
                          "%s \"%s %s\" %u %u",
                          (const char *)IPString,
                          method,
                          requestURI,
                          status,
                          contentLength);
}

void HttpDaemon::Listener(void)
{
    assert(netSubSystem_ != 0);
    socket_ = new CSocket(netSubSystem_, IPPROTO_TCP);
    assert(socket_ != 0);

    if (socket_->Bind(port_) < 0) {
        CLogger::Get()->Write(FromHttpDaemon, LogError, "Cannot bind socket (port %u)", port_);

        delete socket_;
        socket_ = 0;

        return;
    }

    if (socket_->Listen(maxClients_) < 0) {
        CLogger::Get()->Write(FromHttpDaemon, LogError, "Cannot listen on socket");

        delete socket_;
        socket_ = 0;

        return;
    }

    while (1) {
        CIPAddress ForeignIP;
        u16        nForeignPort;
        CSocket   *pConnection = socket_->Accept(&ForeignIP, &nForeignPort);
        if (pConnection == 0) {
            CLogger::Get()->Write(FromHttpDaemon, LogWarning, "Cannot accept connection");
            continue;
        }

        if (instanceCount_ >= maxClients_ + 1) {
            CLogger::Get()->Write(FromHttpDaemon, LogWarning, "Too many clients");
            delete pConnection;
            continue;
        }

        // spawn a worker task which will process this connection
        new HttpDaemon(netSubSystem_,
                       handler_,
                       pConnection,
                       maxContentSize_,
                       port_,
                       timeoutSeconds_,
                       maxClients_);
    }
}

void HttpDaemon::Worker(void)
{
    assert(socket_ != 0);

    socket_->SetOptionReceiveTimeout(timeoutSeconds_ * 1000000);

    // allocate a single buffer for header+body parsing.
    unsigned bufSize = maxContentSize_ > 0 ? maxContentSize_ : MAX_CONTENT_SIZE;
    u8      *pBuf    = new u8[bufSize];
    if (pBuf == nullptr) {
        delete socket_;
        socket_ = nullptr;
        return;
    }

    int nRecv = socket_->Receive((char *)pBuf, bufSize, 0);
    if (nRecv <= 0) {
        CLogger::Get()->Write(FromHttpDaemon, LogWarning, "Receive failed");

        delete[] pBuf;
        delete socket_;
        socket_ = nullptr;
        return;
    }

    // parse request
    HttpParser     parser;
    HttpRequest    request;
    ResponseStatus status = parser.Parse(pBuf, (size_t)nRecv, request);
    if (status != ResponseStatus::OK) {
        // build minimal protocol error response
        HttpResponse resp;
        resp.Status     = status;
        resp.Body       = nullptr;
        resp.BodyLength = 0;

        CString header;
        HttpSerializer::Serialize(resp, header);
        socket_->Send((const char *)header, header.GetLength(), MSG_DONTWAIT);
        CLogger::Get()->Write(FromHttpDaemon, LogWarning, "Parse failed status=%u", status);

        delete[] pBuf;
        delete socket_;
        socket_ = nullptr;
        return;
    }

    // delegate to application handler
    HttpResponse response;
    if (handler_) {
        response = handler_->HandleRequest(request);
    } else {
        response.Status     = ResponseStatus::InternalServerError;
        response.Body       = nullptr;
        response.BodyLength = 0;
    }

    // access log
    const u8 *pClientIP = socket_->GetForeignIP();
    if (pClientIP != 0) {
        CIPAddress  ClientIP(pClientIP);
        const char *target = request.Target.Data ? request.Target.Data : "";
        WriteAccessLog(
            ClientIP, request.Method, target, response.Status, (unsigned)response.BodyLength);
    }

    // serialize and send
    CString header;
    HttpSerializer::Serialize(response, header);

    if (socket_->Send((const char *)header, header.GetLength(), MSG_DONTWAIT) < 0) {
        CLogger::Get()->Write(FromHttpDaemon, LogError, "Cannot send response header");

        delete[] pBuf;
        delete socket_;
        socket_ = nullptr;
        return;
    }

    // send body (unless HEAD)
    if (request.Method != RequestMethod::HEAD && response.BodyLength > 0 &&
        response.Body != nullptr) {
        if (socket_->Send(response.Body, response.BodyLength, MSG_DONTWAIT) < 0) {
            CLogger::Get()->Write(FromHttpDaemon, LogError, "Cannot send response body");
        }
    }

    delete[] pBuf;
    delete socket_;
    socket_ = nullptr;
}

} // namespace zerom2m::http
