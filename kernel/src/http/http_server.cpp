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

HttpServer::HttpServer(u16                 port,
                       CNetSubSystem      *net,
                       CActLED            *led,
                       const KernelConfig *config,
                       unsigned            maxContentSize,
                       unsigned            timeoutSeconds,
                       unsigned            maxClients,
                       CSocket            *socket)
    : HttpDaemon(net, this, socket, maxContentSize, port, timeoutSeconds, maxClients)
    , led_(led)
    , config_(config)
{
}

HttpServer::~HttpServer() { led_ = nullptr; }

HttpResponse HttpServer::HandleRequest(const HttpRequest &request)
{
    HttpResponse response;

    if (request.Path.Data == nullptr) {
        CLogger::Get()->Write(FromHttpServer, LogWarning, "Missing request path");
        response.Status = ResponseStatus::BadRequest;
        return response;
    }

    if (!ViewEquals(request.Path, "/") && !ViewEquals(request.Path, "/index.html")) {
        response.Status = ResponseStatus::NotFound;
        return response;
    }
    response.Status = ResponseStatus::OK;

    // Generate HTML that includes kernel configuration when available.
    static CString page;
    if (config_) {
        const char *mode = config_->network.mode == NetworkMode::Auto
                               ? "auto"
                               : (config_->network.mode == NetworkMode::Wifi ? "wifi" : "ethernet");

        page.Format("<html><head><title>ZeroM2M</title></head><body>"
                    "<h1>ZeroM2M Server</h1><p>Server running.</p>"
                    "<h2>Kernel configuration</h2>"
                    "<table border=\"1\" cellpadding=\"5\">"
                    "<tr><th>Key</th><th>Value</th></tr>"
                    "<tr><td>hostname</td><td>%s</td></tr>"
                    "<tr><td>network.mode</td><td>%s</td></tr>"
                    "<tr><td>network.dhcp</td><td>%s</td></tr>"
                    "<tr><td>network.ip</td><td>%u.%u.%u.%u</td></tr>"
                    "<tr><td>network.netmask</td><td>%u.%u.%u.%u</td></tr>"
                    "<tr><td>network.gateway</td><td>%u.%u.%u.%u</td></tr>"
                    "<tr><td>network.dns</td><td>%u.%u.%u.%u</td></tr>"
                    "<tr><td>http.port</td><td>%u</td></tr>"
                    "<tr><td>http.max_content_size</td><td>%u</td></tr>"
                    "<tr><td>http.timeout_seconds</td><td>%u</td></tr>"
                    "<tr><td>http.max_clients</td><td>%u</td></tr>"
                    "</table></body></html>",
                    (const char *)config_->system.hostname,
                    mode,
                    config_->network.dhcp ? "true" : "false",
                    config_->network.ip[0],
                    config_->network.ip[1],
                    config_->network.ip[2],
                    config_->network.ip[3],
                    config_->network.netmask[0],
                    config_->network.netmask[1],
                    config_->network.netmask[2],
                    config_->network.netmask[3],
                    config_->network.gateway[0],
                    config_->network.gateway[1],
                    config_->network.gateway[2],
                    config_->network.gateway[3],
                    config_->network.dns[0],
                    config_->network.dns[1],
                    config_->network.dns[2],
                    config_->network.dns[3],
                    config_->http.port,
                    config_->http.max_content_size,
                    config_->http.timeout_seconds,
                    config_->http.max_clients);
    } else {
        page = indexPage;
    }

    response.Body       = (const u8 *)(const char *)page;
    response.BodyLength = page.GetLength();

    static HttpHeader headers[] = {
        {{"Content-Type", sizeof("Content-Type") - 1}, {"text/html", sizeof("text/html") - 1}},
    };

    response.Headers     = headers;
    response.HeaderCount = 1;

    return response;
}

} // namespace zerom2m::http
