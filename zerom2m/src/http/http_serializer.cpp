/*
 * http_serializer.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "http_serializer.h"
#include <zerom2m/http/http_codec.h>

#include <zerom2m/http/config.h>
#include <zerom2m/http/types.h>

#include <circle/logger.h>
#include <circle/string.h>
#include <circle/util.h>

namespace zerom2m::http
{

namespace
{
const char FromHttpSerializer[] = "http_serializer";
}

/**
 * @brief Map a ResponseStatus to a human-readable reason phrase.
 */
const char *HttpSerializer::StatusReason(ResponseStatus status)
{
    switch (status) {
        // 1xx — Informational
        case ResponseStatus::Continue:
            return "Continue";
        case ResponseStatus::SwitchingProtocols:
            return "Switching Protocols";
        case ResponseStatus::Processing:
            return "Processing";
        case ResponseStatus::EarlyHints:
            return "Early Hints";

        // 2xx — Success
        case ResponseStatus::OK:
            return "OK";
        case ResponseStatus::Created:
            return "Created";
        case ResponseStatus::Accepted:
            return "Accepted";
        case ResponseStatus::NonAuthoritativeInformation:
            return "Non-Authoritative Information";
        case ResponseStatus::NoContent:
            return "No Content";
        case ResponseStatus::ResetContent:
            return "Reset Content";
        case ResponseStatus::PartialContent:
            return "Partial Content";
        case ResponseStatus::MultiStatus:
            return "Multi-Status";
        case ResponseStatus::AlreadyReported:
            return "Already Reported";
        case ResponseStatus::IMUsed:
            return "IM Used";

        // 3xx — Redirection
        case ResponseStatus::MultipleChoices:
            return "Multiple Choices";
        case ResponseStatus::MovedPermanently:
            return "Moved Permanently";
        case ResponseStatus::Found:
            return "Found";
        case ResponseStatus::SeeOther:
            return "See Other";
        case ResponseStatus::NotModified:
            return "Not Modified";
        case ResponseStatus::TemporaryRedirect:
            return "Temporary Redirect";
        case ResponseStatus::PermanentRedirect:
            return "Permanent Redirect";

        // 4xx — Client Error
        case ResponseStatus::BadRequest:
            return "Bad Request";
        case ResponseStatus::Unauthorized:
            return "Unauthorized";
        case ResponseStatus::PaymentRequired:
            return "Payment Required";
        case ResponseStatus::Forbidden:
            return "Forbidden";
        case ResponseStatus::NotFound:
            return "Not Found";
        case ResponseStatus::MethodNotAllowed:
            return "Method Not Allowed";
        case ResponseStatus::NotAcceptable:
            return "Not Acceptable";
        case ResponseStatus::ProxyAuthenticationRequired:
            return "Proxy Authentication Required";
        case ResponseStatus::RequestTimeout:
            return "Request Timeout";
        case ResponseStatus::Conflict:
            return "Conflict";
        case ResponseStatus::Gone:
            return "Gone";
        case ResponseStatus::LengthRequired:
            return "Length Required";
        case ResponseStatus::PreconditionFailed:
            return "Precondition Failed";
        case ResponseStatus::ContentTooLarge:
            return "Content Too Large";
        case ResponseStatus::URITooLong:
            return "URI Too Long";
        case ResponseStatus::UnsupportedMediaType:
            return "Unsupported Media Type";
        case ResponseStatus::RangeNotSatisfiable:
            return "Range Not Satisfiable";
        case ResponseStatus::ExpectationFailed:
            return "Expectation Failed";
        case ResponseStatus::ImATeapot:
            return "I'm a Teapot";
        case ResponseStatus::MisdirectedRequest:
            return "Misdirected Request";
        case ResponseStatus::UnprocessableContent:
            return "Unprocessable Content";
        case ResponseStatus::Locked:
            return "Locked";
        case ResponseStatus::FailedDependency:
            return "Failed Dependency";
        case ResponseStatus::TooEarly:
            return "Too Early";
        case ResponseStatus::UpgradeRequired:
            return "Upgrade Required";
        case ResponseStatus::PreconditionRequired:
            return "Precondition Required";
        case ResponseStatus::TooManyRequests:
            return "Too Many Requests";
        case ResponseStatus::RequestHeaderFieldsTooLarge:
            return "Request Header Fields Too Large";
        case ResponseStatus::UnavailableForLegalReasons:
            return "Unavailable For Legal Reasons";

        // 5xx — Server Error
        case ResponseStatus::InternalServerError:
            return "Internal Server Error";
        case ResponseStatus::NotImplemented:
            return "Not Implemented";
        case ResponseStatus::BadGateway:
            return "Bad Gateway";
        case ResponseStatus::ServiceUnavailable:
            return "Service Unavailable";
        case ResponseStatus::GatewayTimeout:
            return "Gateway Timeout";
        case ResponseStatus::HTTPVersionNotSupported:
            return "HTTP Version Not Supported";
        case ResponseStatus::VariantAlsoNegotiates:
            return "Variant Also Negotiates";
        case ResponseStatus::InsufficientStorage:
            return "Insufficient Storage";
        case ResponseStatus::LoopDetected:
            return "Loop Detected";
        case ResponseStatus::NotExtended:
            return "Not Extended";
        case ResponseStatus::NetworkAuthenticationRequired:
            return "Network Authentication Required";

        case ResponseStatus::UnknownResponseStatus:
        default:
            return "Unknown";
    }
}

void HttpSerializer::Serialize(const HttpResponse &response, CString &outHeader)
{
    HttpCodec::SerializeResponse(response, outHeader);
}

} // namespace zerom2m::http
