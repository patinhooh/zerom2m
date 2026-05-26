/*
 * http_adapter.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "headers.h"

#include <zerom2m/compat/types.h>
#include <zerom2m/onem2m/bindings/http/http_adapter.h>
#include <zerom2m/onem2m/onem2m_service.h>
#include <zerom2m/onem2m/types/enums.h>
#include <zerom2m/onem2m/types/short_names.h>
#include <zerom2m/serde/serde.h>

#include <circle/logger.h>

namespace zerom2m::onem2m::bindings::http
{

using namespace zerom2m::http;
using namespace zerom2m::onem2m::types;
using namespace zerom2m::compat;
using zerom2m::serde::SerDe;

HttpResponse HttpAdapter::HandleRequest(const HttpRequest &req)
{
    CString path = StringViewToCString(req.Path);
    CLogger::Get()->Write(
        "http_adapter", LogNotice, "HTTP request method=%d path=%s", (int)req.Method, path.c_str());

    RequestPrimitive prim = decodeRequest(req);

    CString errMsg;
    if (!isValid(prim, errMsg)) {
        CLogger::Get()->Write("http_adapter", LogWarning, "Invalid primitive: %s", errMsg.c_str());
        ResponsePrimitive errResp = makeResponse(prim, ResponseStatusCode::BadRequest);
        return encodeResponse(errResp, mime::JSON);
    }

    CString mimeType = req.GetHeaderValue(ACCEPT);
    if (mimeType.GetLength() == 0) mimeType = req.GetHeaderValue(CONTENT_TYPE);
    if (mimeType.GetLength() == 0) mimeType = mime::JSON;

    ResponsePrimitive resp = OneM2MService::Get().HandleRequest(prim);
    HttpResponse      out  = encodeResponse(resp, mimeType);

    CLogger::Get()->Write("http_adapter",
                          LogNotice,
                          "Responding rsc=%u http=%d",
                          (unsigned)resp.responseStatusCode,
                          (int)out.Status);

    return out;
}

ParsedContentType HttpAdapter::parseContentType(const CString &ct)
{
    ParsedContentType result;
    const char       *raw = ct.c_str();
    if (raw == nullptr || *raw == '\0') { return result; }

    const char  *semi    = strchr(raw, ';');
    const size_t mimeLen = semi == nullptr ? strlen(raw) : static_cast<size_t>(semi - raw);

    size_t trimmedLen = mimeLen;
    while (trimmedLen > 0 && raw[trimmedLen - 1] == ' ') {
        --trimmedLen;
    }

    for (size_t i = 0; i < trimmedLen; ++i) {
        result.mimeType += raw[i];
    }

    if (semi == nullptr) { return result; }

    const char *params = semi + 1;
    const char *tyPos  = strstr(params, "ty=");
    if (tyPos != nullptr) {
        char         *end    = nullptr;
        unsigned long parsed = strtoul(tyPos + 3, &end, 10);
        if (end != tyPos + 3 && (*end == '\0' || *end == ' ')) {
            result.ty = static_cast<ResourceType>(static_cast<long>(parsed));
        }
    }
    return result;
}
void HttpAdapter::decodeRequestHeaders(const HttpRequest &r, RequestPrimitive &prim)
{
    auto fetchHeader = [&](const char *name) -> Optional<CString> {
        CString value = r.GetHeaderValue(name);
        if (value.GetLength() == 0) { return Optional<CString>(); }
        return Optional<CString>(value);
    };

    auto fetchBoolHeader = [&](const char *name) -> Optional<boolean> {
        CString value = r.GetHeaderValue(name);
        if (value.GetLength() == 0) { return Optional<boolean>(); }
        return Optional<boolean>(value.Compare("1") == 0 || value.Compare("true") == 0);
    };

    auto fetchIntHeader = [&](const char *name) -> Optional<int> {
        CString value = r.GetHeaderValue(name);
        if (value.GetLength() == 0) { return Optional<int>(); }

        char         *end    = nullptr;
        unsigned long parsed = strtoul(value.c_str(), &end, 10);
        if (end == value.c_str() || *end != '\0') { return Optional<int>(); }

        return Optional<int>(static_cast<int>(parsed));
    };

    prim.from              = r.GetHeaderValue(ORIGIN);
    prim.requestIdentifier = r.GetHeaderValue(REQUEST_ID);

    prim.releaseVersionIndicator    = fetchHeader(RELEASE_VERSION);
    prim.originatingTimestamp       = fetchHeader(ORIG_TIMESTAMP);
    prim.requestExpirationTimestamp = fetchHeader(REQ_EXP_TS);
    prim.resultExpirationTimestamp  = fetchHeader(RES_EXP_TS);
    prim.operationExecutionTime     = fetchHeader(OP_EXEC_TIME);
    prim.resultPersistence          = fetchHeader(RESULT_PERSIST);
    prim.groupRequestIdentifier     = fetchHeader(GROUP_REQ_ID);
    prim.vendorInformation          = fetchHeader(VENDOR_INFO);
    prim.m2mServiceUser             = fetchHeader(SERVICE_USER);
    prim.primitiveProfileIdentifier = fetchHeader(PRIM_PROFILE);
    prim.tokens                     = fetchHeader(TOKENS);
    prim.tokenIDs                   = fetchHeader(TOKEN_IDS);

    prim.deliveryAggregation    = fetchBoolHeader(DELIV_AGGR);
    prim.authorSignIndicator    = fetchBoolHeader(AUTH_SIGN_IND);
    prim.semanticQueryIndicator = fetchBoolHeader(SEM_QUERY_IND);

    auto rcnVal = fetchIntHeader(RESULT_CONTENT);
    if (rcnVal) prim.resultContent = static_cast<ResultContent>(*rcnVal);

    auto rtVal = fetchIntHeader(RESPONSE_TYPE);
    if (rtVal) {
        ResponseTypeInfo rti;
        rti.responseTypeValue = static_cast<ResponseType>(*rtVal);
        prim.responseType     = rti;
    }

    auto ecVal = fetchIntHeader(EVENT_CATEGORY);
    if (ecVal) prim.eventCategory = static_cast<u8>(*ecVal);

    // Resource type: from Content-Type ;ty= OR X-M2M-TY header
    CString ct = r.GetHeaderValue(CONTENT_TYPE);
    if (ct.GetLength() != 0) {
        auto parsed = parseContentType(ct);
        if (parsed.ty) prim.resourceType = parsed.ty;
    }
    if (!prim.resourceType) {
        auto tyHdr = fetchIntHeader(RESOURCE_TYPE);
        if (tyHdr) prim.resourceType = static_cast<ResourceType>(*tyHdr);
    }
}

void HttpAdapter::decodeQueryParams(const HttpRequest &r, RequestPrimitive &prim)
{
    CString rcn = r.GetQueryParamValue(sn::prim::RESULT_CONTENT);
    if (rcn.GetLength() != 0) {
        char         *end    = nullptr;
        unsigned long parsed = strtoul(rcn.c_str(), &end, 10);
        if (end != rcn.c_str() && (*end == '\0' || *end == ' ')) {
            prim.resultContent = static_cast<ResultContent>(static_cast<long>(parsed));
        }
    }

    CString drt = r.GetQueryParamValue(sn::prim::DESIRED_IDENTIFIER_RESULT_TYPE);
    if (drt.GetLength() != 0) {
        char         *end    = nullptr;
        unsigned long parsed = strtoul(drt.c_str(), &end, 10);
        if (end != drt.c_str() && (*end == '\0' || *end == ' ')) {
            prim.desiredIdentifierResultType =
                static_cast<DesiredIdentifierResultType>(static_cast<long>(parsed));
        }
    }
}

RequestPrimitive HttpAdapter::decodeRequest(const HttpRequest &r)
{
    RequestPrimitive prim;

    decodeRequestHeaders(r, prim);
    prim.to = StringViewToCString(r.Path);

    decodeQueryParams(r, prim);

    Operation op = methodToOperation(r.Method, prim.resourceType.has_value());
    if (op == Operation::Unsupported) op = Operation::Retrieve;
    prim.op = op;

    if (prim.op == Operation::Retrieve) {
        // Build FilterCriteria from request query params (if any filter keys present)
        FilterCriteria fc    = filterCriteriaFromQuery(r);
        boolean        hasFc = fc.filterUsage.has_value() || !fc.resourceType.empty() ||
                               !fc.labels.empty() || fc.limit.has_value() ||
                               fc.createdBefore.has_value() || fc.createdAfter.has_value() ||
                               fc.modifiedSince.has_value() || fc.contentFilterQuery.has_value();
        if (hasFc) prim.filterCriteria = fc;
    }

    // ---- Body -> PrimitiveContent --------------------------------------------
    if (r.Body != nullptr && r.BodyLength > 0) {
        StringView bodyView{reinterpret_cast<const char *>(r.Body), r.BodyLength};
        CString    body        = StringViewToCString(bodyView);
        CString    contentType = r.GetHeaderValue(CONTENT_TYPE);
        SerDe::Get().DeserializeRequestBody(body, contentType, prim);
    }

    return prim;
}

HttpResponse HttpAdapter::encodeResponse(const ResponsePrimitive &rsp, const CString &contentType)
{
    HttpResponse out;

    // HTTP status
    out.Status = rscToHttpStatus(rsp.responseStatusCode);

    // Add mandatory oneM2M response headers
    CString rscStr;
    rscStr.Format("%u", static_cast<unsigned>(rsp.responseStatusCode));
    out.AddHeader(RSC, rscStr);
    out.AddHeader(REQUEST_ID, rsp.requestIdentifier);
    out.AddHeader(ORIGIN, rsp.from);

    // Add optional metadata headers
    if (rsp.releaseVersionIndicator.has_value())
        out.AddHeader(RELEASE_VERSION, *rsp.releaseVersionIndicator);
    if (rsp.originatingTimestamp.has_value())
        out.AddHeader(ORIG_TIMESTAMP, *rsp.originatingTimestamp);
    if (rsp.resultExpirationTimestamp.has_value())
        out.AddHeader(RES_EXP_TS, *rsp.resultExpirationTimestamp);
    if (rsp.vendorInformation.has_value()) out.AddHeader(VENDOR_INFO, *rsp.vendorInformation);
    if (rsp.m2mServiceUser.has_value()) out.AddHeader(SERVICE_USER, *rsp.m2mServiceUser);
    if (rsp.assignedTokenIdentifiers.has_value())
        out.AddHeader(ASSIGNED_TOKENS, *rsp.assignedTokenIdentifiers);
    if (rsp.tokenRequestInformation.has_value())
        out.AddHeader(TOKEN_REQ_INFO, *rsp.tokenRequestInformation);

    // Handle specific RESTful location behaviors
    if (rsp.responseStatusCode == ResponseStatusCode::Created && rsp.to.GetLength() > 0) {
        out.AddHeader(CONTENT_LOCATION, rsp.to);
    }

    // Short-circuit if there is no body payload
    if (rsp.content.empty()) { return out; }

    // Serialize PrimitiveContent
    CString body;
    if (!SerDe::Get().SerializePrimitiveContent(rsp.content, contentType, body)) {
        CLogger::Get()->Write("http_adapter",
                              LogError,
                              "Failed to serialize response content for rsc=%u",
                              static_cast<unsigned>(rsp.responseStatusCode));

        out.Status = ResponseStatus::InternalServerError;
        out.ClearBody();
        return out;
    }

    if (body.GetLength() > 0) {
        out.SetBody(body);
        out.AddHeader(CONTENT_TYPE, contentType);
    }

    return out;
}

FilterCriteria HttpAdapter::filterCriteriaFromQuery(const HttpRequest &r)
{
    FilterCriteria fc;

    auto getStr = [&](const char *k) -> Optional<CString> {
        CString v = r.GetQueryParamValue(k);
        if (v.GetLength() == 0) return Optional<CString>();
        return Optional<CString>(v);
    };

    auto getInt32 = [&](const char *k) -> Optional<s32> {
        CString v = r.GetQueryParamValue(k);
        if (v.GetLength() == 0) return Optional<s32>();
        char         *end = nullptr;
        unsigned long p   = strtoul(v.c_str(), &end, 10);
        if (end == v.c_str()) return Optional<s32>();
        return Optional<s32>(static_cast<s32>(p));
    };

    auto getInt64 = [&](const char *k) -> Optional<s64> {
        CString v = r.GetQueryParamValue(k);
        if (v.GetLength() == 0) return Optional<s64>();
        char         *end = nullptr;
        unsigned long p   = strtoul(v.c_str(), &end, 10);
        if (end == v.c_str()) return Optional<s64>();
        return Optional<s64>(static_cast<s64>(p));
    };

    fc.createdBefore       = getStr("crb");
    fc.createdAfter        = getStr("cra");
    fc.modifiedSince       = getStr("ms");
    fc.unmodifiedSince     = getStr("us");
    fc.stateTagSmaller     = getInt32("sts");
    fc.stateTagBigger      = getInt32("stb");
    fc.expireBefore        = getStr("exb");
    fc.expireAfter         = getStr("exa");
    fc.labelsQuery         = getStr("lbq");
    fc.sizeAbove           = getInt64("sza");
    fc.sizeBelow           = getInt64("szb");
    fc.contentType         = getStr("cty");
    fc.limit               = getInt32("lim");
    fc.level               = getInt32("lvl");
    fc.offset              = getInt32("ofst");
    fc.contentFilterSyntax = getStr("cfs");
    fc.contentFilterQuery  = getStr("cfq");
    fc.semanticsFilter     = getStr("smf");

    // multi-value keys
    for (size_t i = 0; i < r.QueryParamCount; ++i) {
        const QueryParam &p    = r.QueryParams[i];
        CString           name = StringViewToCString(p.Name);
        CString           val  = StringViewToCString(p.Value);
        if (name == sn::attr::LABELS) fc.labels.push_back(val);
        if (name == sn::attr::RESOURCE_TYPE) {
            char         *end = nullptr;
            unsigned long v   = strtoul(val.c_str(), &end, 10);
            if (end != val.c_str())
                fc.resourceType.push_back(static_cast<ResourceType>(static_cast<long>(v)));
        }
    }

    auto fu = r.GetQueryParamValue(sn::dt::fc::FILTER_USAGE);
    if (fu.GetLength() != 0) {
        char         *end = nullptr;
        unsigned long v   = strtoul(fu.c_str(), &end, 10);
        if (end != fu.c_str()) fc.filterUsage = static_cast<FilterUsage>(static_cast<long>(v));
    }

    auto fo = r.GetQueryParamValue(sn::dt::fc::FILTER_OPERATION);
    if (fo.GetLength() != 0) {
        char         *end = nullptr;
        unsigned long v   = strtoul(fo.c_str(), &end, 10);
        if (end != fo.c_str()) fc.filterOperation = static_cast<u8>(static_cast<unsigned>(v));
    }

    auto chty = r.GetQueryParamValue("chty");
    if (chty.GetLength() != 0) {
        char         *end = nullptr;
        unsigned long v   = strtoul(chty.c_str(), &end, 10);
        if (end != chty.c_str())
            fc.childResourceType = static_cast<ResourceType>(static_cast<long>(v));
    }

    return fc;
}

RequestMethod HttpAdapter::operationToMethod(Operation op)
{
    switch (op) {
        case Operation::Create:
            return RequestMethod::POST;
        case Operation::Retrieve:
            return RequestMethod::GET;
        case Operation::Notify:
            return RequestMethod::POST;
        // XXX: we only support Create/Retrieve/Notify for now. PUT/DELETE are out of scope of
        // this project.
        // case Operation::Update:
        //     return RequestMethod::PUT;
        // case Operation::Delete:
        //     return RequestMethod::DELETE;
        default:
            return RequestMethod::RequestMethodUnknown;
    }
}

Operation HttpAdapter::methodToOperation(RequestMethod method, boolean hasResourceType)
{
    switch (method) {
        case RequestMethod::POST:
            return hasResourceType ? Operation::Create : Operation::Notify;
        case RequestMethod::GET:
            return Operation::Retrieve;
        // XXX: we only support Create/Retrieve/Notify for now. PUT/DELETE are out of scope of this
        // project.
        // case RequestMethod::PUT:
        //     return Operation::Update;
        // case RequestMethod::DELETE:
        //     return Operation::Delete;
        default:
            return Operation::Unsupported;
    }
}

ResponseStatus HttpAdapter::rscToHttpStatus(ResponseStatusCode rsc)
{
    switch (rsc) {
        // 2xxx - success
        case ResponseStatusCode::OK:
            return ResponseStatus::OK;
        case ResponseStatusCode::Created:
            return ResponseStatus::Created;
        case ResponseStatusCode::Deleted:
            return ResponseStatus::OK;
        case ResponseStatusCode::Updated:
            return ResponseStatus::OK;

        // 4xxx - client errors
        case ResponseStatusCode::BadRequest:
            return ResponseStatus::BadRequest;
        case ResponseStatusCode::NotPermitted:
            return ResponseStatus::Forbidden;
        case ResponseStatusCode::NotFound:
            return ResponseStatus::NotFound;
        case ResponseStatusCode::OperationNotAllowed:
            return ResponseStatus::MethodNotAllowed;
        case ResponseStatusCode::RequestTimeout:
            return ResponseStatus::RequestTimeout;
        case ResponseStatusCode::Unsupported:
            return ResponseStatus::UnsupportedMediaType;
        case ResponseStatusCode::GroupMemberTypeInconsistent:
            return ResponseStatus::BadRequest;

        // 5xxx - server / conflict errors
        case ResponseStatusCode::InternalServerError:
            return ResponseStatus::InternalServerError;
        case ResponseStatusCode::NotImplemented:
            return ResponseStatus::NotImplemented;
        case ResponseStatusCode::TargetNotReachable:
            return ResponseStatus::ServiceUnavailable;
        case ResponseStatusCode::NoPrivilege:
            return ResponseStatus::Forbidden;
        case ResponseStatusCode::AlreadyExists:
            return ResponseStatus::Conflict;
        case ResponseStatusCode::TargetNotSubscribable:
            return ResponseStatus::UnprocessableContent;
        case ResponseStatusCode::SubscriptionVerificationInitiated:
            return ResponseStatus::OK;
        case ResponseStatusCode::MaxNrOfChildresourcesExceeded:
            return ResponseStatus::BadRequest;
        case ResponseStatusCode::MaxNrOfMemberExceeded:
            return ResponseStatus::BadRequest;
        case ResponseStatusCode::FilterCriteriaNotImplemented:
            return ResponseStatus::BadRequest;
        default:
            return ResponseStatus::InternalServerError;
    }
}

ResponseStatusCode HttpAdapter::httpStatusToRsc(ResponseStatus httpStatus)
{
    // The real RSC always comes from X-M2M-RSC; HTTP status is secondary.
    // This helper is a best-effort fallback when the header is absent.
    switch (httpStatus) {
        case ResponseStatus::OK:
            return ResponseStatusCode::OK;
        case ResponseStatus::Created:
            return ResponseStatusCode::Created;
        case ResponseStatus::BadRequest:
            return ResponseStatusCode::BadRequest;
        case ResponseStatus::Forbidden:
            return ResponseStatusCode::NoPrivilege;
        case ResponseStatus::NotFound:
            return ResponseStatusCode::NotFound;
        case ResponseStatus::MethodNotAllowed:
            return ResponseStatusCode::OperationNotAllowed;
        case ResponseStatus::Conflict:
            return ResponseStatusCode::AlreadyExists;
        case ResponseStatus::InternalServerError:
            return ResponseStatusCode::InternalServerError;
        case ResponseStatus::NotImplemented:
            return ResponseStatusCode::NotImplemented;
        case ResponseStatus::ServiceUnavailable:
            return ResponseStatusCode::TargetNotReachable;
        default:
            return ResponseStatusCode::UnknownStatus;
    }
}

// TODO: this would be neede for notifications if we implement client-side calls. For now it's
// unused.
HttpRequest HttpAdapter::encodeRequest(const RequestPrimitive &prim,
                                       const CString          &baseUrl,
                                       const CString          &acceptType)
{
    // TODO: implement outbound request encoding when client-side calls are needed
    (void)prim;
    (void)baseUrl;
    (void)acceptType;
    return HttpRequest{};
}
ResponsePrimitive HttpAdapter::decodeResponse(const HttpResponse &h,
                                              const CString      &requestIdentifier)
{
    // TODO: implement inbound response decoding when client-side calls are needed
    (void)h;
    (void)requestIdentifier;
    return ResponsePrimitive{};
}

} // namespace zerom2m::onem2m::bindings::http
