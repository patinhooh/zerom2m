/*
 * http_adapter.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <zerom2m/compat/optional.h>
#include <zerom2m/http/http_handler.h>
#include <zerom2m/http/types.h>
#include <zerom2m/onem2m/onem2m_service.h>
#include <zerom2m/onem2m/binding.h>
#include <zerom2m/onem2m/types/resources.h>
#include <zerom2m/serde/serde.h>

#include <circle/string.h>

namespace zerom2m::onem2m::bindings::http
{

using namespace zerom2m::http;
using namespace zerom2m::onem2m::types;

using zerom2m::compat::Optional;
using zerom2m::serde::SerDe;

struct ParsedContentType {
    CString                mimeType;
    Optional<ResourceType> ty;
};

class HttpAdapter : public IHttpHandler, public IBinding
{
public:
    HttpResponse HandleRequest(const HttpRequest &req) override;

    bool SendNotification(const RequestPrimitive &request, CNetSubSystem *net) override;
private:
    HttpRequest       encodeRequest(const RequestPrimitive &prim,
                                    const CString          &baseUrl    = "",
                                    const CString          &acceptType = mime::JSON);
    RequestPrimitive  decodeRequest(const HttpRequest &r);
    void              decodeRequestHeaders(const HttpRequest &r, RequestPrimitive &prim);
    ParsedContentType parseContentType(const CString &ct);
    void              decodeQueryParams(const HttpRequest &r, RequestPrimitive &prim);

    HttpResponse      encodeResponse(const ResponsePrimitive &rsp,
                                     const CString           &contentType = mime::JSON);
    ResponsePrimitive decodeResponse(const HttpResponse &h, const CString &requestIdentifier = "");

    FilterCriteria filterCriteriaFromQuery(const HttpRequest &r);

    RequestMethod operationToMethod(Operation op);
    Operation     methodToOperation(RequestMethod m, boolean hasResourceType = false);

    ResponseStatus     rscToHttpStatus(ResponseStatusCode rsc);
};

} // namespace zerom2m::onem2m::bindings::http
