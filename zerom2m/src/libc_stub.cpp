/*
 * libc_stub.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include <circle/util.h>
#include <zerom2m/libc_stub.h>

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = (const unsigned char *)s;

    while (n--) {
        if (*p == (unsigned char)c) return (void *)p;
        p++;
    }

    return 0;
}

size_t strspn(const char *s, const char *accept)
{
    size_t i = 0;

    while (s[i]) {
        const char *a     = accept;
        int         found = 0;

        while (*a) {
            if (s[i] == *a) {
                found = 1;
                break;
            }
            a++;
        }

        if (!found) break;

        i++;
    }

    return i;
}

size_t strcspn(const char *s, const char *reject)
{
    size_t i = 0;

    while (s[i]) {
        const char *r = reject;

        while (*r) {
            if (s[i] == *r) return i;
            r++;
        }

        i++;
    }

    return i;
}

char *strrchr(const char *s, int c)
{
    const char *last = 0;

    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }

    return (char *)last;
}