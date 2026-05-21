/*
 * index_handler.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "zerom2m/handlers/index_handler.h"

#include <circle/logger.h>
#include <circle/string.h>

namespace zerom2m::handlers
{

namespace
{

const char indexPage[] =
    "<html>"
    "<head><title>ZeroM2M</title>"
    "<link rel=\"icon\" href=\"data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' "
    "width='16' height='16'><rect width='16' height='16' fill='green'/></svg>\">"
    "</head>"
    "<body>"
    "<h1>ZeroM2M Server</h1>"
    "<p>Server running.</p>"
    "</body>"
    "</html>";
}

IndexHandler::IndexHandler(const KernelConfig *cfg)
    : cfg_(cfg)
{
}

http::HttpResponse IndexHandler::HandleRequest(const http::HttpRequest &request)
{
    (void)request;
    http::HttpResponse response;
    response.Status = http::ResponseStatus::OK;

    static CString page;
    if (cfg_) {
        const char *mode = cfg_->network.mode == NetworkMode::Auto
                               ? "auto"
                               : (cfg_->network.mode == NetworkMode::Wifi ? "wifi" : "ethernet");

        page.Format("<html><head><title>ZeroM2M</title>"
                    "<link rel=\"icon\" href=\"data:image/svg+xml;utf8,<svg "
                    "xmlns='http://www.w3.org/2000/svg' width='16' height='16'><rect width='16' "
                    "height='16' fill='black'/></svg>\">"
                    "</head><body>"
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
                    (const char *)cfg_->system.hostname,
                    mode,
                    cfg_->network.dhcp ? "true" : "false",
                    cfg_->network.ip[0],
                    cfg_->network.ip[1],
                    cfg_->network.ip[2],
                    cfg_->network.ip[3],
                    cfg_->network.netmask[0],
                    cfg_->network.netmask[1],
                    cfg_->network.netmask[2],
                    cfg_->network.netmask[3],
                    cfg_->network.gateway[0],
                    cfg_->network.gateway[1],
                    cfg_->network.gateway[2],
                    cfg_->network.gateway[3],
                    cfg_->network.dns[0],
                    cfg_->network.dns[1],
                    cfg_->network.dns[2],
                    cfg_->network.dns[3],
                    cfg_->http.port,
                    cfg_->http.max_content_size,
                    cfg_->http.timeout_seconds,
                    cfg_->http.max_clients);
    } else {
        page = indexPage;
    }

    static http::HttpHeader headers[] = {
        {{"Content-Type", sizeof("Content-Type") - 1}, {"text/html", sizeof("text/html") - 1}},
    };

    response.Headers     = headers;
    response.HeaderCount = 1;
    response.Body        = (const u8 *)(const char *)page;
    response.BodyLength  = page.GetLength();
    return response;
}

} // namespace zerom2m::handlers
