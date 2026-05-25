/*
 * config.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <circle/types.h>

#ifndef COMMIT_HASH
#define COMMIT_HASH "unknown"
#endif

#define SERVER_NAME "ZeroM2M/" COMMIT_HASH

namespace zerom2m::http
{

static constexpr unsigned DEFAULT_PORT         = 80u;
static constexpr unsigned MAX_CONTENT_SIZE     = 2048u;
static constexpr unsigned MAX_CLIENTS          = 10u;
static constexpr unsigned TIMEOUT_SECONDS      = 10u;
static constexpr size_t   MAX_RESPONSE_HEADERS = 16;

} // namespace zerom2m::http