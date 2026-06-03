/*
 * http_client.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <zerom2m/http/http_codec.h>
#include <zerom2m/http/types.h>

#include <circle/net/ipaddress.h>
#include <circle/net/netsubsystem.h>
#include <circle/types.h>

namespace zerom2m::http
{

/**
 * @brief Simple synchronous HTTP client.
 *
 * The client opens a TCP connection per request, sends a single HTTP/1.1
 * request with `Connection: close`, and parses the response into the project
 * HTTP response type.
 */
class HttpClient
{
public:
    HttpClient(CNetSubSystem    *netSubSystem,
               const CIPAddress &serverIP,
               u16               serverPort     = DEFAULT_PORT,
               const char       *serverName     = nullptr,
               unsigned          timeoutSeconds = 0);

    ~HttpClient(void);

    bool Request(const HttpRequest &request, HttpResponse &response);

    /**
     * @brief Send request and read only response headers/status (no body parse).
     *
     * Useful for notification verification where only the response status
     * matters; avoids allocating/parsing the response body.
     */
    bool RequestHeadersOnly(const HttpRequest &request, HttpResponse &response);

    bool Get(const char *path, HttpResponse &response);
    bool Post(const char *path, const CString &body, size_t bodyLength, HttpResponse &response);

    void SetTimeoutSeconds(unsigned timeoutSeconds);

private:
    CNetSubSystem *netSubSystem_;
    CIPAddress     serverIP_;
    u16            serverPort_;
    CString        serverName_;
    unsigned       timeoutSeconds_;
};

} // namespace zerom2m::http