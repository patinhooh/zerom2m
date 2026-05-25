/*
 * index_handler.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include "zerom2m/kernel_config.h"

#include <zerom2m/http/http_handler.h>

namespace zerom2m::handlers
{

class IndexHandler : public http::IHttpHandler
{
public:
    explicit IndexHandler(const KernelConfig *cfg);
    http::HttpResponse HandleRequest(const http::HttpRequest &request) override;

private:
    const KernelConfig *cfg_;
};

} // namespace zerom2m::handlers