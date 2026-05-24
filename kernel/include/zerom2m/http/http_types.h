/*
 * http_types.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include "zerom2m/compat/string_view.h"
#include "zerom2m/http/http_common.h"

#include <circle/string.h>
#include <circle/types.h>
#include <circle/util.h>

namespace zerom2m::http
{

using StringView = zerom2m::compat::StringView;

/**
 * @brief Name/value pair representing a single HTTP header.
 */
struct HttpHeader {
    StringView Name;
    StringView Value;
};

/**
 * @brief Name/value pair representing a single HTTP query parameter.
 */
struct QueryParam {
    StringView Name;
    StringView Value;
};

struct HttpRequest {
    RequestMethod Method{RequestMethod::RequestMethodUnknown};

    StringView Target;
    StringView Path;
    StringView Query;

    StringView Version;

    const HttpHeader *Headers{nullptr};
    size_t            HeaderCount{0};

    const QueryParam *QueryParams{nullptr};
    size_t            QueryParamCount{0};

    const u8 *Body{nullptr};
    size_t    BodyLength{0};

    // Header helpers
    const HttpHeader *FindHeader(const char *name) const
    {
        if (Headers == nullptr || name == nullptr) { return nullptr; }

        const size_t want = strlen(name);

        for (size_t i = 0; i < HeaderCount; ++i) {
            const auto &h = Headers[i];

            if (h.Name.Data == nullptr) { continue; }

            if (h.Name.Length == want && strncasecmp(h.Name.Data, name, want) == 0) { return &h; }
        }

        return nullptr;
    }

    CString GetHeaderValue(const char *name) const
    {
        const HttpHeader *h = FindHeader(name);

        if (h == nullptr || h->Value.Data == nullptr) { return CString{}; }

        return StringViewToCString(h->Value);
    }

    // Query helpers
    const QueryParam *FindQueryParam(const char *name) const
    {
        if (QueryParams == nullptr || name == nullptr) { return nullptr; }

        const size_t want = strlen(name);

        for (size_t i = 0; i < QueryParamCount; ++i) {
            const auto &q = QueryParams[i];

            if (q.Name.Data == nullptr) { continue; }

            if (q.Name.Length == want && strncmp(q.Name.Data, name, want) == 0) { return &q; }
        }

        return nullptr;
    }

    CString GetQueryParamValue(const char *name) const
    {
        const QueryParam *q = FindQueryParam(name);

        if (q == nullptr || q->Value.Data == nullptr) { return CString{}; }

        return StringViewToCString(q->Value);
    }
};

/**
 * @brief HTTP response description returned by handlers.
 */
// At the top of the file or alongside MAX_CONTENT_SIZE:
static constexpr size_t MAX_RESPONSE_HEADERS = 16;

struct HttpResponse {
    ResponseStatus Status{ResponseStatus::OK};

    const HttpHeader *Headers{nullptr};
    size_t            HeaderCount{0};

    const u8 *Body{nullptr};
    size_t    BodyLength{0};
    char      BodyStorage[MAX_CONTENT_SIZE + 1]{};

    HttpHeader HeaderStorage[MAX_RESPONSE_HEADERS]{};
    size_t     OwnedHeaderCount{0};

    CString OwnedHeaderNames[MAX_RESPONSE_HEADERS]{};
    CString OwnedHeaderValues[MAX_RESPONSE_HEADERS]{};

    // -----------------------------------------------------------------------
    HttpResponse() = default;
    HttpResponse(const HttpResponse &other) { CopyFrom(other); }
    HttpResponse(HttpResponse &&other) noexcept { CopyFrom(other); }
    HttpResponse &operator=(const HttpResponse &other)
    {
        if (this != &other) { CopyFrom(other); }
        return *this;
    }
    HttpResponse &operator=(HttpResponse &&other) noexcept
    {
        if (this != &other) { CopyFrom(other); }
        return *this;
    }

    // -----------------------------------------------------------------------
    // Add or overwrite a header in owned storage.
    // Silently drops the header if storage is full.
    // -----------------------------------------------------------------------
    void AddHeader(const char *name, const char *value)
    {
        if (name == nullptr || value == nullptr) { return; }

        // Overwrite if name already present
        for (size_t i = 0; i < OwnedHeaderCount; ++i) {
            if (OwnedHeaderNames[i].Compare(name) == 0) {
                OwnedHeaderValues[i]   = value;
                HeaderStorage[i].Value = {OwnedHeaderValues[i].c_str(),
                                          OwnedHeaderValues[i].GetLength()};
                return;
            }
        }

        // Append
        if (OwnedHeaderCount >= MAX_RESPONSE_HEADERS) { return; }

        OwnedHeaderNames[OwnedHeaderCount]  = name;
        OwnedHeaderValues[OwnedHeaderCount] = value;

        HeaderStorage[OwnedHeaderCount].Name  = {OwnedHeaderNames[OwnedHeaderCount].c_str(),
                                                 OwnedHeaderNames[OwnedHeaderCount].GetLength()};
        HeaderStorage[OwnedHeaderCount].Value = {OwnedHeaderValues[OwnedHeaderCount].c_str(),
                                                 OwnedHeaderValues[OwnedHeaderCount].GetLength()};
        ++OwnedHeaderCount;

        Headers     = HeaderStorage;
        HeaderCount = OwnedHeaderCount;
    }

    void AddHeader(const char *name, const CString &value)
    { AddHeader(name, static_cast<const char *>(value)); }

    void AddHeader(const CString &name, const CString &value)
    { AddHeader(static_cast<const char *>(name), static_cast<const char *>(value)); }

    // -----------------------------------------------------------------------
    void ClearBody()
    {
        BodyStorage[0] = '\0';
        Body           = nullptr;
        BodyLength     = 0;
    }

    void SetBody(const char *data, size_t length)
    {
        if (data == nullptr || length == 0) {
            ClearBody();
            return;
        }
        if (length > MAX_CONTENT_SIZE) { length = MAX_CONTENT_SIZE; }
        memcpy(BodyStorage, data, length);
        BodyStorage[length] = '\0';
        Body                = reinterpret_cast<const u8 *>(BodyStorage);
        BodyLength          = length;
    }

    void SetBody(const char *data) { SetBody(data, data != nullptr ? strlen(data) : 0); }

    void SetBody(const CString &data)
    { SetBody(static_cast<const char *>(data), data.GetLength()); }

private:
    void CopyFrom(const HttpResponse &other)
    {
        Status = other.Status;

        // Copy owned headers — re-point StringViews into our own CString backing
        // so they never dangle after the source is destroyed.
        OwnedHeaderCount = other.OwnedHeaderCount;
        for (size_t i = 0; i < OwnedHeaderCount; ++i) {
            OwnedHeaderNames[i]    = other.OwnedHeaderNames[i];
            OwnedHeaderValues[i]   = other.OwnedHeaderValues[i];
            HeaderStorage[i].Name  = {OwnedHeaderNames[i].c_str(), OwnedHeaderNames[i].GetLength()};
            HeaderStorage[i].Value = {OwnedHeaderValues[i].c_str(),
                                      OwnedHeaderValues[i].GetLength()};
        }

        if (OwnedHeaderCount > 0) {
            Headers     = HeaderStorage;
            HeaderCount = OwnedHeaderCount;
        } else {
            // Preserve any externally assigned pointer (existing behaviour)
            Headers     = other.Headers;
            HeaderCount = other.HeaderCount;
        }

        if (other.Body != nullptr && other.BodyLength > 0)
            SetBody(reinterpret_cast<const char *>(other.Body), other.BodyLength);
        else ClearBody();
    }
};

} // namespace zerom2m::http
