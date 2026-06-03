/*
 * http_daemon.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include <zerom2m/http/http_codec.h>
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
    CLogger::Get()->Write(FromHttpDaemon, LogDebug, "Worker %u finished", instanceCount_ - 1);
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
                          "%s %u %u %s '%s'",
                          method,
                          status,
                          contentLength,
                          (const char *)IPString,
                          requestURI);
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

    size_t totalRecv = (size_t)nRecv;
    size_t headerEnd = 0;
    bool   foundEnd  = false;

    for (size_t i = 0; i + 3 < totalRecv; i++) {
        if (pBuf[i] == '\r' && pBuf[i + 1] == '\n' && pBuf[i + 2] == '\r' && pBuf[i + 3] == '\n') {
            headerEnd = i + 4;
            foundEnd  = true;
            break;
        }
    }

    if (foundEnd) {
        u8 saved        = pBuf[headerEnd];
        pBuf[headerEnd] = '\0';

        size_t      contentLength = 0;
        const char *clHeader      = strstr((const char *)pBuf, "Content-Length: ");
        if (clHeader == nullptr) { clHeader = strstr((const char *)pBuf, "content-length: "); }
        if (clHeader != nullptr) { contentLength = (size_t)atoi(clHeader + 16); }

        pBuf[headerEnd] = saved;

        size_t bodyReceived = totalRecv - headerEnd;
        while (bodyReceived < contentLength) {
            size_t spaceLeft = bufSize - totalRecv;
            if (spaceLeft == 0) {
                CLogger::Get()->Write(
                    FromHttpDaemon,
                    LogWarning,
                    "Buffer full before body complete: bufSize=%u contentLength=%u",
                    (unsigned)bufSize,
                    (unsigned)contentLength);
                break;
            }
            size_t remaining = contentLength - bodyReceived;
            size_t toRead    = remaining < spaceLeft ? remaining : spaceLeft;
            int    n         = socket_->Receive((char *)pBuf + totalRecv, toRead, 0);
            if (n <= 0) {
                CLogger::Get()->Write(FromHttpDaemon,
                                      LogWarning,
                                      "Receive failed waiting for body: got=%u need=%u",
                                      (unsigned)bodyReceived,
                                      (unsigned)contentLength);
                break;
            }
            totalRecv += (size_t)n;
            bodyReceived += (size_t)n;
        }
    }

    HttpRequest    request;
    ResponseStatus status = HttpCodec::ParseRequest(pBuf, totalRecv, request);
    if (status != ResponseStatus::OK) {
        HttpResponse resp;
        resp.Status = status;
        resp.Body   = "";

        CString header;
        HttpCodec::SerializeResponse(resp, header);

        socket_->Send(header.c_str(), header.GetLength(), MSG_DONTWAIT);

        CLogger::Get()->Write(
            FromHttpDaemon, LogWarning, "Parse failed status=%u", (unsigned)status);

        delete[] pBuf;
        delete socket_;
        socket_ = nullptr;
        return;
    }

    HttpResponse response;

    if (handler_) {
        response = handler_->HandleRequest(request);
    } else {
        response.Status = ResponseStatus::InternalServerError;
        response.Body   = "";
    }

    const u8 *pClientIP = socket_->GetForeignIP();

    if (pClientIP != 0) {
        CIPAddress clientIP(pClientIP);

        WriteAccessLog(clientIP,
                       request.Method,
                       request.Target.c_str(),
                       response.Status,
                       (unsigned)response.Body.GetLength());
    }

    CString header;
    HttpCodec::SerializeResponse(response, header);

    if (socket_->Send(header.c_str(), header.GetLength(), MSG_DONTWAIT) < 0) {
        CLogger::Get()->Write(FromHttpDaemon, LogError, "Cannot send response header");

        delete[] pBuf;
        delete socket_;
        socket_ = nullptr;
        return;
    }

    if (request.Method != RequestMethod::HEAD && response.Body.GetLength() > 0) {
        if (socket_->Send(response.Body.c_str(), response.Body.GetLength(), MSG_DONTWAIT) < 0) {
            CLogger::Get()->Write(FromHttpDaemon, LogError, "Cannot send response body");
        }
    }

    delete[] pBuf;
    delete socket_;
    socket_ = nullptr;
}

} // namespace zerom2m::http
