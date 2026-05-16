/*
 * httpserver.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <circle/actled.h>
#include <circle/net/httpdaemon.h>

namespace zerom2m
{

class HttpServer : public CHTTPDaemon
{
public:
    /**
     * @brief Construct a new HttpServer instance
     *
     * @param net Pointer to the network subsystem
     * @param led Pointer to the LED to control
     * @param socket Pointer to the socket for this instance.
     *               Pass nullptr for the first instance, which acts as the listener.
     */
    HttpServer(u16 port, CNetSubSystem *net, CActLED *led, CSocket *socket = nullptr);

    ~HttpServer(void);

    /**
     * @brief Create a new worker instance for an accepted connection
     *
     * @param net Pointer to the network subsystem
     * @param socket Pointer to the accepted socket
     * @return CHTTPDaemon* Pointer to a new HttpServer worker
     */
    CHTTPDaemon *CreateWorker(CNetSubSystem *net, CSocket *socket) override;

    /**
     * @brief Provides content for HTTP requests
     *
     * Override this method to handle paths and generate responses.
     *
     * @param path Path of the request (e.g., "/")
     * @param params GET parameters ("" if none)
     * @param formData POST data ("" if none)
     * @param buffer Buffer to copy response content into
     * @param length In: buffer size, Out: content length
     * @param contentType Out: MIME type of response
     * @return THTTPStatus HTTP status code
     */
    THTTPStatus GetContent(const char  *path,
                           const char  *params,
                           const char  *formData,
                           u8          *buffer,
                           unsigned    *length,
                           const char **contentType) override;

private:
    CActLED *led_;
    u16      port_;
};

} // namespace zerom2m