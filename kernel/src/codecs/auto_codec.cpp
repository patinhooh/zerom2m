/*
 * auto_codec.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "zerom2m/codecs/auto_codec.h"

#include "zerom2m/codecs/json.h"
#include "zerom2m/compat/collections.h"
#include "zerom2m/onem2m/types/enums.h"
#include "zerom2m/onem2m/types/short_names.h"

#include <circle/logger.h>
#include <circle/types.h>
#include <circle/util.h>

// TODO: This is just a very basic implementation to get something working.

namespace zerom2m::codecs
{

using namespace zerom2m::compat;
using namespace zerom2m::onem2m::types;

namespace
{
// CString WrapJsonResource(const char *shortName, const CString &payload)
// {
//     CString out;
//     out.Append("{\"m2m:");
//     out.Append(shortName);
//     out.Append("\":");
//     out.Append(payload);
//     out.Append("}");
//     return out;
// }

void AppendVectorOfStringsField(CString               &out,
                                bool                  &first,
                                const char            *name,
                                const Vector<CString> &values)
{
    if (values.empty()) { return; }

    if (!first) { out.Append(","); }
    first = false;

    out.Append("\"");
    out.Append(name);
    out.Append("\":[");
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) { out.Append(","); }
        out.Append("\"");
        json::AppendEscapedString(out, values[i].c_str());
        out.Append("\"");
    }
    out.Append("]");
}

void AppendOptionalStringField(CString                 &out,
                               bool                    &first,
                               const char              *name,
                               const Optional<CString> &value)
{
    if (!value.has_value() || value->GetLength() == 0) { return; }
    json::AppendStringField(out, first, name, value->c_str());
}

template <typename T>
void AppendOptionalUnsignedField(CString           &out,
                                 bool              &first,
                                 const char        *name,
                                 const Optional<T> &value)
{
    if (!value.has_value()) { return; }

    if (!first) { out.Append(","); }
    first = false;

    CString number;
    number.Format("%u", static_cast<unsigned>(*value));
    out.Append("\"");
    out.Append(name);
    out.Append("\":");
    out.Append(number);
}

template <typename T>
void AppendOptionalSignedField(CString           &out,
                               bool              &first,
                               const char        *name,
                               const Optional<T> &value)
{
    if (!value.has_value()) { return; }

    if (!first) { out.Append(","); }
    first = false;

    CString number;
    number.Format("%d", static_cast<int>(*value));
    out.Append("\"");
    out.Append(name);
    out.Append("\":");
    out.Append(number);
}

template <typename T>
void AppendOptionalBoolField(CString &out, bool &first, const char *name, const Optional<T> &value)
{
    if (!value.has_value()) { return; }
    json::AppendBoolField(out, first, name, static_cast<bool>(*value));
}

CString BuildBaseResourceJson(const ResourceBase &resource, const char *shortName)
{
    CString body;
    body.Append("{\"m2m:");
    body.Append(shortName);
    body.Append("\":{");

    bool first = true;
    json::AppendStringField(body,
                            first,
                            sn::attr::RESOURCE_NAME,
                            resource.resourceName.GetLength() == 0 ? nullptr
                                                                   :
                                                                   resource.resourceName.c_str());
    json::AppendStringField(body,
                            first,
                            sn::attr::RESOURCE_ID,
                            resource.resourceID.GetLength() == 0 ? nullptr
                                                                 : resource.resourceID.c_str());
    if (resource.resourceType.has_value()) {
        json::AppendUnsignedField(
            body, first, sn::attr::RESOURCE_TYPE, static_cast<unsigned>(*resource.resourceType));
    }
    json::AppendStringField(body,
                            first,
                            sn::attr::PARENT_ID,
                            resource.parentID.GetLength() == 0 ? nullptr
                                                               : resource.parentID.c_str());
    json::AppendStringField(body,
                            first,
                            sn::attr::CREATION_TIME,
                            resource.creationTime.GetLength() == 0 ? nullptr
                                                                   :
                                                                   resource.creationTime.c_str());
    json::AppendStringField(
        body,
        first,
        sn::attr::LAST_MODIFIED_TIME,
        resource.lastModifiedTime.GetLength() == 0 ? nullptr :
        resource.lastModifiedTime.c_str());
    AppendOptionalStringField(body, first, sn::attr::EXPIRATION_TIME, resource.expirationTime);
    AppendVectorOfStringsField(body, first, sn::attr::LABELS, resource.labels);
    AppendVectorOfStringsField(
        body, first, sn::attr::ACCESS_CONTROL_POLICY_IDS, resource.accessControlPolicyIDs);
    AppendOptionalStringField(body, first, sn::attr::CUSTODIAN, resource.custodian);
    AppendVectorOfStringsField(
        body, first, sn::attr::DYNAMIC_AUTH_CONSULT_IDS, resource.dynamicAuthConsultIDs);
    AppendOptionalStringField(body, first, sn::attr::ANNOUNCE_TO, resource.announceTo);
    AppendVectorOfStringsField(
        body, first, sn::attr::ANNOUNCED_ATTRIBUTE, resource.announcedAttribute);
    AppendOptionalUnsignedField(
        body, first, sn::attr::ANNOUNCE_SYNC_TYPE, resource.announceSyncType);

    body.Append("}}");
    return body;
}

// CString BuildResourceNameJson(const ResourceBase &resource, const char *shortName)
// { return BuildBaseResourceJson(resource, shortName); }
} // namespace

CString AutoCodec::NormalizeMimeType(const CString &mimeType) const
{
    const char *raw = mimeType.c_str();
    if (raw == nullptr) { return CString{}; }

    const char  *semi = strchr(raw, ';');
    const size_t len  = semi == nullptr ? strlen(raw) : static_cast<size_t>(semi - raw);

    CString normalized;
    for (size_t i = 0; i < len; ++i) {
        const char ch = raw[i];
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') { continue; }
        normalized += ch;
    }
    return normalized;
}

bool AutoCodec::IsJsonMime(const CString &mimeType) const
{
    const CString normalized = NormalizeMimeType(mimeType);
    return normalized.Compare(mime::JSON) == 0 || normalized.Compare(mime::JSON_M2M) == 0;
}

bool AutoCodec::DeserializeRequestBody(const CString    &body,
                                       const CString    &mimeType,
                                       RequestPrimitive &prim) const
{
    if (!IsJsonMime(mimeType) || body.GetLength() == 0) {
        CLogger::Get()->Write(
            "auto_codec", LogDebug, "DeserializeRequestBody: unsupported mime or empty body");
        return false;
    }

    const char *jsonText = body.c_str();
    if (jsonText == nullptr || *jsonText == '\0') { return false; }

    char buffer[1024]{};

    if (json::FindKey(jsonText, "m2m:ae") != nullptr) {
        AE ae;
        if (json::ExtractStringValue(jsonText, sn::attr::RESOURCE_NAME, buffer, sizeof(buffer))) {
            ae.resourceName = buffer;
        }
        if (json::ExtractStringValue(jsonText, sn::attr::APP_ID, buffer, sizeof(buffer))) {
            ae.appID = buffer;
        }
        if (json::ExtractStringValue(jsonText, sn::attr::AE_ID, buffer, sizeof(buffer))) {
            ae.aeID = buffer;
        }
        if (json::ExtractStringValue(jsonText, sn::attr::APP_NAME, buffer, sizeof(buffer))) {
            ae.appName = buffer;
        }
        bool rr = false;
        if (json::ExtractBoolValue(jsonText, sn::attr::REQUEST_REACHABILITY, rr)) {
            ae.requestReachability = rr;
        }
        if (json::ExtractStringArrayValue(jsonText, sn::attr::POINT_OF_ACCESS, ae.pointOfAccess)) {
            // parsed array
        }
        prim.content = ae;
        return true;
    }

    if (json::FindKey(jsonText, "m2m:cnt") != nullptr) {
        Container cnt;
        if (json::ExtractStringValue(jsonText, sn::attr::RESOURCE_NAME, buffer, sizeof(buffer))) {
            cnt.resourceName = buffer;
        }

        unsigned number = 0;
        if (json::ExtractUnsignedValue(jsonText, sn::attr::MAX_NR_OF_INSTANCES, number)) {
            cnt.maxNrOfInstances = static_cast<s64>(number);
        }
        if (json::ExtractUnsignedValue(jsonText, sn::attr::MAX_BYTE_SIZE, number)) {
            cnt.maxByteSize = static_cast<s64>(number);
        }
        if (json::ExtractUnsignedValue(jsonText, sn::attr::MAX_INSTANCE_AGE, number)) {
            cnt.maxInstanceAge = static_cast<s64>(number);
        }

        prim.content = cnt;
        return true;
    }

    if (json::FindKey(jsonText, "m2m:cin") != nullptr) {
        ContentInstance cin;
        if (json::ExtractStringValue(jsonText, sn::attr::RESOURCE_NAME, buffer, sizeof(buffer))) {
            cin.resourceName = buffer;
        }
        if (json::ExtractStringValue(jsonText, sn::attr::CONTENT_INFO, buffer, sizeof(buffer))) {
            cin.contentInfo = buffer;
        }
        if (json::ExtractStringValue(jsonText, sn::attr::CONTENT, buffer, sizeof(buffer))) {
            cin.content = buffer;
        }
        unsigned number = 0;
        if (json::ExtractUnsignedValue(jsonText, sn::attr::CONTENT_SIZE, number)) {
            cin.contentSize = static_cast<s64>(number);
        }
        prim.content = cin;
        return true;
    }

    if (json::FindKey(jsonText, "m2m:sub") != nullptr) {
        Subscription sub;
        if (json::ExtractStringValue(jsonText, sn::attr::RESOURCE_NAME, buffer, sizeof(buffer))) {
            sub.resourceName = buffer;
        }
        if (json::ExtractStringArrayValue(
                jsonText, sn::attr::NOTIFICATION_URI, sub.notificationURI)) {
            // parsed array
        }
        prim.content = sub;
        return true;
    }

    if (json::FindKey(jsonText, "m2m:grp") != nullptr) {
        Group grp;
        if (json::ExtractStringValue(jsonText, sn::attr::RESOURCE_NAME, buffer, sizeof(buffer))) {
            grp.resourceName = buffer;
        }

        unsigned number = 0;
        if (json::ExtractUnsignedValue(jsonText, sn::attr::MAX_NR_OF_MEMBERS, number)) {
            grp.maxNrOfMembers = static_cast<s32>(number);
        }
        if (json::ExtractStringArrayValue(jsonText, sn::attr::MEMBER_IDS, grp.memberIDs)) {
            // parsed array
        }

        prim.content = grp;
        return true;
    }

    CLogger::Get()->Write(
        "auto_codec", LogDebug, "DeserializeRequestBody: unrecognized JSON payload");
    return false;
}

CString AutoCodec::SerializeResource(const ResourceBase &resource, const CString &mimeType) const
{
    if (!IsJsonMime(mimeType)) { return CString{}; }

    if (!resource.resourceType.has_value()) { return CString{}; }

    const char *shortName = shortNameByResourceType((ResourceType)*resource.resourceType);
    if (shortName == nullptr) { return CString{}; }

    return BuildBaseResourceJson(resource, shortName);
}

CString AutoCodec::SerializePrimitiveContent(const PrimitiveContent &content,
                                             const CString          &mimeType) const
{
    if (!IsJsonMime(mimeType) || content.empty()) { return CString{}; }

    if (const auto *ae = content.GetIf<AE>()) { return SerializeResource(*ae, mimeType); }
    if (const auto *cnt = content.GetIf<Container>()) { return SerializeResource(*cnt, mimeType); }
    if (const auto *cin = content.GetIf<ContentInstance>()) {
        return SerializeResource(*cin, mimeType);
    }
    if (const auto *grp = content.GetIf<Group>()) { return SerializeResource(*grp, mimeType); }
    if (const auto *sub = content.GetIf<Subscription>()) {
        return SerializeResource(*sub, mimeType);
    }
    if (const auto *cse = content.GetIf<CSEBase>()) { return SerializeResource(*cse, mimeType); }
    if (const auto *csr = content.GetIf<RemoteCSE>()) { return SerializeResource(*csr, mimeType); }
    if (const auto *req = content.GetIf<RequestResource>()) {
        return SerializeResource(*req, mimeType);
    }
    if (const auto *node = content.GetIf<Node>()) { return SerializeResource(*node, mimeType); }
    if (const auto *pch = content.GetIf<PollingChannel>()) {
        return SerializeResource(*pch, mimeType);
    }
    if (const auto *sch = content.GetIf<Schedule>()) { return SerializeResource(*sch, mimeType); }
    if (const auto *mgc = content.GetIf<MgmtCmd>()) { return SerializeResource(*mgc, mimeType); }
    if (const auto *exin = content.GetIf<ExecInstance>()) {
        return SerializeResource(*exin, mimeType);
    }
    if (const auto *ts = content.GetIf<TimeSeries>()) { return SerializeResource(*ts, mimeType); }
    if (const auto *tsi = content.GetIf<TimeSeriesInstance>()) {
        return SerializeResource(*tsi, mimeType);
    }

    return CString{};
}

} // namespace zerom2m::codecs
