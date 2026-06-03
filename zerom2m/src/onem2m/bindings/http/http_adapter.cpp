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
#include <zerom2m/http/http_client.h>
#include <zerom2m/onem2m/bindings/http/http_adapter.h>
#include <zerom2m/onem2m/onem2m_service.h>
#include <zerom2m/onem2m/types/enums.h>
#include <zerom2m/onem2m/types/short_names.h>
#include <zerom2m/onem2m/utils.h>
#include <zerom2m/serde/serde.h>

#include <circle/logger.h>

namespace zerom2m::onem2m::bindings::http
{

using namespace zerom2m::http;
using namespace zerom2m::onem2m::types;
using namespace zerom2m::compat;
using zerom2m::serde::SerDe;

namespace
{

CString SliceCString(const char *start, size_t length)
{
    char *buf = new char[length + 1];
    memcpy(buf, start, length);
    buf[length] = '\0';
    CString out(buf);
    delete[] buf;
    return out;
}

bool ParseIpv4Literal(const CString &text, CIPAddress &ip)
{
    const char *raw = text.c_str();
    if (raw == nullptr || *raw == '\0') return false;

    u8          octets[4] = {0, 0, 0, 0};
    const char *p         = raw;
    for (unsigned i = 0; i < 4; ++i) {
        if (*p == '\0') return false;

        char         *end   = nullptr;
        unsigned long value = strtoul(p, &end, 10);
        if (end == p || value > 255) return false;

        octets[i] = static_cast<u8>(value);
        if (i < 3) {
            if (*end != '.') return false;
            p = end + 1;
        } else if (*end != '\0') {
            return false;
        }
    }

    ip.Set(octets);
    return true;
}

bool ParseNotificationUrl(const CString &url, CIPAddress &ip, u16 &port, CString &path)
{
    const char *raw = url.c_str();
    if (raw == nullptr) return false;

    const char *scheme = strstr(raw, "://");
    if (scheme == nullptr) return false;

    const char *authority = scheme + 3;
    const char *slash     = strchr(authority, '/');
    const char *query     = nullptr;
    if (slash != nullptr) { query = strchr(slash, '?'); }

    CString hostPort;
    if (slash != nullptr) {
        hostPort = SliceCString(authority, static_cast<size_t>(slash - authority));
    } else {
        hostPort = authority;
    }

    CString     host;
    CString     portText;
    const char *colon = strchr(hostPort.c_str(), ':');
    if (colon != nullptr) {
        host     = SliceCString(hostPort.c_str(), static_cast<size_t>(colon - hostPort.c_str()));
        portText = CString(colon + 1);
    } else {
        host = hostPort;
    }

    if (!ParseIpv4Literal(host, ip)) return false;

    port = 80;
    if (portText.GetLength() != 0) {
        char         *end    = nullptr;
        unsigned long parsed = strtoul(portText.c_str(), &end, 10);
        if (end == portText.c_str() || *end != '\0' || parsed > 65535) return false;
        port = static_cast<u16>(parsed);
    }

    if (slash != nullptr) {
        if (query != nullptr) {
            path = SliceCString(slash, static_cast<size_t>(query - slash));
        } else {
            path = slash;
        }
    } else {
        path = "/";
    }

    return true;
}

} // namespace

HttpResponse HttpAdapter::HandleRequest(const HttpRequest &req)
{
    CString path = req.Path;

    RequestPrimitive prim = decodeRequest(req);

    CString errMsg;
    if (!isValidRequest(prim, errMsg)) {
        CLogger::Get()->Write("http_adapter", LogWarning, "Invalid primitive: %s", errMsg.c_str());
        ResponsePrimitive errResp = makeResponse(prim, ResponseStatusCode::BAD_REQUEST);
        return encodeResponse(errResp, mime::JSON);
    }

    CString mimeType = req.GetHeader("Accept");
    if (mimeType.GetLength() == 0) mimeType = req.GetHeader("Content-Type");
    if (mimeType.GetLength() == 0) mimeType = mime::JSON;

    ResponsePrimitive resp = OneM2MService::Get().HandleRequest(prim);
    HttpResponse      out  = encodeResponse(resp, mimeType);

    CLogger::Get()->Write("http_adapter",
                          LogNotice,
                          "Responding rsc=%u",
                          (unsigned)resp.responseStatusCode,
                          (int)out.Status);

    return out;
}

bool HttpAdapter::SendNotification(const RequestPrimitive &request, CNetSubSystem *net)
{

    CIPAddress ip;
    u16        port = 0;
    CString    path;
    ParseNotificationUrl(request.to, ip, port, path);
    // CLogger::Get()->Write("http_adapter",
    //                       LogNotice,
    //                       "Sending notification to %u.%u.%u.%u:%u%s",
    //                       ip.Get()[0],
    //                       ip.Get()[1],
    //                       ip.Get()[2],
    //                       ip.Get()[3],
    //                       port,
    //                       path.c_str());

    const HttpRequest req = encodeRequest(request);
    // CLogger::Get()->Write(
    //     "http_adapter", LogNotice, "Sending notification:\n %s", req.Body.c_str());

    if (net == nullptr) return false;
    auto client = HttpClient(net, ip, port, SERVER_NAME, 2);

    // XXX: Fire and forget for now, but we should handle failures and retries
    HttpResponse resp;
    client.Request(req, resp);
    if (resp.Status != ResponseStatus::OK && resp.Status != ResponseStatus::Created) {
        CLogger::Get()->Write("http_adapter",
                              LogWarning,
                              "Notification request failed with HTTP status %d",
                              (int)resp.Status);
        return false;
    }
    return true;
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
        CString value = r.GetHeader(name);
        if (value.GetLength() == 0) { return Optional<CString>(); }
        return Optional<CString>(value);
    };

    auto fetchBoolHeader = [&](const char *name) -> Optional<boolean> {
        CString value = r.GetHeader(name);
        if (value.GetLength() == 0) { return Optional<boolean>(); }
        return Optional<boolean>(value.Compare("1") == 0 || value.Compare("true") == 0);
    };

    auto fetchIntHeader = [&](const char *name) -> Optional<int> {
        CString value = r.GetHeader(name);
        if (value.GetLength() == 0) { return Optional<int>(); }

        char         *end    = nullptr;
        unsigned long parsed = strtoul(value.c_str(), &end, 10);
        if (end == value.c_str() || *end != '\0') { return Optional<int>(); }

        return Optional<int>(static_cast<int>(parsed));
    };

    prim.from              = r.GetHeader(ORIGIN);
    prim.requestIdentifier = r.GetHeader(REQUEST_ID);

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
    CString ct = r.GetHeader(CONTENT_TYPE);
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
    CString rcn = r.GetQuery(sn::prim::RESULT_CONTENT);
    if (rcn.GetLength() != 0) {
        char         *end    = nullptr;
        unsigned long parsed = strtoul(rcn.c_str(), &end, 10);
        if (end != rcn.c_str() && (*end == '\0' || *end == ' ')) {
            prim.resultContent = static_cast<ResultContent>(static_cast<long>(parsed));
        }
    }

    CString drt = r.GetQuery(sn::prim::DESIRED_IDENTIFIER_RESULT_TYPE);
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
    prim.to = r.Path;

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
    if (r.Body.GetLength() > 0) {
        CString body        = r.Body;
        CString contentType = r.GetHeader(CONTENT_TYPE);
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
    if (rsp.responseStatusCode == ResponseStatusCode::CREATED && rsp.to.GetLength() > 0) {
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
        out.Body   = "";
        return out;
    }

    if (body.GetLength() > 0) {
        out.Body = body;
        out.AddHeader(CONTENT_TYPE, contentType);
    }

    return out;
}

FilterCriteria HttpAdapter::filterCriteriaFromQuery(const HttpRequest &r)
{
    FilterCriteria fc;

    auto getStr = [&](const char *k) -> Optional<CString> {
        CString v = r.GetQuery(k);
        if (v.GetLength() == 0) return Optional<CString>();
        return Optional<CString>(v);
    };

    auto getInt32 = [&](const char *k) -> Optional<s32> {
        CString v = r.GetQuery(k);
        if (v.GetLength() == 0) return Optional<s32>();
        char         *end = nullptr;
        unsigned long p   = strtoul(v.c_str(), &end, 10);
        if (end == v.c_str()) return Optional<s32>();
        return Optional<s32>(static_cast<s32>(p));
    };

    auto getInt64 = [&](const char *k) -> Optional<s64> {
        CString v = r.GetQuery(k);
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
    for (size_t i = 0; i < r.QueryNames.GetCount(); ++i) {
        CString name = r.QueryNames[i];
        CString val  = r.QueryValues[i];
        if (name == sn::attr::LABELS) fc.labels.push_back(val);
        if (name == sn::attr::RESOURCE_TYPE) {
            char         *end = nullptr;
            unsigned long v   = strtoul(val.c_str(), &end, 10);
            if (end != val.c_str())
                fc.resourceType.push_back(static_cast<ResourceType>(static_cast<long>(v)));
        }
    }

    auto fu = r.GetQuery(sn::dt::fc::FILTER_USAGE);
    if (fu.GetLength() != 0) {
        char         *end = nullptr;
        unsigned long v   = strtoul(fu.c_str(), &end, 10);
        if (end != fu.c_str()) fc.filterUsage = static_cast<FilterUsage>(static_cast<long>(v));
    }

    auto fo = r.GetQuery(sn::dt::fc::FILTER_OPERATION);
    if (fo.GetLength() != 0) {
        char         *end = nullptr;
        unsigned long v   = strtoul(fo.c_str(), &end, 10);
        if (end != fo.c_str()) fc.filterOperation = static_cast<u8>(static_cast<unsigned>(v));
    }

    auto chty = r.GetQuery("chty");
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
        case Operation::Update:
            return RequestMethod::PUT;
        case Operation::Delete:
            return RequestMethod::DELETE;
        case Operation::Notify:
            return RequestMethod::POST;
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
        case RequestMethod::PUT:
            return Operation::Update;
        case RequestMethod::DELETE:
            return Operation::Delete;
        default:
            return Operation::Unsupported;
    }
}

ResponseStatus HttpAdapter::rscToHttpStatus(ResponseStatusCode rsc)
{
    switch (rsc) {
        case ResponseStatusCode::OK:
        case ResponseStatusCode::DELETED:
        case ResponseStatusCode::UPDATED:
            return ResponseStatus::OK;

        case ResponseStatusCode::CREATED:
            return ResponseStatus::Created;

        case ResponseStatusCode::ACCEPTED:
        case ResponseStatusCode::ACCEPTED_NON_BLOCKING_REQUEST_SYNCH:
        case ResponseStatusCode::ACCEPTED_NON_BLOCKING_REQUEST_ASYNCH:
            return ResponseStatus::Accepted;

        case ResponseStatusCode::BAD_REQUEST:
        case ResponseStatusCode::CONTENTS_UNACCEPTABLE:
        case ResponseStatusCode::GROUP_MEMBER_TYPE_INCONSISTENT:
        case ResponseStatusCode::INVALID_SEMANTICS:
        case ResponseStatusCode::INVALID_TRIGGER_PURPOSE:
        case ResponseStatusCode::ILLEGAL_TRANSACTION_STATE_TRANSITION_ATTEMPTED:
        case ResponseStatusCode::ONTOLOGY_MAPPING_POLICY_NOT_MATCHED:
        case ResponseStatusCode::BAD_FACT_INPUTS_FOR_REASONING:
        case ResponseStatusCode::BAD_RULE_INPUTS_FOR_REASONING:
        case ResponseStatusCode::PRIMITIVE_PROFILE_BAD_REQUEST:
        case ResponseStatusCode::INVALID_PROCESS_CONFIGURATION:
        case ResponseStatusCode::INVALID_SPARQL_QUERY:
        case ResponseStatusCode::MAX_NUMBER_OF_MEMBER_EXCEEDED:
        case ResponseStatusCode::INVALID_CMDTYPE:
        case ResponseStatusCode::INVALID_ARGUMENTS:
        case ResponseStatusCode::INSUFFICIENT_ARGUMENTS:
            return ResponseStatus::BadRequest;

        case ResponseStatusCode::SUBSCRIPTION_CREATOR_HAS_NO_PRIVILEGE:
        case ResponseStatusCode::ORIGINATOR_HAS_NO_PRIVILEGE:
        case ResponseStatusCode::RECEIVER_HAS_NO_PRIVILEGE:
        case ResponseStatusCode::TARGET_NOT_SUBSCRIBABLE:
        case ResponseStatusCode::SUBSCRIPTION_HOST_HAS_NO_PRIVILEGE:
        case ResponseStatusCode::ORIGINATOR_NOT_AUTHENTICATED:
        case ResponseStatusCode::SECURITY_ASSOCIATION_REQUIRED:
        case ResponseStatusCode::INVALID_CHILD_RESOURCE_TYPE:
        case ResponseStatusCode::NO_MEMBERS:
        case ResponseStatusCode::ESPRIM_UNSUPPORTED_OPTION:
        case ResponseStatusCode::ESPRIM_UNKNOWN_KEY_ID:
        case ResponseStatusCode::ESPRIM_UNKNOWN_ORIG_RAND_ID:
        case ResponseStatusCode::ESPRIM_UNKNOWN_RECV_RAND_ID:
        case ResponseStatusCode::ESPRIM_BAD_MAC:
        case ResponseStatusCode::ESPRIM_IMPERSONATION_ERROR:
        case ResponseStatusCode::ORIGINATOR_HAS_ALREADY_REGISTERED:
        case ResponseStatusCode::APP_RULE_VALIDATION_FAILED:
        case ResponseStatusCode::OPERATION_DENIED_BY_REMOTE_ENTITY:
        case ResponseStatusCode::SERVICE_SUBSCRIPTION_NOT_ESTABLISHED:
        case ResponseStatusCode::DISCOVERY_LIMIT_EXCEEDED:
        case ResponseStatusCode::PRIMITIVE_PROFILE_NOT_ACCESSIBLE:
        case ResponseStatusCode::UNAUTHORIZED_USER:
        case ResponseStatusCode::SERVICE_SUBSCRIPTION_NOT_ACTIVE:
        case ResponseStatusCode::DISCOVERY_DENIED_BY_IPE:
        case ResponseStatusCode::TARGET_HAS_NO_SESSION_CAPABILITY:
        case ResponseStatusCode::SESSION_IS_ONLINE:
        case ResponseStatusCode::TRIGGERING_DISABLED_FOR_RECIPIENT:
        case ResponseStatusCode::TRANSACTION_PROCESSING_IS_INCOMPLETE:
        case ResponseStatusCode::REQUESTED_ACTIVITY_PATTERN_NOT_PERMITTED:
            return ResponseStatus::Forbidden;

        case ResponseStatusCode::NOT_FOUND:
        case ResponseStatusCode::ONTOLOGY_NOT_AVAILABLE:
        case ResponseStatusCode::LINKED_SEMANTICS_NOT_AVAILABLE:
        case ResponseStatusCode::MASHUP_MEMBER_NOT_FOUND:
        case ResponseStatusCode::ONTOLOGY_MAPPING_ALGORITHM_NOT_AVAILABLE:
        case ResponseStatusCode::ONTOLOGY_MAPPING_NOT_AVAILABLE:
        case ResponseStatusCode::TARGET_NOT_REACHABLE:
        case ResponseStatusCode::REMOTE_ENTITY_NOT_REACHABLE:
        case ResponseStatusCode::EXTERNAL_OBJECT_NOT_REACHABLE:
        case ResponseStatusCode::EXTERNAL_OBJECT_NOT_FOUND:
            return ResponseStatus::NotFound;

        case ResponseStatusCode::OPERATION_NOT_ALLOWED:
            return ResponseStatus::MethodNotAllowed;

        case ResponseStatusCode::NOT_ACCEPTABLE:
            return ResponseStatus::NotAcceptable;

        case ResponseStatusCode::GROUP_REQUEST_IDENTIFIER_EXISTS:
        case ResponseStatusCode::CONFLICT:
        case ResponseStatusCode::BLOCKING_SUBSCRIPTION_ALREADY_EXISTS:
        case ResponseStatusCode::SOFTWARE_CAMPAIGN_CONFLICT:
        case ResponseStatusCode::ALREADY_EXISTS:
        case ResponseStatusCode::UNABLE_TO_REPLACE_REQUEST:
        case ResponseStatusCode::UNABLE_TO_RECALL_REQUEST:
        case ResponseStatusCode::ALREADY_COMPLETE:
        case ResponseStatusCode::MGMT_COMMAND_NOT_CANCELLABLE:
            return ResponseStatus::Conflict;

        case ResponseStatusCode::UNSUPPORTED_MEDIA_TYPE:
            return ResponseStatus::UnsupportedMediaType;

        case ResponseStatusCode::RELEASE_VERSION_NOT_SUPPORTED:
        case ResponseStatusCode::SPECIALIZATION_SCHEMA_NOT_FOUND:
        case ResponseStatusCode::NOT_IMPLEMENTED:
            return ResponseStatus::NotImplemented;

        case ResponseStatusCode::NON_BLOCKING_SYNCH_REQUEST_NOT_SUPPORTED:
        case ResponseStatusCode::REQUEST_TIMEOUT:
        case ResponseStatusCode::EXTERNAL_OBJECT_NOT_REACHABLE_BEFORE_RQET_TIMEOUT:
        case ResponseStatusCode::EXTERNAL_OBJECT_NOT_REACHABLE_BEFORE_OET_TIMEOUT:
            return ResponseStatus::GatewayTimeout;

        case ResponseStatusCode::INTERNAL_SERVER_ERROR:
        case ResponseStatusCode::SUBSCRIPTION_VERIFICATION_INITIATION_FAILED:
        case ResponseStatusCode::GROUP_MEMBERS_NOT_RESPONDED:
        case ResponseStatusCode::ESPRIM_DECRYPTION_ERROR:
        case ResponseStatusCode::ESPRIM_ENCRYPTION_ERROR:
        case ResponseStatusCode::SPARQL_UPDATE_ERROR:
        case ResponseStatusCode::JOIN_MULTICAST_GROUP_FAILED:
        case ResponseStatusCode::LEAVE_MULTICAST_GROUP_FAILED:
        case ResponseStatusCode::CROSS_RESOURCE_OPERATION_FAILURE:
        case ResponseStatusCode::ONTOLOGY_MAPPING_ALGORITHM_FAILED:
        case ResponseStatusCode::ONTOLOGY_CONVERSION_FAILED:
        case ResponseStatusCode::REASONING_PROCESSING_FAILED:
        case ResponseStatusCode::MGMT_SESSION_CANNOT_BE_ESTABLISHED:
        case ResponseStatusCode::MGMT_SESSION_ESTABLISHMENT_TIMEOUT:
        case ResponseStatusCode::MGMT_CONVERSION_ERROR:
        case ResponseStatusCode::MGMT_CANCELLATION_FAILED:
        case ResponseStatusCode::NETWORK_QOS_CONFIG_ERROR:
        default:
            return ResponseStatus::InternalServerError;
    }
}

HttpRequest HttpAdapter::encodeRequest(const RequestPrimitive &prim,
                                       const CString          &baseUrl,
                                       const CString          &acceptType)
{
    HttpRequest req;

    req.Method = operationToMethod(prim.op);

    // -------------------------
    // Target resolution
    // -------------------------
    CString target = prim.to;
    if (target.GetLength() == 0) { target = baseUrl; }

    req.Target = target;

    int q = target.Find('?');
    if (q >= 0) {
        for (int i = 0; i < q; ++i) {
            req.Path += target.c_str()[i];
        }

        const char *queryStart = target.c_str() + q + 1;
        req.Query              = queryStart;
    } else {
        req.Path = target;
    }

    // -------------------------
    // Headers (IMPORTANT FIX)
    // -------------------------

    req.HeaderNames.push_back(ORIGIN);
    req.HeaderValues.push_back(prim.from);
    req.HeaderNames.push_back(REQUEST_ID);
    req.HeaderValues.push_back(prim.requestIdentifier);

    if (prim.releaseVersionIndicator) {
        req.HeaderNames.push_back(RELEASE_VERSION);
        req.HeaderValues.push_back(*prim.releaseVersionIndicator);
    }
    if (prim.originatingTimestamp) {
        req.HeaderNames.push_back(ORIG_TIMESTAMP);
        req.HeaderValues.push_back(*prim.originatingTimestamp);
    }

    if (prim.requestExpirationTimestamp) {
        req.HeaderNames.push_back(REQ_EXP_TS);
        req.HeaderValues.push_back(*prim.requestExpirationTimestamp);
    }

    if (prim.resultExpirationTimestamp) {
        req.HeaderNames.push_back(RES_EXP_TS);
        req.HeaderValues.push_back(*prim.resultExpirationTimestamp);
    }

    if (prim.operationExecutionTime) {
        req.HeaderNames.push_back(OP_EXEC_TIME);
        req.HeaderValues.push_back(*prim.operationExecutionTime);
    }

    if (prim.groupRequestIdentifier) {
        req.HeaderNames.push_back(GROUP_REQ_ID);
        req.HeaderValues.push_back(*prim.groupRequestIdentifier);
    }

    if (acceptType.GetLength() > 0) {
        req.HeaderNames.push_back(ACCEPT);
        req.HeaderValues.push_back(acceptType);
    }

    // -------------------------
    // Content-Type
    // -------------------------
    CString contentType = acceptType;
    if (contentType.GetLength() == 0) contentType = mime::JSON;

    if (prim.op == Operation::Create && prim.resourceType) {
        CString ct;
        ct.Format("%s;ty=%d", contentType.c_str(), static_cast<int>(*prim.resourceType));
        contentType = ct;
    }

    // -------------------------
    // Body
    // -------------------------
    if (!prim.content.empty()) {
        CString body;

        if (SerDe::Get().SerializePrimitiveContent(prim.content, contentType, body)) {
            req.HeaderNames.push_back(CONTENT_TYPE);
            req.HeaderValues.push_back(contentType);
            req.Body = body;
        }
    }

    return req;
}

ResponsePrimitive HttpAdapter::decodeResponse(const HttpResponse &h,
                                              const CString      &requestIdentifier)
{
    ResponsePrimitive rsp;

    rsp.requestIdentifier = requestIdentifier;

    auto fetchHeader = [&](const char *name) -> Optional<CString> {
        const CString val = h.GetHeader(name);
        if (val.GetLength() == 0) return Optional<CString>();
        return Optional<CString>(val);
    };

    auto rscHdr = fetchHeader(RSC);

    if (rscHdr) {
        char *end = nullptr;

        unsigned long v = strtoul(rscHdr->c_str(), &end, 10);

        if (end != rscHdr->c_str())
            rsp.responseStatusCode = static_cast<ResponseStatusCode>(static_cast<long>(v));
    }

    auto fr = fetchHeader(ORIGIN);
    if (fr) rsp.from = *fr;

    auto rvi = fetchHeader(RELEASE_VERSION);
    if (rvi) rsp.releaseVersionIndicator = *rvi;

    auto ot = fetchHeader(ORIG_TIMESTAMP);
    if (ot) rsp.originatingTimestamp = *ot;

    auto rst = fetchHeader(RES_EXP_TS);
    if (rst) rsp.resultExpirationTimestamp = *rst;

    auto vi = fetchHeader(VENDOR_INFO);
    if (vi) rsp.vendorInformation = *vi;

    auto su = fetchHeader(SERVICE_USER);
    if (su) rsp.m2mServiceUser = *su;

    auto ati = fetchHeader(ASSIGNED_TOKENS);
    if (ati) rsp.assignedTokenIdentifiers = *ati;

    auto tri = fetchHeader(TOKEN_REQ_INFO);
    if (tri) rsp.tokenRequestInformation = *tri;

    auto cl = fetchHeader(CONTENT_LOCATION);
    if (cl) rsp.to = *cl;

    if (h.Body.GetLength() > 0) {

        CString contentType;

        auto ct = fetchHeader(CONTENT_TYPE);
        if (ct) contentType = *ct;
        else contentType = mime::JSON;
        SerDe::Get().DeserializeResponsePrimitive(h.Body, contentType, rsp);
    }

    return rsp;
}

} // namespace zerom2m::onem2m::bindings::http
