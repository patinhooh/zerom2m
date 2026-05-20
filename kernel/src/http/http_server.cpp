/*
 * http_server.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "zerom2m/http/http_server.h"

#include <assert.h>
#include <circle/logger.h>
#include <circle/string.h>
#include <circle/util.h>

#define MAX_CONTENT_SIZE 2048
#define TIMEOUT_SECONDS 10

namespace zerom2m::http
{

namespace
{
const char FromHttpServer[] = "http_server";

const char indexPage[] = "<html>"
                         "<head><title>ZeroM2M</title></head>"
                         "<body>"
                         "<h1>ZeroM2M Server</h1>"
                         "<p>Server running.</p>"
                         "</body>"
                         "</html>";

bool ViewEquals(const StringView &view, const char *literal)
{
    if (view.Data == nullptr || literal == nullptr) { return false; }
    size_t len = strlen(literal);
    return view.Length == len && strncmp(view.Data, literal, len) == 0;
}
} // namespace

HttpServer::HttpServer(u16 port, CNetSubSystem *net, CActLED *led, CSocket *socket)
    : HttpDaemon(net, this, socket, MAX_CONTENT_SIZE, port, TIMEOUT_SECONDS)
    , led_(led)
{
}

HttpServer::~HttpServer() { led_ = nullptr; }

HttpResponse HttpServer::HandleRequest(const HttpRequest &request)
{
    HttpResponse response;

    if (request.Path.Data == nullptr) {
        response.Status = ResponseStatus::BadRequest;
        return response;
    }

    if (!ViewEquals(request.Path, "/") && !ViewEquals(request.Path, "/index.html")) {
        response.Status = ResponseStatus::NotFound;
        return response;
    }

    response.Status     = ResponseStatus::OK;
    response.Body       = (const u8 *)indexPage;
    response.BodyLength = strlen(indexPage);

    static HttpHeader headers[] = {
        {{"Content-Type", sizeof("Content-Type") - 1}, {"text/html", sizeof("text/html") - 1}},
    };

    response.Headers     = headers;
    response.HeaderCount = 1;

    return response;
}

} // namespace zerom2m::http
