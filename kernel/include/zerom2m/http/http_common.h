/*
 * http_common.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#ifndef COMMIT_HASH
#define COMMIT_HASH "unknown"
#endif

#define SERVER_NAME "ZeroM2M/" COMMIT_HASH

namespace zerom2m::http
{

static constexpr unsigned DEFAULT_PORT     = 80u;
static constexpr unsigned MAX_CONTENT_SIZE = 2048u;
static constexpr unsigned MAX_CLIENTS      = 10u;
static constexpr unsigned TIMEOUT_SECONDS  = 10u;

enum RequestMethod {
    GET,
    HEAD,
    OPTIONS,
    TRACE,
    PUT,
    DELETE,
    POST,
    PATCH,
    CONNECT,
    RequestMethodUnknown,
    RequestMethodCount
};

enum ResponseStatus {
    // Informational
    Continue           = 100,
    SwitchingProtocols = 101,
    Processing         = 102,
    EarlyHints         = 103,
    // Success
    OK                          = 200,
    Created                     = 201,
    Accepted                    = 202,
    NonAuthoritativeInformation = 203,
    NoContent                   = 204,
    ResetContent                = 205,
    PartialContent              = 206,
    MultiStatus                 = 207,
    AlreadyReported             = 208,
    IMUsed                      = 226,
    // Redirection
    MultipleChoices   = 300,
    MovedPermanently  = 301,
    Found             = 302,
    SeeOther          = 303,
    NotModified       = 304,
    TemporaryRedirect = 307,
    PermanentRedirect = 308,
    // Client Error
    BadRequest                  = 400,
    Unauthorized                = 401,
    PaymentRequired             = 402,
    Forbidden                   = 403,
    NotFound                    = 404,
    MethodNotAllowed            = 405,
    NotAcceptable               = 406,
    ProxyAuthenticationRequired = 407,
    RequestTimeout              = 408,
    Conflict                    = 409,
    Gone                        = 410,
    LengthRequired              = 411,
    PreconditionFailed          = 412,
    ContentTooLarge             = 413,
    URITooLong                  = 414,
    UnsupportedMediaType        = 415,
    RangeNotSatisfiable         = 416,
    ExpectationFailed           = 417,
    ImATeapot                   = 418,
    MisdirectedRequest          = 421,
    UnprocessableContent        = 422,
    Locked                      = 423,
    FailedDependency            = 424,
    TooEarly                    = 425,
    UpgradeRequired             = 426,
    PreconditionRequired        = 428,
    TooManyRequests             = 429,
    RequestHeaderFieldsTooLarge = 431,
    UnavailableForLegalReasons  = 451,
    // Server Error
    InternalServerError           = 500,
    NotImplemented                = 501,
    BadGateway                    = 502,
    ServiceUnavailable            = 503,
    GatewayTimeout                = 504,
    HTTPVersionNotSupported       = 505,
    VariantAlsoNegotiates         = 506,
    InsufficientStorage           = 507,
    LoopDetected                  = 508,
    NotExtended                   = 510,
    NetworkAuthenticationRequired = 511,
    // Self defined codes (for the server only)
    UnknownResponseStatus = 600,
};

} // namespace zerom2m::http