/*
 * http_codec.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include <zerom2m/http/config.h>
#include <zerom2m/http/http_codec.h>

#include <circle/logger.h>
#include <circle/string.h>

#include <string.h>

namespace zerom2m::http
{

namespace
{
const char FromHttpCodec[] = "http_codec";

inline void TrimSpaces(char *&start, char *&end)
{
    while (start < end && (*start == ' ' || *start == '\t')) {
        start++;
    }
    while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
}

inline bool Equals(const char *a, const char *b) { return strcmp(a, b) == 0; }

RequestMethod ParseMethodToken(const char *token)
{
    if (Equals(token, "GET")) return RequestMethod::GET;
    if (Equals(token, "HEAD")) return RequestMethod::HEAD;
    if (Equals(token, "POST")) return RequestMethod::POST;
    if (Equals(token, "PUT")) return RequestMethod::PUT;
    if (Equals(token, "DELETE")) return RequestMethod::DELETE;
    if (Equals(token, "PATCH")) return RequestMethod::PATCH;
    if (Equals(token, "OPTIONS")) return RequestMethod::OPTIONS;
    if (Equals(token, "TRACE")) return RequestMethod::TRACE;
    if (Equals(token, "CONNECT")) return RequestMethod::CONNECT;
    return RequestMethod::RequestMethodUnknown;
}

CString BuildTarget(const HttpRequest &request)
{
    CString target;

    // 1. If full target exists, use it directly
    if (request.Target.GetLength() > 0) {
        target = request.Target;
        return target;
    }

    // 2. Build from path
    if (request.Path.GetLength() > 0) {
        target = request.Path;
    } else {
        target = "/";
    }

    // 3. Append query if present
    if (request.Query.GetLength() > 0) {
        target += '?';
        target += request.Query;
    }
    return target;
}

const char *StatusReason(ResponseStatus status)
{
    switch (status) {
        case ResponseStatus::Continue:
            return "Continue";
        case ResponseStatus::SwitchingProtocols:
            return "Switching Protocols";
        case ResponseStatus::Processing:
            return "Processing";
        case ResponseStatus::EarlyHints:
            return "Early Hints";

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
} // namespace

ResponseStatus HttpCodec::ParseRequest(const u8 *data, size_t length, HttpRequest &request)
{
    if (data == nullptr || length == 0) { return ResponseStatus::BadRequest; }

    request.Clear();

    char *buffer = (char *)data;

    //
    // Find end of headers (\r\n\r\n)
    //

    size_t headerEnd = MAX_HEADERS;

    for (size_t i = 0; i + 3 < length; ++i) {
        if (buffer[i] == '\r' && buffer[i + 1] == '\n' && buffer[i + 2] == '\r' &&
            buffer[i + 3] == '\n') {
            headerEnd = i;
            break;
        }
    }

    if (headerEnd == MAX_HEADERS) { return ResponseStatus::BadRequest; }

    auto parseRequestLine = [&](char *line) -> ResponseStatus {
        char *sp1 = strchr(line, ' ');
        if (!sp1) { return ResponseStatus::BadRequest; }

        *sp1 = '\0';

        char *sp2 = strchr(sp1 + 1, ' ');
        if (!sp2) { return ResponseStatus::BadRequest; }

        *sp2 = '\0';

        char *method  = line;
        char *target  = sp1 + 1;
        char *version = sp2 + 1;

        request.Method = ParseMethodToken(method);

        if (request.Method == RequestMethodUnknown) { return ResponseStatus::BadRequest; }

        request.Target  = target;
        request.Version = version;

        char *q = strchr(target, '?');

        if (q != nullptr) {
            *q = '\0';

            request.Path  = target;
            request.Query = q + 1;
        } else {
            request.Path  = target;
            request.Query = "";
        }

        return ResponseStatus::OK;
    };

    auto parseHeaderLine = [&](char *line) -> ResponseStatus {
        char *colon = strchr(line, ':');

        if (!colon) { return ResponseStatus::BadRequest; }

        *colon = '\0';

        char *nameStart  = line;
        char *valueStart = colon + 1;

        while (*valueStart == ' ' || *valueStart == '\t') {
            ++valueStart;
        }

        char *valueEnd = valueStart + strlen(valueStart);

        while (valueEnd > valueStart && (valueEnd[-1] == ' ' || valueEnd[-1] == '\t')) {
            --valueEnd;
        }

        *valueEnd = '\0';

        request.HeaderNames.push_back(nameStart);
        request.HeaderValues.push_back(valueStart);
        // CLogger::Get()->Write(
        //     FromHttpCodec, LogNotice, "Parsed header: %s: %s", nameStart, valueStart);

        return ResponseStatus::OK;
    };

    //
    // Parse request line + headers
    //

    bool   firstLine = true;
    size_t lineStart = 0;

    for (size_t i = 0; i < headerEnd + 2 && i < length; ++i) {
        if (buffer[i] == '\r') { buffer[i] = '\0'; }

        if (buffer[i] == '\n') {
            buffer[i] = '\0';

            char *line = &buffer[lineStart];

            if (*line != '\0') {
                ResponseStatus status;

                if (firstLine) {
                    status    = parseRequestLine(line);
                    firstLine = false;
                } else {
                    status = parseHeaderLine(line);
                }

                if (status != ResponseStatus::OK) { return status; }
            }

            lineStart = i + 1;
        }
    }

    //
    // Parse body
    //

    size_t bodyOffset = headerEnd + 4;

    CString cl = request.GetHeader("Content-Length");

    size_t contentLength = 0;

    if (cl.GetLength() > 0) { contentLength = (size_t)atoi(cl.c_str()); }

    if (contentLength == 0) {
        request.Body = "";
        return ResponseStatus::OK;
    }

    if (bodyOffset + contentLength > length) { return ResponseStatus::BadRequest; }

    char *bodyCopy = new char[contentLength + 1];

    memcpy(bodyCopy, &buffer[bodyOffset], contentLength);

    bodyCopy[contentLength] = '\0';

    request.Body = bodyCopy;

    delete[] bodyCopy;

    return ResponseStatus::OK;
}

void HttpCodec::SerializeRequest(const HttpRequest &request,
                                 CString           &outHeader,
                                 CString           &host,
                                 CString           &userAgent)
{
    outHeader = "";

    const char *method = nullptr;
    switch (request.Method) {
        case GET:
            method = "GET";
            break;
        case HEAD:
            method = "HEAD";
            break;
        case OPTIONS:
            method = "OPTIONS";
            break;
        case TRACE:
            method = "TRACE";
            break;
        case PUT:
            method = "PUT";
            break;
        case DELETE:
            method = "DELETE";
            break;
        case POST:
            method = "POST";
            break;
        case PATCH:
            method = "PATCH";
            break;
        case CONNECT:
            method = "CONNECT";
            break;
        default:
            method = "GET";
            break;
    }

    CString target = BuildTarget(request);
    if (target.GetLength() == 0) { target = "/"; }

    outHeader.Format("%s %s HTTP/1.1\r\n", method, target.c_str());

    if (host.GetLength() > 0) {
        outHeader.Append("Host: ");
        outHeader.Append(host);
        outHeader.Append("\r\n");
    }

    if (userAgent.GetLength() > 0) {
        outHeader.Append("User-Agent: ");
        outHeader.Append(userAgent);
        outHeader.Append("\r\n");
    }

    // Headers stored as parallel vectors: Name[i], Value[i]
    for (size_t i = 0; i < request.HeaderCount(); ++i) {
        const CString &name  = request.HeaderNames[i];
        const CString &value = request.HeaderValues[i];

        // Skip empty headers
        if (name.GetLength() == 0) continue;

        // Skip reserved headers
        if (name.Compare("Host") == 0 || name.Compare("Connection") == 0 ||
            name.Compare("User-Agent") == 0 || name.Compare("Content-Length") == 0) {
            continue;
        }

        outHeader.Append(name);
        outHeader.Append(": ");
        outHeader.Append(value);
        outHeader.Append("\r\n");
    }

    if (request.Body.GetLength() > 0) {
        CString contentLength;
        contentLength.Format("%u", static_cast<unsigned>(request.Body.GetLength()));

        outHeader.Append("Content-Length: ");
        outHeader.Append(contentLength);
        outHeader.Append("\r\n");
    }

    outHeader.Append("Connection: close\r\n\r\n");
}

void HttpCodec::SerializeResponse(const HttpResponse &response, CString &outHeader)
{
    outHeader.Format("HTTP/1.1 %u %s\r\n", response.Status, StatusReason(response.Status));
    outHeader.Append("Server: " SERVER_NAME "\r\n");

    bool hasContentLength = false;
    for (size_t i = 0; i < response.HeaderValues.GetCount(); ++i) {
        const CString &name  = response.HeaderNames[i];
        const CString &value = response.HeaderValues[i];
        if (name.GetLength() == 0 || value.GetLength() == 0) { continue; }

        if (name.GetLength() == sizeof("Content-Length") - 1 &&
            strncmp(name.c_str(), "Content-Length", sizeof("Content-Length") - 1) == 0) {
            hasContentLength = true;
        }

        outHeader += name.c_str();
        outHeader.Append(": ");
        outHeader += value.c_str();
        outHeader.Append("\r\n");
    }

    if (!hasContentLength) {
        CString len;
        len.Format("%u", static_cast<unsigned>(response.Body.GetLength()));
        outHeader.Append("Content-Length: ");
        outHeader.Append(len);
        outHeader.Append("\r\n");
    }

    outHeader.Append("Connection: close\r\n\r\n");
}

bool HttpCodec::ParseResponse(const u8 *data, size_t length, HttpResponse &response)
{
    response = HttpResponse();

    if (data == nullptr || length == 0) { return false; }

    char  *buffer    = reinterpret_cast<char *>(const_cast<u8 *>(data));
    size_t headerEnd = 0;
    bool   foundEnd  = false;

    for (size_t i = 0; i + 3 < length; ++i) {
        if (buffer[i] == '\r' && buffer[i + 1] == '\n' && buffer[i + 2] == '\r' &&
            buffer[i + 3] == '\n') {
            headerEnd = i + 4;
            foundEnd  = true;
            break;
        }
    }

    if (!foundEnd) {
        for (size_t i = 0; i + 1 < length; ++i) {
            if (buffer[i] == '\n' && buffer[i + 1] == '\n') {
                headerEnd = i + 2;
                foundEnd  = true;
                break;
            }
        }
    }

    if (!foundEnd || headerEnd == 0) { return false; }

    size_t lineStart        = 0;
    size_t i                = 0;
    bool   firstLine        = true;
    size_t contentLength    = 0;
    bool   hasContentLength = false;

    while (i < headerEnd) {
        if (buffer[i] == '\r' || buffer[i] == '\n') {
            char newline = buffer[i];
            buffer[i]    = '\0';
            if (newline == '\r' && i + 1 < headerEnd && buffer[i + 1] == '\n') {
                buffer[i + 1] = '\0';
                ++i;
            }

            char *line = &buffer[lineStart];
            if (line[0] == '\0') { break; }

            if (firstLine) {
                firstLine = false;

                char *sp1 = strchr(line, ' ');
                if (sp1 == nullptr) { return false; }
                *sp1 = '\0';

                char *statusText = sp1 + 1;
                char *sp2        = strchr(statusText, ' ');
                if (sp2 != nullptr) { *sp2 = '\0'; }

                if (strncmp(line, "HTTP/", 5) != 0) { return false; }

                char         *end    = nullptr;
                unsigned long status = strtoul(statusText, &end, 10);
                if (end == statusText || (end != nullptr && *end != '\0')) { return false; }
                response.Status = static_cast<ResponseStatus>(status);
            } else {
                char *colon = strchr(line, ':');
                if (colon == nullptr) {
                    lineStart = i + 1;
                    continue;
                }

                *colon      = '\0';
                char *name  = line;
                char *value = colon + 1;
                while (*value == ' ' || *value == '\t') {
                    ++value;
                }

                char *valueEnd = value + strlen(value);
                while (valueEnd > value && (valueEnd[-1] == ' ' || valueEnd[-1] == '\t')) {
                    --valueEnd;
                }
                *valueEnd = '\0';

                if (strncasecmp(name, "Content-Length", strlen("Content-Length")) == 0) {
                    char         *end    = nullptr;
                    unsigned long parsed = strtoul(value, &end, 10);
                    if (end == value || (end != nullptr && *end != '\0')) { return false; }
                    contentLength    = static_cast<size_t>(parsed);
                    hasContentLength = true;
                }

                response.AddHeader(name, value);
            }

            lineStart = i + 1;
        }

        ++i;
    }

    const size_t bodyStart = headerEnd;
    if (bodyStart > length) { return false; }

    size_t bodyLength = length - bodyStart;

    if (hasContentLength) {
        if (contentLength > bodyLength) {
            return false; // truncated response
        }

        bodyLength = contentLength;
    }

    if (bodyLength > MAX_CONTENT_SIZE) { return false; }

    if (bodyLength > 0) {
        char bodyStorage[MAX_CONTENT_SIZE + 1]{};

        memcpy(bodyStorage, &buffer[bodyStart], bodyLength);
        bodyStorage[bodyLength] = '\0';

        response.Body = bodyStorage;
    } else {
        response.Body = "";
    }

    return true;
}

} // namespace zerom2m::http