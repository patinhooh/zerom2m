/*
 * libc_stub.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <circle/types.h>

extern "C" void  *memchr(const void *s, int c, size_t n);
extern "C" size_t strspn(const char *s, const char *accept);
extern "C" size_t strcspn(const char *s, const char *reject);
extern "C" char  *strrchr(const char *s, int c);