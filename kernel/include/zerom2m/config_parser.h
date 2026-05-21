/*
 * config_parser.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include "zerom2m/kernel_config.h"

#include <circle/logger.h>

namespace zerom2m
{

class ConfigParser
{
public:
    explicit ConfigParser(KernelConfig &config, CLogger &logger);

    bool Load(const char *path);
    void DumpConfig();

private:
    KernelConfig &config_;
    CLogger      &logger_;

    void ParseLine(const char *section, char *line);

    static void Trim(char *str);
    static void StripComment(char *str);

    static bool ParseBool(const char *value);
    static bool ParseIPv4(const char *value, u8 out[4]);
};

} // namespace zerom2m