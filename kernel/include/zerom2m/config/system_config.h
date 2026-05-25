/*
 * kernel_config.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <zerom2m/http/config.h>

#include <circle/string.h>
#include <circle/types.h>

namespace zerom2m::config
{

enum class NetworkMode { Auto, Wifi, Ethernet };

struct SystemConfig {
    struct {
        NetworkMode mode;
        CString     open_net_ssid;
        bool        dhcp;
        // static network config (used only if dhcp == false)
        u8 ip[4];
        u8 netmask[4];
        u8 gateway[4];
        u8 dns[4];
    } network;

    struct {
        u16      port;
        unsigned max_content_size;
        unsigned timeout_seconds;
        unsigned max_clients;
    } http;

    struct {
        CString hostname;
    } system;

    SystemConfig()
    {
        network.mode          = NetworkMode::Auto;
        network.open_net_ssid = "";
        network.dhcp          = true;
        network.ip[0] = network.ip[1] = network.ip[2] = network.ip[3] = 0;
        network.netmask[0] = network.netmask[1] = network.netmask[2] = network.netmask[3] = 0;
        network.gateway[0] = network.gateway[1] = network.gateway[2] = network.gateway[3] = 0;
        network.dns[0] = network.dns[1] = network.dns[2] = network.dns[3] = 0;

        http.port             = http::DEFAULT_PORT;
        http.max_content_size = http::MAX_CONTENT_SIZE;
        http.timeout_seconds  = http::TIMEOUT_SECONDS;
        http.max_clients      = http::MAX_CLIENTS;
        system.hostname       = "zerom2m";
    }
};

} // namespace zerom2m
