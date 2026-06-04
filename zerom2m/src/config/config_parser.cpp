/*
 * config_parser.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include <zerom2m/config/config_parser.h>

#include <circle/logger.h>
#include <circle/string.h>
#include <circle/util.h>
#include <fatfs/ff.h>

namespace zerom2m::config
{

namespace
{
const char FromConfigParser[] = "config_parser";
} // namespace

ConfigParser::ConfigParser(SystemConfig &config, CLogger &logger)
    : config_(config)
    , logger_(logger)
{
}

bool ConfigParser::Load(const char *path)
{
    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK) return false;

    char section[32] = {0};
    char line[128];

    while (f_gets(line, sizeof(line), &file)) {
        Trim(line);

        // skip empty
        if (line[0] == '\0') continue;
        // comments
        if (line[0] == '#' || line[0] == ';') continue;
        // section
        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end) {
                *end = '\0';
                strncpy(section, line + 1, sizeof(section) - 1);
            }
            continue;
        }

        ParseLine(section, line);
    }

    f_close(&file);
    return true;
}

void ConfigParser::ParseLine(const char *section, char *line)
{
    char *eq = strchr(line, '=');
    if (!eq) return;

    *eq         = '\0';
    char *key   = line;
    char *value = eq + 1;

    Trim(key);
    Trim(value);
    StripComment(value);
    Trim(value);

    // [network]
    if (strcmp(section, "network") == 0) {
        if (strcmp(key, "mode") == 0) {
            if (strcmp(value, "auto") == 0) {
                config_.network.mode = NetworkMode::Auto;
            } else if (strcmp(value, "wifi") == 0) {
                config_.network.mode = NetworkMode::Wifi;
            } else if (strcmp(value, "ethernet") == 0) {
                config_.network.mode = NetworkMode::Ethernet;
            } else logger_.Write(FromConfigParser, LogWarning, "Unknown network mode: '%s'", value);
        } else if (strcmp(key, "open_net_ssid") == 0) {
            config_.network.open_net_ssid = value;
        } else if (strcmp(key, "dhcp") == 0) {
            config_.network.dhcp = ParseBool(value);
        } else if (strcmp(key, "ip") == 0) {
            if (!ParseIPv4(value, config_.network.ip)) {
                logger_.Write(FromConfigParser, LogWarning, "Invalid IP address: '%s'", value);
            }
        } else if (strcmp(key, "netmask") == 0) {
            if (!ParseIPv4(value, config_.network.netmask)) {
                logger_.Write(FromConfigParser, LogWarning, "Invalid netmask: '%s'", value);
            }
        } else if (strcmp(key, "gateway") == 0) {
            if (!ParseIPv4(value, config_.network.gateway)) {
                logger_.Write(FromConfigParser, LogWarning, "Invalid gateway: '%s'", value);
            }
        } else if (strcmp(key, "dns") == 0) {
            if (!ParseIPv4(value, config_.network.dns)) {
                logger_.Write(FromConfigParser, LogWarning, "Invalid DNS server: '%s'", value);
            }
        } else {
            logger_.Write(FromConfigParser, LogWarning, "Unknown network config key: '%s'", key);
        }
    }
    // [http]
    else if (strcmp(section, "http") == 0) {
        if (strcmp(key, "port") == 0) {
            config_.http.port = atoi(value);
        } else if (strcmp(key, "max_content_size") == 0) {
            config_.http.max_content_size = (unsigned)atoi(value);
        } else if (strcmp(key, "timeout_seconds") == 0) {
            config_.http.timeout_seconds = (unsigned)atoi(value);
        } else if (strcmp(key, "max_clients") == 0) {
            config_.http.max_clients = (unsigned)atoi(value);
        } else {
            logger_.Write(FromConfigParser, LogWarning, "Unknown http config key: '%s'", key);
        }
    }
    // [cse]
    else if (strcmp(section, "cse") == 0) {
        if (strcmp(key, "resource_name") == 0) config_.cse.resource_name = value;
        else if (strcmp(key, "resource_id") == 0) config_.cse.resource_id = value;
        else if (strcmp(key, "cse_id") == 0) config_.cse.cse_id = value;
        else if (strcmp(key, "sp_id") == 0) config_.cse.sp_id = value;
        else logger_.Write(FromConfigParser, LogWarning, "Unknown cse config key: '%s'", key);
    }
    // [system]
    else if (strcmp(section, "system") == 0) {
        if (strcmp(key, "hostname") == 0) config_.system.hostname = value;
        else if (strcmp(key, "clean_db_on_boot") == 0)
            config_.system.clean_db_on_boot = ParseBool(value);
        else if (strcmp(key, "p2p_task") == 0) config_.system.p2p_task = (u8)atoi(value);
        else logger_.Write(FromConfigParser, LogWarning, "Unknown system config key: '%s'", key);
    }
    // unknown section
    else {
        logger_.Write(FromConfigParser, LogWarning, "Unknown config section: '%s'", section);
    }
}

void ConfigParser::Trim(char *str)
{
    if (!str) return;

    // leading
    int start = 0;
    while (str[start] == ' ' || str[start] == '\t')
        ++start;
    if (start > 0) memmove(str, str + start, strlen(str) - start + 1);

    // trailing
    char *end = str + strlen(str) - 1;
    while (end >= str) {
        if (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') {
            *end = '\0';
            --end;
        } else {
            break;
        }
    }
}

void ConfigParser::StripComment(char *str)
{
    if (!str) return;

    bool inQuotes = false;
    for (char *p = str; *p; ++p) {
        if (*p == '"') inQuotes = !inQuotes;
        if (!inQuotes && (*p == ';' || *p == '#')) {
            *p = '\0';
            return;
        }
    }
}

bool ConfigParser::ParseBool(const char *value)
{ return strcmp(value, "true") == 0 || strcmp(value, "1") == 0 || strcmp(value, "yes") == 0; }

void ConfigParser::DumpConfig()
{
    logger_.Write(FromConfigParser,
                  LogDebug,
                  "network.mode = %s",
                  config_.network.mode == NetworkMode::Auto   ? "auto"
                  : config_.network.mode == NetworkMode::Wifi ? "wifi"
                                                              : "ethernet");
    logger_.Write(
        FromConfigParser, LogDebug, "network.dhcp = %s", config_.network.dhcp ? "true" : "false");
    if (!config_.network.dhcp) {
        logger_.Write(FromConfigParser,
                      LogDebug,
                      "network.ip = %u.%u.%u.%u",
                      config_.network.ip[0],
                      config_.network.ip[1],
                      config_.network.ip[2],
                      config_.network.ip[3]);
        logger_.Write(FromConfigParser,
                      LogDebug,
                      "network.netmask = %u.%u.%u.%u",
                      config_.network.netmask[0],
                      config_.network.netmask[1],
                      config_.network.netmask[2],
                      config_.network.netmask[3]);
        logger_.Write(FromConfigParser,
                      LogDebug,
                      "network.gateway = %u.%u.%u.%u",
                      config_.network.gateway[0],
                      config_.network.gateway[1],
                      config_.network.gateway[2],
                      config_.network.gateway[3]);
        logger_.Write(FromConfigParser,
                      LogDebug,
                      "network.dns = %u.%u.%u.%u",
                      config_.network.dns[0],
                      config_.network.dns[1],
                      config_.network.dns[2],
                      config_.network.dns[3]);
    }
    logger_.Write(FromConfigParser, LogDebug, "http.port = %u", config_.http.port);
    logger_.Write(
        FromConfigParser, LogDebug, "http.max_content_size = %u", config_.http.max_content_size);
    logger_.Write(
        FromConfigParser, LogDebug, "http.timeout_seconds = %u", config_.http.timeout_seconds);
    logger_.Write(FromConfigParser, LogDebug, "http.max_clients = %u", config_.http.max_clients);
    logger_.Write(FromConfigParser,
                  LogDebug,
                  "cse.resource_name = %s",
                  (const char *)config_.cse.resource_name);
    logger_.Write(
        FromConfigParser, LogDebug, "cse.resource_id = %s", (const char *)config_.cse.resource_id);
    logger_.Write(FromConfigParser, LogDebug, "cse.cse_id = %s", (const char *)config_.cse.cse_id);
    logger_.Write(FromConfigParser, LogDebug, "cse.sp_id = %s", (const char *)config_.cse.sp_id);
    logger_.Write(
        FromConfigParser, LogDebug, "system.hostname = %s", (const char *)config_.system.hostname);
    logger_.Write(FromConfigParser,
                  LogDebug,
                  "system.clean_db_on_boot = %s",
                  config_.system.clean_db_on_boot ? "true" : "false");
    logger_.Write(FromConfigParser, LogDebug, "system.p2p_task = %u", config_.system.p2p_task);
}

bool ConfigParser::ParseIPv4(const char *value, u8 out[4])
{
    size_t part  = 0;
    int    octet = 0;

    for (size_t i = 0; i < strlen(value); i++) {
        char c = value[i];

        if (c >= '0' && c <= '9') {
            octet = octet * 10 + (c - '0');

            if (octet > 255) return false;
        } else if (c == '.') {
            if (part >= 3) return false;

            out[part++] = (u8)octet;
            octet       = 0;
        } else {
            return false;
        }
    }

    if (part != 3) return false;

    out[3] = (u8)octet;
    return true;
}

} // namespace zerom2m::config
