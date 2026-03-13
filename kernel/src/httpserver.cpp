/*
 * httpserver.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "zerom2m/httpserver.h"
#include <assert.h>
#include <circle/logger.h>
#include <circle/string.h>
#include <circle/util.h>

#define MAX_CONTENT_SIZE 2048
#define TIMEOUT_SECONDS 10

namespace zerom2m
{

namespace
{
const char FromHttpServer[] = "http";

const char indexPage[] = "<html>"
                         "<head><title>ZeroM2M</title></head>"
                         "<body>"
                         "<h1>ZeroM2M Server</h1>"
                         "<p>Server running.</p>"
                         "</body>"
                         "</html>";
} // namespace

HttpServer::HttpServer(CNetSubSystem *net, CActLED *led, CSocket *socket)
    : CHTTPDaemon(net, socket, MAX_CONTENT_SIZE, HTTP_PORT, 0, TIMEOUT_SECONDS)
    , led_(led)
{
}

HttpServer::~HttpServer() { led_ = nullptr; }

CHTTPDaemon *HttpServer::CreateWorker(CNetSubSystem *net, CSocket *socket)
{
    return new HttpServer(net, led_, socket);
}

THTTPStatus HttpServer::GetContent(const char  *path,
                                   const char  *params,
                                   const char  *formData,
                                   u8          *buffer,
                                   unsigned    *length,
                                   const char **contentType)
{
    assert(path != 0);
    assert(buffer != 0);
    assert(length != 0);
    assert(contentType != 0);

    if (strcmp(path, "/") != 0 && strcmp(path, "/index.html") != 0) { return HTTPNotFound; }

    unsigned size = strlen(indexPage);

    if (*length < size) {
        CLogger::Get()->Write(
            FromHttpServer, LogError, "Increase MAX_CONTENT_SIZE to at least %u", size);

        return HTTPInternalServerError;
    }

    memcpy(buffer, indexPage, size);

    *length      = size;
    *contentType = "text/html";

    return HTTPOK;
}

} // namespace zerom2m
