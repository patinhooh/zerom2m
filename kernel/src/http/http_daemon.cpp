/*
 * http_daemon.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "zerom2m/http/http_daemon.h"

#include <assert.h>
#include <circle/logger.h>
#include <circle/net/in.h>
#include <circle/netdevice.h>
#include <circle/string.h>
#include <circle/sysconfig.h>
#include <circle/util.h>

#define MAX_CLIENTS 10

#define HTTPD_STACK_SIZE TASK_STACK_SIZE

namespace zerom2m::http
{

static const char FromHttpDaemon[] = "http_daemon";

unsigned HttpDaemon::instanceCount_ = 0;

HttpDaemon::HttpDaemon(CNetSubSystem *pNetSubSystem,
                       IHttpHandler  *pHandler,
                       CSocket       *pSocket,
                       unsigned       nMaxContentSize,
                       u16            nPort,
                       unsigned       nTimeoutSeconds)
    : CTask(HTTPD_STACK_SIZE)
    , netSubSystem_(pNetSubSystem)
    , socket_(pSocket)
    , handler_(pHandler)
    , maxContentSize_(nMaxContentSize)
    , port_(nPort)
    , timeoutSeconds_(nTimeoutSeconds)
{
    instanceCount_++;

    if (pSocket == nullptr) {
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

void HttpDaemon::WriteAccessLog(const CIPAddress &rRemoteIP,
                                RequestMethod     requestMethod,
                                const char       *pRequestURI,
                                ResponseStatus    status,
                                unsigned          nContentLength)
{
    assert(pRequestURI != 0);

    CString IPString;
    rRemoteIP.Format(&IPString);

    const char *pMethod;

    static_assert(RequestMethod::RequestMethodCount == 10,
                  "RequestMethod enum has changed, update ParseMethodToken accordingly");
    switch (requestMethod) {
        case GET:
            pMethod = "GET";
            break;
        case HEAD:
            pMethod = "HEAD";
            break;
        case POST:
            pMethod = "POST";
            break;
        case PUT:
            pMethod = "PUT";
            break;
        case DELETE:
            pMethod = "DELETE";
            break;
        case PATCH:
            pMethod = "PATCH";
            break;
        case OPTIONS:
            pMethod = "OPTIONS";
            break;
        case TRACE:
            pMethod = "TRACE";
            break;
        case CONNECT:
            pMethod = "CONNECT";
            break;
        default:
            pMethod = "UNKNOWN";
            break;
    }

    CLogger::Get()->Write(FromHttpDaemon,
                          LogDebug,
                          "%s \"%s %s\" %u %u",
                          (const char *)IPString,
                          pMethod,
                          pRequestURI,
                          status,
                          nContentLength);
}

// Listener: bind and accept, spawn worker tasks (HttpDaemon instances)
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

    if (socket_->Listen(MAX_CLIENTS) < 0) {
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

        if (instanceCount_ >= MAX_CLIENTS + 1) {
            CLogger::Get()->Write(FromHttpDaemon, LogWarning, "Too many clients");
            delete pConnection;
            continue;
        }

        // spawn a worker task which will process this connection
        new HttpDaemon(
            netSubSystem_, handler_, pConnection, maxContentSize_, port_, timeoutSeconds_);
    }
}

// Worker: process a single connection, parse request, delegate to handler, serialize response
void HttpDaemon::Worker(void)
{
    assert(socket_ != 0);

    socket_->SetOptionReceiveTimeout(timeoutSeconds_ * 1000000);

    // allocate a single buffer for header+body parsing. Embedded-friendly: fixed size.
    unsigned bufSize = maxContentSize_ > 0 ? maxContentSize_ : 2048;
    u8      *pBuf    = new u8[bufSize];
    if (pBuf == nullptr) {
        delete socket_;
        socket_ = nullptr;
        return;
    }

    int nRecv = socket_->Receive((char *)pBuf, bufSize, 0);
    if (nRecv <= 0) {
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
