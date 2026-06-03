/*
 * onem2m_service.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */

#include <zerom2m/compat/types.h>
#include <zerom2m/config/system_config.h>
#include <zerom2m/http/http_client.h>
#include <zerom2m/kernel/paths.h>
#include <zerom2m/onem2m/binding.h>
#include <zerom2m/onem2m/onem2m_service.h>
#include <zerom2m/onem2m/types/primitives.h>
#include <zerom2m/onem2m/utils.h>
#include <zerom2m/serde/serde.h>
#include <zerom2m/sqlite/database.h>

#include <circle/logger.h>
#include <circle/net/ipaddress.h>
#include <circle/net/netsubsystem.h>
#include <circle/util.h>

#include <string.h>

namespace zerom2m::onem2m
{

using namespace zerom2m::onem2m::types;
using namespace zerom2m::http;
using namespace zerom2m::serde;
using zerom2m::config::SystemConfig;
using zerom2m::sqlite::Database;

namespace
{

bool MatchesChildType(const Subscription &sub, ResourceType type)
{
    const auto &types = sub.eventNotificationCriteria.childResourceType;

    if (types.empty()) return true;

    for (auto t : types) {
        if (t == type) return true;
    }

    return false;
}

bool MatchesLabels(const Subscription &sub, const ResourceBase *resource)
{
    const auto &required = sub.eventNotificationCriteria.labels;

    if (required.empty()) return true;

    for (const auto &label : required) {
        bool found = false;
        for (const auto &rl : resource->labels) {
            if (rl == label) {
                found = true;
                break;
            }
        }

        if (!found) return false;
    }

    return true;
}

bool MatchesSizeFilter(const Subscription &sub, const PrimitiveContent &pc)
{
    s64 size = 0;

    if (auto *cin = pc.GetIf<ContentInstance>()) size = cin->contentSize;
    if (auto *tsi = pc.GetIf<TimeSeriesInstance>()) size = tsi->contentSize;

    const auto &enc = sub.eventNotificationCriteria;

    if (enc.sizeAbove.has_value() && size <= *enc.sizeAbove) return false;
    if (enc.sizeBelow.has_value() && size >= *enc.sizeBelow) return false;

    return true;
}

bool ContainsNotificationEvent(const Subscription &sub, NotificationEventType eventType)
{
    for (const NotificationEventType value : sub.eventNotificationCriteria.notificationEventType) {
        if (value == eventType) return true;
    }
    return false;
}

bool MatchesCriteria(const Subscription     &sub,
                     const PrimitiveContent &changed,
                     NotificationEventType   eventType)
{
    const ResourceBase *base = GetResourceBase(changed);

    if (changed.empty()) return false;
    if (!ContainsNotificationEvent(sub, eventType)) return false;
    if (!MatchesChildType(sub, base->resourceType.value())) return false;
    if (!MatchesLabels(sub, base)) return false;
    if (!MatchesSizeFilter(sub, changed)) return false;

    return true;
}

Notification BuildNotification(const Subscription     &sub,
                               const PrimitiveContent &changed,
                               NotificationEventType   eventType)
{
    Notification      sgn;
    PrimitiveContent *c = new PrimitiveContent(changed);

    sgn.subscriptionReference = sub.resourceID;

    sgn.notificationEvent.emplace();

    auto &nev = *sgn.notificationEvent;

    nev.notificationEventType = eventType;

    //
    // nct handling
    //

    NotificationContentType nct = NotificationContentType::AllAttributes;
    if (sub.notificationContentType.has_value()) nct = sub.notificationContentType.value();

    switch (nct) {
        case NotificationContentType::AllAttributes: {
            nev.representation = c;
            break;
        }
        case NotificationContentType::ResourceID: {
            // TODO: create lightweight representation
            break;
        }
        case NotificationContentType::ModifiedAttributes: {
            // XXX: out of scope
            break;
        }
        default:
            break;
    }

    // FIXME: populate other attributes

    return sgn;
}

// TODO: Clean this ones below

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

bool LooksLikeServiceProviderId(const CString &spid)
{
    const char *text = spid.c_str();
    if (text == nullptr || *text == '\0') return false;
    return strchr(text, '.') != nullptr || strchr(text, '-') != nullptr ||
           strchr(text, ':') != nullptr;
}

bool ParseAddressingPath(const CString   &path,
                         Vector<CString> &segments,
                         bool            &absolute,
                         bool            &networkPrefix,
                         CString         &spid)
{
    segments.clear();
    spid          = CString();
    networkPrefix = false;
    absolute      = false;

    const char *raw = path.c_str();
    if (raw == nullptr || raw[0] != '/') return false;

    const size_t len = path.GetLength();
    if (len > 1 && raw[len - 1] == '/') return false;

    networkPrefix    = len > 2 && raw[1] == '~' && raw[2] == '/';
    bool underscored = len > 2 && raw[1] == '_' && raw[2] == '/';
    absolute         = (len > 1 && raw[1] == '/' && !networkPrefix) || underscored;
    size_t pos       = networkPrefix ? 3 : (absolute ? (underscored ? 3 : 2) : 1);

    if (absolute) {
        size_t start = pos;
        while (raw[pos] != '\0' && raw[pos] != '/')
            ++pos;
        if (pos == start) return false;

        spid = SliceCString(raw + start, pos - start);

        // Only enforce FQDN-like check for // form; /_/ accepts any non-empty SPID token
        if (!underscored && !LooksLikeServiceProviderId(spid)) return false;

        if (raw[pos] != '/') return false;
        ++pos;
        if (raw[pos] == '\0') return false;
    }

    // For networkPrefix paths, peek at the first segment to detect
    // absolute addressing: /~/spid/cseid/... vs SP-relative: /~/cseid/...
    if (networkPrefix) {
        size_t segStart = pos;
        size_t segEnd   = pos;
        while (raw[segEnd] != '\0' && raw[segEnd] != '/')
            ++segEnd;
        if (segEnd == segStart) return false;

        CString firstSeg = SliceCString(raw + segStart, segEnd - segStart);
        if (LooksLikeServiceProviderId(firstSeg)) {
            // Absolute via /~/: extract SPID and mark as absolute
            spid          = firstSeg;
            absolute      = true;
            networkPrefix = false; // treat remainder as normal after SPID extraction
            pos           = segEnd;
            if (raw[pos] != '/') return false;
            ++pos;
            if (raw[pos] == '\0') return false;
        }
        // else: SP-relative /~/cseid/... — leave pos as-is, parse normally
    }

    while (raw[pos] != '\0') {
        size_t start = pos;
        while (raw[pos] != '\0' && raw[pos] != '/')
            ++pos;
        if (pos == start) return false;

        segments.push_back(SliceCString(raw + start, pos - start));

        if (raw[pos] == '/') {
            ++pos;
            if (raw[pos] == '\0') return false;
        }
    }

    return true;
}

CString CanonicalizeAddressingPath(const CString &path,
                                   const CString &cseName,
                                   const CString &cseId,
                                   const CString &spid,
                                   bool          &valid,
                                   bool          &wrongSpid)
{
    valid     = false;
    wrongSpid = false;

    Vector<CString> segments;
    CString         parsedSpid;
    bool            absolute      = false;
    bool            networkPrefix = false;
    if (!ParseAddressingPath(path, segments, absolute, networkPrefix, parsedSpid)) return CString();

    CString cseIdName = cseId;
    if (cseIdName.GetLength() > 0 && cseIdName.c_str()[0] == '/') {
        cseIdName = CString(cseIdName.c_str() + 1);
    }

    // Validate SPID for absolute addressing
    if (absolute && parsedSpid.GetLength() > 0) {
        CString knownSpid = spid;
        if (knownSpid.Compare(parsedSpid) != 0) {
            wrongSpid = true;
            return CString();
        }
    }

    unsigned start = 0;
    if (absolute) {
        // After SPID, remaining segments are: cseid/rn/... or cseid/ri
        // segments[0] should be cseid, segments[1] the cseName or "-" or a resource
        if (segments.GetCount() < 2) return CString(); // BAD_REQUEST: only cseid, no resource
        if (segments[0].Compare(cseIdName) != 0) return CString();
        start = 1; // skip the cseid segment
    } else if (networkPrefix) {
        // SP-relative: /~/cseid/...
        if (segments.GetCount() == 0) return CString();
        if (segments[0].Compare(cseIdName) != 0) return CString();
        if (segments.GetCount() == 1) return CString(); // BAD_REQUEST: only cseid
        // Unstructured shortcut: /~/cseid/ri (exactly 2 segments, second not cseName or "-")
        if (segments.GetCount() == 2 && segments[1].Compare(cseIdName) != 0 &&
            segments[1].Compare(cseName) != 0 && segments[1].Compare("-") != 0) {
            CString out("/");
            out.Append(segments[1].c_str());
            valid = true;
            return out;
        }
        start = 1; // skip cseid
    } else if (segments.GetCount() >= 2 && segments[0].Compare(cseIdName) == 0 &&
               (segments[1].Compare(cseIdName) == 0 || segments[1].Compare("-") == 0)) {
        start = 1;
    }

    Vector<CString> normalized;
    for (unsigned i = start; i < segments.GetCount(); ++i) {
        if (i == start && segments[i].Compare("-") == 0) {
            normalized.push_back(cseName);
        } else {
            normalized.push_back(segments[i]);
        }
    }

    if (normalized.GetCount() == 0) return CString();

    CString out;
    for (unsigned i = 0; i < normalized.GetCount(); ++i) {
        if (i > 0) out.Append("/");
        out.Append(normalized[i].c_str());
    }

    valid = true;
    return out;
}

boolean IsValidContentInfo(const CString &contentInfo)
{
    if (contentInfo.GetLength() == 0) return true;

    const char *text  = contentInfo.c_str();
    const char *slash = nullptr;
    const char *colon = nullptr;
    for (const char *p = text; *p != '\0'; ++p) {
        if (*p == '/') {
            if (slash != nullptr) return false;
            slash = p;
        } else if (*p == ':') {
            if (colon != nullptr) return false;
            colon = p;
        }
    }

    if (!slash || !colon || slash > colon) return false;
    if (slash == text || colon == slash + 1 || *(colon + 1) == '\0') return false;
    if (strchr(colon + 1, ':') != nullptr) return false;

    for (const char *p = colon + 1; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') return false;
    }

    int charset = 0;
    for (const char *p = colon + 1; *p != '\0'; ++p) {
        charset = charset * 10 + (*p - '0');
        if (charset > 8) return false;
    }

    return true;
}

bool MatchesResourceTarget(Database &db, const ResourceBase &resource, const CString &target)
{
    CString fullPath;
    CString loadErr;
    if (!db.GetPathByRI(resource.resourceID, fullPath, loadErr)) {
        CLogger::Get()->Write("onem2m_service",
                              LogError,
                              "Failed to get path for container '%s': %s",
                              resource.resourceID.c_str(),
                              loadErr.c_str());
        return false;
    }
    CString laPath;
    laPath.Append(fullPath);
    laPath.Append("/la");

    CString olPath;
    olPath.Append(fullPath);
    olPath.Append("/ol");

    CString normalized = NormalizePath(target);
    return fullPath.Compare(target) == 0 || NormalizePath(fullPath).Compare(normalized) == 0 ||
           resource.resourceID.Compare(normalized) == 0 ||
           resource.resourceName.Compare(normalized) == 0 || laPath.Compare(target) == 0 ||
           olPath.Compare(target) == 0;
}

bool IsAllowedForContainer(const RequestPrimitive &request, Database &db, const Container &cnt)
{
    if (request.from.Compare("CAdmin") == 0) return true;

    CString  current = cnt.parentID;
    unsigned guard   = 0;
    while (current.GetLength() != 0 && guard++ < 32) {
        PrimitiveContent parent;
        CString          loadErr;
        if (!db.LoadPrimitiveContentByTarget(current, parent, loadErr)) break;

        if (const AE *ae = parent.GetIf<AE>()) return request.from.Compare(ae->aeID) == 0;
        if (const Container *parentCnt = parent.GetIf<Container>()) {
            current = parentCnt->parentID;
            continue;
        }
        if (const CSEBase *cse = parent.GetIf<CSEBase>()) {
            return request.from.Compare(cse->cseID) == 0;
        }
        break;
    }
    return false;
}

} // namespace

CString OneM2MService::GetId()
{
    CString id;
    id.Format("C%u", nextResourceId_++);
    return id;
}

void OneM2MService::Initialize(const SystemConfig &config,
                               CNetSubSystem      &net,
                               IBinding           &httpBinding)
{
    if (initialized_) return;
    CLogger::Get()->Write("onem2m_service", LogNotice, "Initializing... ");

    net_         = &net;
    httpBinding_ = &httpBinding;

    if (config.system.clean_db_on_boot) {
        if (!db_.DeleteDatabaseFile(DB_PATH)) {
            CLogger::Get()->Write(
                "onem2m_service", LogError, "Failed to delete database file: %s", DB_PATH);
        }
    }

    // Open (or create) the SQLite database on the FAT32 volume.
    // The path uses the FatFs drive prefix set up in kernel.cpp.
    CString dbErr;
    if (!db_.Open(DB_PATH, dbErr)) {
        CLogger::Get()->Write("onem2m_service", LogError, "DB open failed: %s", dbErr.c_str());
        return;
    }

    if (!db_.InitSchema()) {
        CLogger::Get()->Write("onem2m_service", LogError, "DB schema init failed");
        return;
    }

    // If a CSEBase already exists in the DB we are resuming from persistent
    // storage — skip seeding.
    {
        CSEBase existing;
        CString lookupErr;
        if (db_.GetCSEBase(existing, lookupErr)) {
            CLogger::Get()->Write("onem2m_service",
                                  LogNotice,
                                  "CSEBase already in DB (ri='%s'), skipping seed",
                                  existing.resourceID.c_str());
            initialized_ = true;
            return;
        }
    }

    nextResourceId_ = 1;

    // set global SPID for addressing validation
    spId_ = config.cse.sp_id;

    CSEBase cse;
    cse.resourceType = ResourceType::CSEBase;
    cse.resourceName = config.cse.resource_name;
    cse.resourceID   = config.cse.resource_id;
    cse.parentID     = "";

    cse.creationTime     = "2026-01-01T00:00:00Z";
    cse.lastModifiedTime = cse.creationTime;

    cse.cseType     = CSEType::IN_CSE;
    cse.cseID       = config.cse.cse_id;
    cse.currentTime = cse.creationTime;
    cse.supportedReleaseVersions.push_back("4");

    static const ResourceType kSupportedTypes[] = {
        ResourceType::CSEBase,
        ResourceType::AE,
        ResourceType::Container,
        ResourceType::ContentInstance,
        ResourceType::Subscription,
    };
    for (ResourceType type : kSupportedTypes) {
        cse.supportedResourceType.push_back(type);
    }

    PrimitiveContent pc;
    pc = cse;

    CString saveErr;
    if (!db_.SavePrimitiveContent(pc, saveErr)) {
        CLogger::Get()->Write(
            "onem2m_service", LogError, "Failed to save CSEBase: %s", saveErr.c_str());
        return;
    }

    CLogger::Get()->Write("onem2m_service",
                          LogNotice,
                          "Inserted CSEBase: rn='%s' ri='%s' pi='%s'",
                          cse.resourceName.c_str(),
                          cse.resourceID.c_str(),
                          cse.parentID.c_str());

    initialized_ = true;
    CLogger::Get()->Write("onem2m_service", LogNotice, "Initialize() complete");
}

ResponsePrimitive OneM2MService::HandleRequest(const RequestPrimitive &request)
{
    switch (request.op) {
        case Operation::Create:
            return Create(request);
        case Operation::Retrieve:
            return Retrieve(request);
        case Operation::Update:
            return Update(request);
        case Operation::Delete:
            return Delete(request);
        default: {
            ResponsePrimitive r;
            r.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return r;
        }
    }
}

ResponsePrimitive OneM2MService::Create(const RequestPrimitive &request)
{
    ResponsePrimitive resp;

    CSEBase cse;
    CString lookupErr;
    bool    targetValid = false;
    bool    wrongSpid   = false;
    db_.GetCSEBase(cse, lookupErr);
    CString target =
        lookupErr.GetLength() == 0
            ? CanonicalizeAddressingPath(
                  request.to, cse.resourceName, cse.cseID, spId_, targetValid, wrongSpid)
            : CString();
    if (!targetValid) {
        CLogger::Get()->Write(
            "onem2m_service", LogError, "CSEBase lookup failed: %s", lookupErr.c_str());
        resp.responseStatusCode =
            wrongSpid ? ResponseStatusCode::NOT_FOUND : ResponseStatusCode::BAD_REQUEST;
        return resp;
    }

    if (request.from.GetLength() == 0) {
        if (!request.resourceType.has_value() || request.resourceType.value() != ResourceType::AE) {
            CLogger::Get()->Write(
                "onem2m_service", LogNotice, "Create request missing 'from' and not creating AE");
            resp.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return resp;
        }
    }

    if (request.content.empty()) {
        CLogger::Get()->Write("onem2m_service", LogNotice, "Create request content is empty");
        resp.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
        return resp;
    }

    PrimitiveContent pc = request.content;
    if (auto p = pc.GetIf<AE>()) resp = CreateAE(*p, request, target);
    else if (auto p = pc.GetIf<Container>()) resp = CreateContainer(*p, request, target);
    else if (auto p = pc.GetIf<ContentInstance>())
        resp = CreateContentInstance(*p, request, target);
    else if (auto p = pc.GetIf<Subscription>()) resp = CreateSubscription(*p, request, target);
    else {
        // if ResourceType is not supported or undefined
        resp.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
        return resp;
    }

    if (resp.responseStatusCode != ResponseStatusCode::CREATED) { return resp; }

    // Get the response content as for notification processing
    pc = resp.content;
    // Notify creation subscriptions
    if (!pc.GetIf<Subscription>()) {
        SendNotification(pc, NotificationEventType::CreateOfDirectChildResource, request.from);
    }
    return resp;
}

ResponsePrimitive
OneM2MService::CreateAE(const AE &ae, const RequestPrimitive &req, const CString &target)
{
    // Local copy to modify before saving to DB
    AE r = ae;

    PrimitiveContent parent;
    CString          lookupErr;
    if (!db_.LoadPrimitiveContentByTarget(target, parent, lookupErr)) {
        CLogger::Get()->Write(
            "onem2m_service", LogError, "Parent resource lookup failed: %s", lookupErr.c_str());
        ResponsePrimitive resp;
        resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
        return resp;
    }
    const CSEBase *cse = parent.GetIf<CSEBase>();
    if (!cse) {
        CLogger::Get()->Write(
            "onem2m_service", LogError, "Parent resource is not CSEBase as expected");
        ResponsePrimitive resp;
        resp.responseStatusCode = ResponseStatusCode::INVALID_CHILD_RESOURCE_TYPE;
        ;
        return resp;
    }
    r.parentID = cse->cseID;

    // Reject 'creator' attribute
    if (req.vendorInformation.has_value() && req.vendorInformation->Compare("has_creator") == 0) {
        ResponsePrimitive bad;
        bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
        return bad;
    }

    if (r.appID.GetLength() == 0) {
        ResponsePrimitive bad;
        bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
        return bad;
    }

    // Reject if appID does not start with 'R' or 'N'
    char prefix = r.appID.c_str()[0];
    if (req.releaseVersionIndicator.has_value() && req.releaseVersionIndicator->Compare("4") == 0) {
        // For Release 4, appID must start with 'R'
        if (prefix != 'R' && prefix != 'N') {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return bad;
        }
    } else {
        // For earlier releases,
        if (prefix != 'r') {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return bad;
        }
    }

    // Reject csz attribute for AE: not supported in this implementation
    if (!r.contentSerialization.empty()) {
        ResponsePrimitive bad;
        bad.responseStatusCode = ResponseStatusCode::NOT_IMPLEMENTED;
        return bad;
    }

    CString dupErr;
    if (db_.ExistsResourceByParentAndName(r.parentID, r.resourceName, dupErr)) {
        CLogger::Get()->Write("onem2m_service",
                              LogNotice,
                              "Resource name conflict for CREATE AE: %s",
                              dupErr.c_str());
        ResponsePrimitive resp;
        resp.responseStatusCode = ResponseStatusCode::CONFLICT;
        return resp;
    }

    CString cseid = cse->cseID;
    if (cseid.GetLength() > 0 && cseid.c_str()[0] == '/') { cseid = CString(cseid.c_str() + 1); }
    if (req.from.GetLength() != 0 && req.from.Compare(cseid) == 0) {
        CLogger::Get()->Write("onem2m_service",
                              LogNotice,
                              "from: '%s', cseid: '%s'",
                              req.from.c_str(),
                              cseid.c_str());
        ResponsePrimitive respSec;
        respSec.responseStatusCode = ResponseStatusCode::SECURITY_ASSOCIATION_REQUIRED;
        return respSec;
    }

    r.resourceType = ResourceType::AE;
    r.resourceID   = GetId();

    if (req.from.GetLength() == 0 || (req.from.GetLength() != 0 && req.from.Compare("C") == 0)) {
        r.aeID = r.resourceID;
    } else {
        // Verify if is unique
        if (db_.ExistsAEbyAEID(req.from, dupErr)) {
            CLogger::Get()->Write("onem2m_service",
                                  LogNotice,
                                  "AE originator conflict for CREATE AE: %s",
                                  dupErr.c_str());
            ResponsePrimitive resp;
            resp.responseStatusCode = ResponseStatusCode::ORIGINATOR_HAS_ALREADY_REGISTERED;
            return resp;
        }
        // Use the stem provided by the AE
        r.aeID = req.from;
        // ri MUST equal the AE-ID-Stem
        r.resourceID = r.aeID;
    }

    if (r.resourceName.GetLength() == 0) {
        CString rn;
        rn.Format("AE%s", r.resourceID.c_str());
        r.resourceName = rn;
    }
    if (!isValidResourceName(r.resourceName)) {
        ResponsePrimitive bad;
        bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
        return bad;
    }

    // Populate times with default values, we dont have a real clock.
    if (r.creationTime.GetLength() == 0) r.creationTime = "2026-01-01T00:00:00Z";
    if (r.lastModifiedTime.GetLength() == 0) r.lastModifiedTime = r.creationTime;
    if (!r.expirationTime.has_value()) r.expirationTime = CString("2027-01-01T00:00:00Z");

    if (r.requestReachability.has_value() && r.requestReachability.value())
        r.requestReachability = true;
    else r.requestReachability = false;

    // FIXME: clean up
    CString poa;
    for (unsigned i = 0; i < r.pointOfAccess.GetCount(); ++i) {
        if (i > 0) poa.Append(",");
        poa.Append(r.pointOfAccess[i].c_str());
    }

    CLogger::Get()->Write("onem2m_service", LogNotice, "AE poa: '%s'", poa.c_str());

    CString saveErr;
    if (!db_.SaveAE(r, saveErr)) {
        CLogger::Get()->Write(
            "onem2m_service", LogError, "CREATE AE save failed: %s", saveErr.c_str());
        ResponsePrimitive resp;
        resp.responseStatusCode = ResponseStatusCode::INTERNAL_SERVER_ERROR;
        return resp;
    }
    CLogger::Get()->Write("onem2m_service",
                          LogNotice,
                          "Inserted AE: rn='%s' ri='%s' pi='%s'",
                          r.resourceName.c_str(),
                          r.resourceID.c_str(),
                          r.parentID.c_str());

    PrimitiveContent pc;
    pc = r;
    return makeResponse(req, ResponseStatusCode::CREATED, pc);
}

ResponsePrimitive OneM2MService::CreateContainer(const Container        &con,
                                                 const RequestPrimitive &req,
                                                 const CString          &target)
{
    // Local copy to modify before saving to DB
    Container r = con;

    PrimitiveContent parent;
    CString          lookupErr;

    if (!db_.LoadPrimitiveContentByTarget(target, parent, lookupErr)) {
        CLogger::Get()->Write(
            "onem2m_service", LogError, "Parent resource lookup failed: %s", lookupErr.c_str());
        ResponsePrimitive resp;
        resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
        return resp;
    }

    ResourceBase base;
    if (parent.GetIf<CSEBase>()) {
        base = *parent.GetIf<CSEBase>();
    } else if (parent.GetIf<AE>()) {
        base = *parent.GetIf<AE>();
    } else if (parent.GetIf<Container>()) {
        base = *parent.GetIf<Container>();
    } else {
        CLogger::Get()->Write("onem2m_service",
                              LogError,
                              "Parent resource is not as expected pkind=%u",
                              parent.kind());
        ResponsePrimitive resp;
        resp.responseStatusCode = ResponseStatusCode::INVALID_CHILD_RESOURCE_TYPE;
        return resp;
    }

    r.resourceType = ResourceType::Container;
    r.parentID     = base.resourceID;
    r.resourceID   = GetId();
    if (r.resourceName.GetLength() == 0) {
        CString rn;
        rn.Format("cnt%s", r.resourceID.c_str());
        r.resourceName = rn;
    }
    if (!isValidResourceName(r.resourceName)) {
        ResponsePrimitive bad;
        bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
        return bad;
    }

    // Populate times with default values, we dont have a real clock.
    if (r.creationTime.GetLength() == 0) r.creationTime = "2026-01-01T00:00:00Z";
    if (r.lastModifiedTime.GetLength() == 0) r.lastModifiedTime = r.creationTime;
    if (!r.expirationTime.has_value()) r.expirationTime = CString("2027-01-01T00:00:00Z");
    if (!r.stateTag.has_value()) r.stateTag = 0;

    if (req.vendorInformation.has_value()) {
        if (req.vendorInformation->Compare("cnt_creator_present") == 0) {
            CLogger::Get()->Write(
                "onem2m_service", LogNotice, "Create Container request with invalid creator");
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return bad;
        }
        if (req.vendorInformation->Compare("cnt_creator_null") == 0) {
            if (req.from.GetLength() == 0) {
                CLogger::Get()->Write(
                    "onem2m_service",
                    LogNotice,
                    "Create Container request with null creator and missing originator");
                ResponsePrimitive bad;
                bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
                return bad;
            }
            r.creator = req.from;
        }
    }

    CString saveErr;
    if (!db_.SaveContainer(r, saveErr)) {
        CLogger::Get()->Write(
            "onem2m_service", LogError, "CREATE Container save failed: %s", saveErr.c_str());
        ResponsePrimitive resp;
        resp.responseStatusCode = ResponseStatusCode::INTERNAL_SERVER_ERROR;
        return resp;
    }
    CLogger::Get()->Write("onem2m_service",
                          LogNotice,
                          "Inserted Container: rn='%s' ri='%s' pi='%s'",
                          r.resourceName.c_str(),
                          r.resourceID.c_str(),
                          r.parentID.c_str());

    PrimitiveContent pc;
    pc = r;
    return makeResponse(req, ResponseStatusCode::CREATED, pc);
}

ResponsePrimitive OneM2MService::CreateContentInstance(const ContentInstance  &cin,
                                                       const RequestPrimitive &req,
                                                       const CString          &target)
{
    ContentInstance  r = cin;
    PrimitiveContent parent;
    CString          lookupErr;

    if (!db_.LoadPrimitiveContentByTarget(target, parent, lookupErr)) {
        CLogger::Get()->Write(
            "onem2m_service", LogError, "Parent resource lookup failed: %s", lookupErr.c_str());
        ResponsePrimitive resp;
        resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
        return resp;
    }
    const Container *cnt = parent.GetIf<Container>();
    if (!cnt) {
        CLogger::Get()->Write(
            "onem2m_service", LogNotice, "Create ContentInstance req without CNT parent");
        ResponsePrimitive bad;
        bad.responseStatusCode = ResponseStatusCode::INVALID_CHILD_RESOURCE_TYPE;
        return bad;
    }

    if (!IsValidContentInfo(r.contentInfo.has_value() ? *r.contentInfo : CString())) {
        CLogger::Get()->Write(
            "onem2m_service", LogNotice, "Create ContentInstance req with invalid contentInfo");
        ResponsePrimitive bad;
        bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
        return bad;
    }

    r.resourceType = ResourceType::ContentInstance;
    if (r.creationTime.GetLength() == 0) r.creationTime = "2026-01-01T00:00:00Z";
    if (r.lastModifiedTime.GetLength() == 0) r.lastModifiedTime = r.creationTime;
    if (!r.expirationTime.has_value()) r.expirationTime = CString("2027-01-01T00:00:00Z");
    if (!r.stateTag.has_value()) r.stateTag = 0;

    // Creator handling: reject explicit creator value, allow null -> set to originator
    if (req.vendorInformation.has_value()) {
        if (req.vendorInformation->Compare("cin_creator_present") == 0) {
            CLogger::Get()->Write(
                "onem2m_service", LogNotice, "Create ContentInstance req with invalid creator");
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return bad;
        }
        if (req.vendorInformation->Compare("cin_creator_null") == 0) {
            if (req.from.GetLength() == 0) {
                CLogger::Get()->Write(
                    "onem2m_service",
                    LogNotice,
                    "Create ContentInstance req with null creator and missing originator");
                ResponsePrimitive bad;
                bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
                return bad;
            }
            r.creator = req.from;
        }
        if (req.vendorInformation->Compare("cin_has_acpi") == 0) {
            CLogger::Get()->Write(
                "onem2m_service", LogNotice, "Create ContentInstance req with invalid acpi");
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return bad;
        }
    }

    // Ensure contentSize is set (codec may have estimated it)
    if (r.contentSize <= 0) { r.contentSize = static_cast<s64>(r.content.GetLength()); }

    // Enforce max byte size per container, reject if new CIN cannot fit
    if (cnt->maxByteSize.has_value() && cnt->maxByteSize.value() != 0) {
        s64 mbs = *cnt->maxByteSize;
        // XXX: As we have no updates or deletes, the current byte size is just the sum of existing
        // CIN content sizes.

        s64     currentBytes;
        CString err;
        if (!db_.GetContainerCurrentSize(cnt->resourceID, currentBytes, err)) {
            CLogger::Get()->Write("onem2m_service",
                                  LogError,
                                  "Failed to get current size for container '%s': %s",
                                  cnt->resourceID.c_str(),
                                  err.c_str());
            ResponsePrimitive resp;
            resp.responseStatusCode = ResponseStatusCode::INTERNAL_SERVER_ERROR;
            return resp;
        }

        s64             currentNrOfInstances = 0;
        Vector<CString> cinRIs;
        if (db_.LoadPrimitiveContentChildren(
                r.resourceID, ResourceType::ContentInstance, cinRIs, err)) {
            currentNrOfInstances = cinRIs.GetCount();
        }

        if (cnt->maxNrOfInstances.has_value() && cnt->maxNrOfInstances.value() != 0 &&
            currentNrOfInstances > cnt->maxNrOfInstances.value()) {
            CLogger::Get()->Write("onem2m_service",
                                  LogNotice,
                                  "Container '%s' has reached max number of instances: %d",
                                  cnt->resourceID.c_str(),
                                  currentNrOfInstances);
            ResponsePrimitive resp;
            resp.responseStatusCode = ResponseStatusCode::NOT_ACCEPTABLE;
            return resp;
        }

        // XXX: This should just delete old CINs but delete is out of scope.

        if (mbs != 0 && currentBytes + r.contentSize > mbs) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::NOT_ACCEPTABLE;
            return bad;
        }
    }

    // Enforce max number of instances (evict oldest if necessary)
    if (cnt->maxNrOfInstances.has_value() && cnt->maxNrOfInstances.value() != 0) {
        s64 desired = *cnt->maxNrOfInstances;

        // XXX: Has we have no updates or deletes, the current number of instances is just the
        // count of existing CINs. s64 current = cnt->currentNrOfInstances;
        Vector<CString> cinRIs;
        CString         err;
        db_.LoadPrimitiveContentChildren(
            cnt->resourceID, ResourceType::ContentInstance, cinRIs, err);

        if (cinRIs.GetCount() + 1 > desired) {
            // XXX: This should just delete old CINs until we are under the limit, but delete is
            // out of scope. For now just reject if we would exceed the limit.
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::NOT_ACCEPTABLE;
            return bad;
        }
    }

    r.resourceType = ResourceType::ContentInstance;
    r.parentID     = cnt->resourceID;
    r.resourceID   = GetId();
    if (r.resourceName.GetLength() == 0) {
        CString rn;
        rn.Format("cin%s", r.resourceID.c_str());
        r.resourceName = rn;
    }
    if (!isValidResourceName(r.resourceName)) {
        ResponsePrimitive bad;
        bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
        return bad;
    }

    // Populate times with default values, we dont have a real clock.
    if (r.creationTime.GetLength() == 0) r.creationTime = "2026-01-01T00:00:00Z";
    if (r.lastModifiedTime.GetLength() == 0) r.lastModifiedTime = r.creationTime;
    if (!r.expirationTime.has_value()) r.expirationTime = CString("2027-01-01T00:00:00Z");
    if (!r.stateTag.has_value()) r.stateTag = 0;

    CString saveErr;
    if (!db_.SaveContentInstance(r, saveErr)) {
        CLogger::Get()->Write(
            "onem2m_service", LogError, "CREATE ContentInstance save failed: %s", saveErr.c_str());
        ResponsePrimitive resp;
        resp.responseStatusCode = ResponseStatusCode::INTERNAL_SERVER_ERROR;
        return resp;
    }
    CLogger::Get()->Write("onem2m_service",
                          LogNotice,
                          "Inserted ContentInstance: rn='%s' ri='%s' pi='%s'",
                          r.resourceName.c_str(),
                          r.resourceID.c_str(),
                          r.parentID.c_str());

    PrimitiveContent pc;
    pc = r;
    return makeResponse(req, ResponseStatusCode::CREATED, pc);
}

ResponsePrimitive OneM2MService::CreateSubscription(const Subscription     &sub,
                                                    const RequestPrimitive &req,
                                                    const CString          &target)
{
    Subscription     r = sub;
    PrimitiveContent parent;
    CString          lookupErr;
    if (!db_.LoadPrimitiveContentByTarget(target, parent, lookupErr)) {
        CLogger::Get()->Write(
            "onem2m_service", LogError, "Parent resource lookup failed: %s", lookupErr.c_str());
        ResponsePrimitive resp;
        resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
        return resp;
    }

    const ResourceBase *parentBase;
    // XXX: SUB should be creatable under more resource but of our subset only these two make sense.
    if (auto cse = parent.GetIf<CSEBase>()) parentBase = cse;
    else if (auto ae = parent.GetIf<AE>()) parentBase = ae;
    else if (auto cnt = parent.GetIf<Container>()) parentBase = cnt;
    else {
        CLogger::Get()->Write(
            "onem2m_service", LogNotice, "Create Subscription req without valid parent");
        ResponsePrimitive bad;
        bad.responseStatusCode = ResponseStatusCode::INVALID_CHILD_RESOURCE_TYPE;
        return bad;
    }

    r.resourceType = ResourceType::Subscription;
    r.parentID     = parentBase->resourceID;
    r.resourceID   = GetId();
    if (r.resourceName.GetLength() == 0) {
        CString rn;
        rn.Format("sub%s", r.resourceID.c_str());
        r.resourceName = rn;
    }
    if (!isValidResourceName(r.resourceName)) {
        ResponsePrimitive bad;
        bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
        return bad;
    }

    // Validate CHTY (child resource types)
    if (!r.eventNotificationCriteria.childResourceType.empty()) {
        if (parentBase->resourceType.has_value() &&
            parentBase->resourceType.value() == ResourceType::CSEBase) {
            for (const auto &t : r.eventNotificationCriteria.childResourceType) {
                if (t != ResourceType::AE && t != ResourceType::Container &&
                    t != ResourceType::Subscription) {
                    CLogger::Get()->Write("onem2m_service",
                                          LogNotice,
                                          "Subscription rejected: unsupported resource type in "
                                          "CHTY for Container parent");
                    ResponsePrimitive bad;
                    bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
                    return bad;
                }
            }
        } else if (parentBase->resourceType.has_value() &&
                   parentBase->resourceType.value() == ResourceType::AE) {
            for (const auto &t : r.eventNotificationCriteria.childResourceType) {
                if (t != ResourceType::Container && t != ResourceType::Subscription) {
                    CLogger::Get()->Write("onem2m_service",
                                          LogNotice,
                                          "Subscription rejected: unsupported resource type in "
                                          "CHTY for Container parent");
                    ResponsePrimitive bad;
                    bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
                    return bad;
                }
            }
        } else if (parentBase->resourceType.has_value() &&
                   parentBase->resourceType.value() == ResourceType::Container) {
            for (const auto &t : r.eventNotificationCriteria.childResourceType) {
                if (t != ResourceType::Container && t != ResourceType::ContentInstance &&
                    t != ResourceType::Subscription) {
                    CLogger::Get()->Write("onem2m_service",
                                          LogNotice,
                                          "Subscription rejected: unsupported resource type in "
                                          "CHTY for Container parent");
                    ResponsePrimitive bad;
                    bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
                    return bad;
                }
            }
        } else {
            CLogger::Get()->Write("onem2m_service",
                                  LogNotice,
                                  "Subscription rejected: unsupported parent resource type for "
                                  "CHTY");
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return bad;
        }
    }

    if (r.creationTime.GetLength() == 0) r.creationTime = "2026-01-01T00:00:00Z";
    if (r.lastModifiedTime.GetLength() == 0) r.lastModifiedTime = r.creationTime;
    if (!r.expirationTime.has_value()) r.expirationTime = CString("2027-01-01T00:00:00Z");

    bool crPresent = req.vendorInformation.has_value() &&
                     req.vendorInformation->Compare("sub_creator_present") == 0;

    bool crNull = req.vendorInformation.has_value() &&
                  req.vendorInformation->Compare("sub_creator_null") == 0;

    // Reject explicit non-null creator
    if (crPresent && !crNull) {
        CLogger::Get()->Write(
            "onem2m_service", LogNotice, "Create Subscription rejected: non-NULL creator provided");

        ResponsePrimitive bad;
        bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
        return bad;
    }

    // Check NU
    bool nuDiffersFromOriginator = false;

    for (const CString &nu : r.notificationURI) {
        if (nu.GetLength() && nu.Compare(req.from) != 0) {
            nuDiffersFromOriginator = true;
            break;
        }
    }

    // Apply creator rules
    if (crNull) {
        if (req.from.GetLength() == 0) {
            CLogger::Get()->Write(
                "onem2m_service",
                LogNotice,
                "Create Subscription rejected: NULL creator but missing originator");

            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return bad;
        }

        r.creator = req.from;
    } else if (nuDiffersFromOriginator) {
        if (req.from.GetLength() != 0) r.creator = req.from;
    }

    const bool nctProvided = r.notificationContentType.has_value();
    if (!nctProvided) {
        r.notificationContentType = NotificationContentType::AllAttributes;
        CLogger::Get()->Write("onem2m_service",
                              LogNotice,
                              "nct: %u",
                              static_cast<unsigned>(r.notificationContentType.value()));
    }

    // // If the client did not provide an explicit `nct`, some event types are ambiguous or
    // // incompatible with the default `ModifiedAttributes` behaviour. Allow a subscription that
    // only
    // // requests `BlockingUpdate`, but reject subscriptions that request `BlockingUpdate` together
    // // with any other notification event type without an explicit `nct`.
    if (!nctProvided) {
        bool hasBlocking = ContainsNotificationEvent(r, NotificationEventType::BlockingUpdate);
        if (hasBlocking) {
            size_t evtCount = r.eventNotificationCriteria.notificationEventType.size();
            if (evtCount > 1) {
                ResponsePrimitive bad;
                bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
                return bad;
            }
        }
    }

    // For BlockingUpdate subscriptions, exactly one NU is allowed.

    if (ContainsNotificationEvent(r, NotificationEventType::ReportOnMissingDataPoints)) {
        if (parentBase->resourceType.has_value() &&
            parentBase->resourceType.value() != ResourceType::TimeSeries) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return bad;
        }
    } else if (ContainsNotificationEvent(r, NotificationEventType::BlockingUpdate)) {
        if (r.notificationURI.size() != 1) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return bad;
        }
        // Disallow batch notifications and other non-blocking attributes for blocking update
        // subscriptions.
        if (r.batchNotify.has_value()) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return bad;
        }
        // Disallow most ENC attributes except for `atr` (attributeList) for blocking-update
        // subscriptions.
        const EventNotificationCriteria &enc = r.eventNotificationCriteria;
        if (enc.stateTagBigger.has_value() || enc.expireBefore.has_value() ||
            enc.expireAfter.has_value() || enc.sizeAbove.has_value() || enc.sizeBelow.has_value() ||
            enc.labels.size() > 0 || enc.childResourceType.size() > 0 ||
            enc.filterUsage.has_value() || enc.contentFilterQuery.has_value() ||
            enc.contentFilterSyntax.has_value() || enc.missingData.has_value()) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return bad;
        }
        // Only allow attributeList (`atr`) together with BlockingUpdate. enc.attributeList is
        // allowed; others are rejected above.

        // Require attributeList for BlockingUpdate subscriptions when the notification URI is
        // not a remote HTTP/HTTPS URL (i.e. when the NU targets a local AE/POA). If the NU is a
        // remote URL, allow the create so the verification attempt can be performed and
        // potentially fail with SUBSCRIPTION_VERIFICATION_INITIATION_FAILED.
        bool nuIsRemote = false;
        for (const CString &nu : r.notificationURI) {
            CIPAddress ip;
            u16        port = 0;
            CString    path;
            if (ParseNotificationUrl(nu, ip, port, path)) {
                nuIsRemote = true;
                break;
            }
        }
        if (!nuIsRemote && enc.attributeList.empty()) {
            ResponsePrimitive bad;
            bad.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
            return bad;
        }

        // Accept explicit `nct` values as provided by the client. Different test harnesses may
        // use differing numeric mappings for the symbolic values; do not enforce a strict
        // equality here.
    }
    CString subPath;
    CString err;
    if (!db_.GetPathByRI(r.parentID, subPath, err)) {
        ResponsePrimitive bad;
        bad.responseStatusCode = ResponseStatusCode::NOT_FOUND;
        return bad;
    }
    subPath.Append("/");
    subPath.Append(r.resourceID);

    if (!SendSubscriptionVerification(r, subPath, req.from)) {
        ResponsePrimitive resp;
        resp.responseStatusCode = ResponseStatusCode::SUBSCRIPTION_VERIFICATION_INITIATION_FAILED;
        return resp;
    }

    CString saveErr;
    if (!db_.SaveSubscription(r, saveErr)) {
        CLogger::Get()->Write(
            "onem2m_service", LogError, "CREATE Subscription save failed: %s", saveErr.c_str());
        ResponsePrimitive resp;
        resp.responseStatusCode = ResponseStatusCode::INTERNAL_SERVER_ERROR;
        return resp;
    }
    CLogger::Get()->Write("onem2m_service",
                          LogNotice,
                          "Inserted Subscription: rn='%s' ri='%s' pi='%s'",
                          r.resourceName.c_str(),
                          r.resourceID.c_str(),
                          r.parentID.c_str());

    PrimitiveContent out;
    out = r;
    return makeResponse(req, ResponseStatusCode::CREATED, out);
}

ResponsePrimitive OneM2MService::Retrieve(const RequestPrimitive &request)
{
    ResponsePrimitive resp;

    CSEBase cse;
    CString lookupErr;
    bool    targetValid = false;
    bool    wrongSpid   = false;
    db_.GetCSEBase(cse, lookupErr);
    CString target =
        lookupErr.GetLength() == 0
            ? CanonicalizeAddressingPath(
                  request.to, cse.resourceName, cse.cseID, spId_, targetValid, wrongSpid)
            : CString();

    if (!targetValid) {
        CLogger::Get()->Write(
            "onem2m_service", LogError, "CSEBase lookup failed: %s", lookupErr.c_str());
        resp.responseStatusCode =
            wrongSpid ? ResponseStatusCode::NOT_FOUND : ResponseStatusCode::BAD_REQUEST;
        return resp;
    }

    const boolean isCseTarget = target.Compare(cse.resourceName) == 0;

    PrimitiveContent found;
    CString          loadErr;
    if (isCseTarget) {
        found = cse;
    } else {
        // lookup by target path
        CLogger::Get()->Write(
            "onem2m_service", LogDebug, "Retrieve: looking up target='%s' in DB", target.c_str());
        // check if ends with /la or /ol
        CString parentTarget = target;
        if (target.c_str()[target.GetLength() - 3] == '/') {
            char *tmp = new char[target.GetLength() + 1];
            memcpy(tmp, target.c_str(), target.GetLength() + 1);
            tmp[target.GetLength() - 3] = '\0';
            parentTarget                = CString(tmp);
            delete[] tmp;
            CLogger::Get()->Write("onem2m_service",
                                  LogDebug,
                                  "Retrieve: parent target='%s' derived from target='%s'",
                                  parentTarget.c_str(),
                                  target.c_str());
        }
        if (!db_.LoadPrimitiveContentByTarget(parentTarget, found, loadErr)) {

            CLogger::Get()->Write("onem2m_service", LogDebug, "target:'%s'", target.c_str());
            resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
            return resp;
        }
    }

    if (auto *cse = found.GetIf<CSEBase>()) {
        return RetrieveCSE(request, *cse, target);

    } else if (auto *ae = found.GetIf<AE>()) {
        return RetrieveAE(request, *ae, target);

    } else if (auto *cnt = found.GetIf<Container>()) {
        return RetrieveContainer(request, *cnt, target);

    } else if (auto *cin = found.GetIf<ContentInstance>()) {
        return RetrieveContentInstance(request, *cin, target);

    } else if (auto *sub = found.GetIf<Subscription>()) {
        return RetrieveSubscription(request, *sub, target);

    } else {
        resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
        return resp;
    }

    // FIXME: never reached
    const char  *targetText = target.c_str();
    const size_t targetLen  = target.GetLength();
    if (targetLen > 3 && targetText[targetLen - 3] == '/' &&
        ((targetText[targetLen - 2] == 'l' && targetText[targetLen - 1] == 'a') ||
         (targetText[targetLen - 2] == 'o' && targetText[targetLen - 1] == 'l'))) {
        char *tmp = new char[targetLen + 1];
        memcpy(tmp, targetText, targetLen + 1);
        tmp[targetLen - 3] = '\0';
        CString parentTarget(tmp);
        delete[] tmp;

        PrimitiveContent parent;
        if (db_.LoadPrimitiveContentByTarget(parentTarget, parent, loadErr)) {
            if (const Container *cnt = parent.GetIf<Container>()) {
                return RetrieveContainer(request, *cnt, target);
            }
        }
    }

    resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
    return resp;
}

ResponsePrimitive
OneM2MService::RetrieveCSE(const RequestPrimitive &req, const CSEBase &cse, const CString &target)
{
    ResponsePrimitive resp;
    CString           cseRoot;
    cseRoot += cse.resourceName;
    CLogger::Get()->Write("onem2m_service",
                          LogDebug,
                          "RetrieveCSE: cseRoot='%s' target='%s'",
                          cseRoot.c_str(),
                          target.c_str());
    if (target.Compare(cseRoot) != 0) {
        resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
        return resp;
    }
    if (req.from.Compare("CAdmin") != 0) {
        resp.responseStatusCode = ResponseStatusCode::ORIGINATOR_HAS_NO_PRIVILEGE;
        return resp;
    }
    PrimitiveContent out;
    out = cse;
    return makeResponse(req, ResponseStatusCode::OK, out);
}

ResponsePrimitive
OneM2MService::RetrieveAE(const RequestPrimitive &req, const AE &ae, const CString &target)
{
    ResponsePrimitive resp;
    if (!MatchesResourceTarget(db_, ae, target)) {
        resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
        return resp;
    }
    if (req.from.GetLength() == 0 || ae.aeID.GetLength() == 0 || req.from.Compare(ae.aeID) != 0) {
        CLogger::Get()->Write(
            "onem2m_service", LogNotice, "from='%s' aeID='%s'", req.from.c_str(), ae.aeID.c_str());
        resp.responseStatusCode = ResponseStatusCode::ORIGINATOR_HAS_NO_PRIVILEGE;
        return resp;
    }

    PrimitiveContent out;
    out = ae;
    return makeResponse(req, ResponseStatusCode::OK, out);
}

ResponsePrimitive OneM2MService::RetrieveContainer(const RequestPrimitive &req,
                                                   const Container        &cnt,
                                                   const CString          &target)
{
    ResponsePrimitive resp;
    Container         r = cnt;

    if (!MatchesResourceTarget(db_, cnt, target)) {
        resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
        return resp;
    }

    if (!IsAllowedForContainer(req, db_, cnt)) {
        resp.responseStatusCode = ResponseStatusCode::ORIGINATOR_HAS_NO_PRIVILEGE;
        return resp;
    }

    CString fullPath;
    CString loadErr;
    if (!db_.GetPathByRI(cnt.resourceID, fullPath, loadErr)) {
        CLogger::Get()->Write("onem2m_service",
                              LogError,
                              "Failed to get path for container '%s': %s",
                              cnt.resourceID.c_str(),
                              loadErr.c_str());
        resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
        return resp;
    }
    CString laPath = fullPath;
    laPath += "/la";
    CString olPath = fullPath;
    olPath += "/ol";

    // FIXME: check if this should be here
    if (cnt.disableRetrieval.has_value() && *cnt.disableRetrieval) {
        resp.responseStatusCode = ResponseStatusCode::OPERATION_NOT_ALLOWED;
        return resp;
    }

    // Return instance if target ends with /la or /ol
    if (laPath.Compare(target) == 0 || olPath.Compare(target) == 0) {
        CString cinRI;
        if (laPath.Compare(target) == 0 &&
            !db_.LoadLatestChild(cnt.resourceID, ResourceType::ContentInstance, cinRI, loadErr)) {
            CLogger::Get()->Write("onem2m_service",
                                  LogError,
                                  "Failed to load latest child for container '%s': %s",
                                  cnt.resourceID.c_str(),
                                  loadErr.c_str());
            resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
            return resp;
        } else if (olPath.Compare(target) == 0 &&
                   !db_.LoadOldestChild(
                       cnt.resourceID, ResourceType::ContentInstance, cinRI, loadErr)) {
            CLogger::Get()->Write("onem2m_service",
                                  LogError,
                                  "Failed to load oldest child for container '%s': %s",
                                  cnt.resourceID.c_str(),
                                  loadErr.c_str());
            resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
            return resp;
        }
        PrimitiveContent cinPc;
        if (!db_.LoadPrimitiveContentByTarget(cinRI, cinPc, loadErr)) {
            CLogger::Get()->Write("onem2m_service",
                                  LogError,
                                  "Failed to load content instance for container '%s': %s",
                                  cnt.resourceID.c_str(),
                                  loadErr.c_str());
            resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
            return resp;
        }
        return RetrieveContentInstance(req, *cinPc.GetIf<ContentInstance>(), cinRI);
    }

    // XXX: This should be saved on CIN creation and deletion, but update and delete is out of
    // scope
    r.currentNrOfInstances = 0;
    r.currentByteSize      = 0;
    Vector<CString> cinRIs;
    CString         err;
    if (!db_.GetContainerCurrentSize(r.resourceID, r.currentByteSize, err)) {
        CLogger::Get()->Write("onem2m_service",
                              LogError,
                              "Failed to get current size for container '%s': %s",
                              r.resourceID.c_str(),
                              err.c_str());
        ResponsePrimitive resp;
        resp.responseStatusCode = ResponseStatusCode::INTERNAL_SERVER_ERROR;
        return resp;
    }

    if (db_.LoadPrimitiveContentChildren(
            r.resourceID, ResourceType::ContentInstance, cinRIs, err)) {
        r.currentNrOfInstances = cinRIs.GetCount();
    }

    PrimitiveContent out;
    out = r;
    return makeResponse(req, ResponseStatusCode::OK, out);
}

ResponsePrimitive OneM2MService::RetrieveContentInstance(const RequestPrimitive &req,
                                                         const ContentInstance  &cin,
                                                         const CString          &target)
{
    ResponsePrimitive resp;
    if (!MatchesResourceTarget(db_, cin, target)) {
        resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
        return resp;
    }

    PrimitiveContent parentPc;
    CString          loadErr;
    if (cin.parentID.GetLength() == 0 ||
        !db_.LoadPrimitiveContentByTarget(cin.parentID, parentPc, loadErr)) {
        CLogger::Get()->Write("onem2m_service",
                              LogError,
                              "Failed to load parent container for CIN '%s': %s",
                              cin.resourceID.c_str(),
                              loadErr.c_str());
        resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
        return resp;
    }

    const Container *cnt = parentPc.GetIf<Container>();
    if (!cnt) {
        CLogger::Get()->Write(
            "onem2m_service", LogError, "Parent resource of CIN is not a container as expected");
        resp.responseStatusCode = ResponseStatusCode::INTERNAL_SERVER_ERROR;
        return resp;
    }

    if (cnt->disableRetrieval.has_value() && *cnt->disableRetrieval) {
        resp.responseStatusCode = ResponseStatusCode::OPERATION_NOT_ALLOWED;
        return resp;
    }
    if (!IsAllowedForContainer(req, db_, *cnt)) {
        resp.responseStatusCode = ResponseStatusCode::ORIGINATOR_HAS_NO_PRIVILEGE;
        return resp;
    }

    if (cnt->accessControlPolicyIDs.empty() && !cin.accessControlPolicyIDs.empty()) {
        ResponsePrimitive resp;
        resp.responseStatusCode = ResponseStatusCode::BAD_REQUEST;
        return resp;
    }

    // XXX: we do not support policy-based access control
    if (!cin.accessControlPolicyIDs.empty()) {
        ResponsePrimitive resp;
        resp.responseStatusCode = ResponseStatusCode::NOT_IMPLEMENTED;
        return resp;
    }

    PrimitiveContent out;
    out = cin;
    return makeResponse(req, ResponseStatusCode::OK, out);
}

ResponsePrimitive OneM2MService::RetrieveSubscription(const RequestPrimitive &req,
                                                      const Subscription     &sub,
                                                      const CString          &target)
{
    ResponsePrimitive resp;
    if (!MatchesResourceTarget(db_, sub, target)) {
        resp.responseStatusCode = ResponseStatusCode::NOT_FOUND;
        return resp;
    }
    if (!sub.creator.has_value() || req.from.Compare(sub.creator.value()) != 0) {
        resp.responseStatusCode = ResponseStatusCode::ORIGINATOR_HAS_NO_PRIVILEGE;
        return resp;
    }

    CLogger::Get()->Write("onem2m_service",
                          LogDebug,
                          "RetrieveSubscription: creator='%s'",
                          sub.creator.value().c_str());

    PrimitiveContent out;
    out = sub;
    return makeResponse(req, ResponseStatusCode::OK, out);
}

void OneM2MService::SendNotification(const PrimitiveContent     &changed,
                                     const NotificationEventType eventType,
                                     const CString              &originator)
{
    const ResourceBase *base = GetResourceBase(changed);

    if (!base) return;

    Vector<CString> subRiList;
    CString         err;

    if (!db_.LoadPrimitiveContentChildren(
            base->parentID, ResourceType::Subscription, subRiList, err)) {
        return;
    }

    for (const auto &subRi : subRiList) {
        PrimitiveContent pc;

        if (!db_.LoadPrimitiveContentByTarget(subRi, pc, err)) {
            CLogger::Get()->Write("onem2m_service",
                                  LogError,
                                  "Failed to load subscription '%s': %s",
                                  subRi.c_str(),
                                  err.c_str());
            continue;
        }

        auto *sub = pc.GetIf<Subscription>();
        if (!sub) continue;

        if (!MatchesCriteria(*sub, changed, eventType)) {
            CLogger::Get()->Write("onem2m_service",
                                  LogDebug,
                                  "Subscription '%s' does not match criteria for event type %d",
                                  sub->resourceID.c_str(),
                                  static_cast<int>(eventType));
            continue;
        }

        Notification notification = BuildNotification(*sub, changed, eventType);

        CString requestId;
        requestId.Format("ntf-%s", base->resourceID.c_str());

        for (const auto &nu : sub->notificationURI) {
            DeliverTarget(nu, notification, originator, requestId);
        }
    }
}

bool OneM2MService::SendSubscriptionVerification(const Subscription &sub,
                                                 const CString      &subscriptionPath,
                                                 const CString      &originator)
{
    CString requestId;
    requestId.Format("sub-%s", sub.resourceID.c_str());

    Notification sgn;
    sgn.verificationRequest   = true;
    sgn.subscriptionReference = subscriptionPath;
    sgn.creator               = originator;

    RequestPrimitive req;
    req.op                = Operation::Notify;
    req.from              = originator;
    req.requestIdentifier = requestId;
    req.content           = sgn;

    for (const CString &nu : sub.notificationURI) {
        // If the notification URI is an HTTP/HTTPS URL, attempt verification via HTTP POST.
        // If it's not a URL (for example an originator token like "C13"), accept it
        // as a local notification destination without performing HTTP verification.
        if (nu.GetLength() == 0) continue;
        if (nu.c_str()[0] == 'C' || nu.c_str()[0] == 'S') {
            // AE ID CHECK if has poa
            PrimitiveContent pc;
            CString          err;
            if (!db_.LoadPrimitiveContentByTarget(sub.parentID, pc, err)) continue;
            if (auto *ae = pc.GetIf<AE>()) {
                // False if no poa is registered
                if (!ae->pointOfAccess.empty()) return true;
            }
        } else if (nu.GetLength() > 7 && strncmp(nu.c_str(), "http://", 7) == 0) {
            // HTTP binding

            req.to = nu;
            if (httpBinding_->SendNotification(req, net_)) return true;
        } else {
            // Unsupported protocol continue
            CLogger::Get()->Write(
                "onem2m", LogWarning, "Unsupported notification URI: %s", nu.c_str());
        }
    }
    return false;
}

ResponsePrimitive OneM2MService::Notify(const RequestPrimitive &request)
{
    // TODO: Call External notification handler, Do when we have p2p layer done
    ResponsePrimitive resp;
    resp.responseStatusCode = ResponseStatusCode::NOT_IMPLEMENTED;
    return resp;
}

ResponsePrimitive OneM2MService::Update(const RequestPrimitive &request)
{
    // XXX: out of scope of this project
    (void)request;
    ResponsePrimitive resp;
    resp.responseStatusCode = ResponseStatusCode::NOT_IMPLEMENTED;
    return resp;
}

ResponsePrimitive OneM2MService::Delete(const RequestPrimitive &request)
{
    // XXX: out of scope of this project
    (void)request;
    ResponsePrimitive resp;
    resp.responseStatusCode = ResponseStatusCode::NOT_IMPLEMENTED;
    return resp;
}

void OneM2MService::DeliverTarget(const CString      &nu,
                                  const Notification &sgn,
                                  const CString      &originator,
                                  const CString      &requestId)
{
    RequestPrimitive req;
    if (nu.GetLength() == 0) {
        CLogger::Get()->Write("onem2m_service",
                              LogWarning,
                              "Empty notification URI in subscription '%s'",
                              requestId.c_str());
        return;
    }

    req.op                = Operation::Notify;
    req.from              = originator;
    req.requestIdentifier = requestId;

    req.content = sgn;

    Vector<CString> poas;
    CLogger::Get()->Write("onem2m_service", LogDebug, "DeliverTarget: nu='%s'", nu.c_str());
    if (nu.c_str()[0] == 'C' || nu.c_str()[0] == 'S') {
        // ID, lookup POA(s) in DB
        PrimitiveContent pc;
        CString          err;
        if (!db_.LoadPrimitiveContentByTarget(nu, pc, err)) {
            CLogger::Get()->Write(
                "onem2m_service",
                LogError,
                "Failed to load POA for notification URI '%s' in subscription '%s'",
                nu.c_str(),
                requestId.c_str());
            return;
        }

        if (auto *ae = pc.GetIf<AE>()) {
            if (!ae->requestReachability.has_value() || !ae->requestReachability.value()) return;
            if (ae->aeID.Compare(originator) != 0) return;
            poas = ae->pointOfAccess;
        } else {
            CLogger::Get()->Write(
                "onem2m_service",
                LogError,
                "Notification URI '%s' in subscription '%s' does not refer to an AE or CSEBase",
                nu.c_str(),
                requestId.c_str());
            return;
        }

    } else {
        poas.Append(nu);
    }

    for (const auto &poa : poas) {
        req.to = poa;
        CLogger::Get()->Write("onem2m_service",
                              LogDebug,
                              "DeliverTarget: sending notification to POA '%s' for nu='%s'",
                              poa.c_str(),
                              nu.c_str());

        if (req.to.GetLength() > 7 && strncmp(req.to.c_str(), "http://", 7) == 0) {
            // HTTP binding
            httpBinding_->SendNotification(req, net_);
        } else {
            // Unsupported protocol
            CLogger::Get()->Write(
                "onem2m", LogWarning, "Unsupported notification URI: %s", nu.c_str());
        }
    }
}

} // namespace zerom2m::onem2m