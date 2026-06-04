/*
 * json_codec.cpp
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#include "json_codec.h"

#include <zerom2m/compat/utils.h>
#include <zerom2m/onem2m/types/short_names.h>

#include <circle/logger.h>

namespace zerom2m::serde::json
{

using namespace zerom2m::onem2m::types;
using namespace zerom2m::onem2m::types::sn;
using namespace zerom2m::compat;

// Low-level helpers
CString JsonCodec::GetString(const JsonValue &obj, const char *key) const
{
    const JsonValue *v = obj.GetMember(key);
    if (!v) return CString{};
    const CString *s = v->GetString();
    return s ? *s : CString{};
}

void JsonCodec::GetOptString(const JsonValue &obj, const char *key, Optional<CString> &opt) const
{
    const JsonValue *v = obj.GetMember(key);
    if (!v) return;
    const CString *s = v->GetString();
    if (s) opt = *s;
}

void JsonCodec::GetOptBool(const JsonValue &obj, const char *key, Optional<boolean> &opt) const
{
    const JsonValue *v = obj.GetMember(key);
    if (!v) return;
    auto b = v->GetBoolean();
    if (b.has_value()) opt = *b;
}

void JsonCodec::GetOptS32(const JsonValue &obj, const char *key, Optional<s32> &opt) const
{
    const JsonValue *v = obj.GetMember(key);
    if (!v) return;
    auto n = v->GetNumber();
    if (n.has_value()) opt = static_cast<s32>(*n);
}

void JsonCodec::GetOptS64(const JsonValue &obj, const char *key, Optional<s64> &opt) const
{
    const JsonValue *v = obj.GetMember(key);
    if (!v) return;
    auto n = v->GetNumber();
    if (n.has_value()) opt = static_cast<s64>(*n);
}

void JsonCodec::GetStringArray(const JsonValue &obj, const char *key, Vector<CString> &out) const
{
    const JsonValue *arr = obj.GetMember(key);
    if (!arr || arr->GetType() != JSON_ARRAY) return;
    for (size_t i = 0; i < arr->GetArraySize(); ++i) {
        const JsonValue *elem = arr->GetElement(i);
        if (!elem) continue;
        const CString *s = elem->GetString();
        if (s) out.push_back(*s);
    }
}

JsonValue *JsonCodec::MakeStringArray(const Vector<CString> &v) const
{
    JsonValue *arr = new JsonValue(JSON_ARRAY);
    for (size_t i = 0; i < v.GetCount(); ++i)
        arr->AppendElement(new JsonValue(v[i]));
    return arr;
}

JsonValue *JsonCodec::SerializeResourceBase(const ResourceBase &r) const
{
    JsonValue *obj = new JsonValue(JSON_OBJECT);

    if (r.resourceName.GetLength())
        obj->AddMember(attr::RESOURCE_NAME, new JsonValue(r.resourceName));
    if (r.resourceID.GetLength()) obj->AddMember(attr::RESOURCE_ID, new JsonValue(r.resourceID));
    if (r.parentID.GetLength()) obj->AddMember(attr::PARENT_ID, new JsonValue(r.parentID));
    if (r.creationTime.GetLength())
        obj->AddMember(attr::CREATION_TIME, new JsonValue(r.creationTime));
    if (r.lastModifiedTime.GetLength())
        obj->AddMember(attr::LAST_MODIFIED_TIME, new JsonValue(r.lastModifiedTime));
    if (r.resourceType.has_value())
        obj->AddMember(attr::RESOURCE_TYPE,
                       new JsonValue(static_cast<double>((u32)*r.resourceType)));
    if (r.expirationTime.has_value())
        obj->AddMember(attr::EXPIRATION_TIME, new JsonValue(*r.expirationTime));
    if (!r.labels.empty()) obj->AddMember(attr::LABELS, MakeStringArray(r.labels));
    if (!r.accessControlPolicyIDs.empty())
        obj->AddMember(attr::ACCESS_CONTROL_POLICY_IDS, MakeStringArray(r.accessControlPolicyIDs));
    if (r.custodian.has_value()) obj->AddMember(attr::CUSTODIAN, new JsonValue(*r.custodian));
    if (!r.announcedAttribute.empty())
        obj->AddMember(attr::ANNOUNCED_ATTRIBUTE, MakeStringArray(r.announcedAttribute));
    if (r.announceTo.has_value()) obj->AddMember(attr::ANNOUNCE_TO, new JsonValue(*r.announceTo));

    return obj;
}

JsonValue *JsonCodec::SerializeAE(const AE &r) const
{
    JsonValue *obj = SerializeResourceBase(r);

    obj->AddMember(attr::APP_ID, new JsonValue(r.appID));
    if (r.aeID.GetLength()) obj->AddMember(attr::AE_ID, new JsonValue(r.aeID));
    if (r.appName.has_value()) obj->AddMember(attr::APP_NAME, new JsonValue(*r.appName));
    if (!r.pointOfAccess.empty())
        obj->AddMember(attr::POINT_OF_ACCESS, MakeStringArray(r.pointOfAccess));
    if (r.requestReachability.has_value())
        obj->AddMember(attr::REQUEST_REACHABILITY,
                       new JsonValue(static_cast<boolean>(*r.requestReachability)));
    if (!r.contentSerialization.empty())
        obj->AddMember(attr::CONTENT_SERIALIZATION, MakeStringArray(r.contentSerialization));
    if (r.nodeLink.has_value()) obj->AddMember(attr::NODE_LINK, new JsonValue(*r.nodeLink));
    if (!r.supportedReleaseVersions.empty())
        obj->AddMember(attr::SUPPORTED_RELEASE_VERSIONS,
                       MakeStringArray(r.supportedReleaseVersions));

    return obj;
}

JsonValue *JsonCodec::SerializeContainer(const Container &r) const
{
    JsonValue *obj = SerializeResourceBase(r);

    if (r.stateTag.has_value())
        obj->AddMember(attr::STATE_TAG, new JsonValue(static_cast<double>(*r.stateTag)));
    if (r.creator.has_value()) obj->AddMember(attr::CREATOR, new JsonValue(*r.creator));
    if (r.maxNrOfInstances.has_value())
        obj->AddMember(attr::MAX_NR_OF_INSTANCES,
                       new JsonValue(static_cast<double>(*r.maxNrOfInstances)));
    if (r.maxByteSize.has_value())
        obj->AddMember(attr::MAX_BYTE_SIZE, new JsonValue(static_cast<double>(*r.maxByteSize)));
    if (r.maxInstanceAge.has_value())
        obj->AddMember(attr::MAX_INSTANCE_AGE,
                       new JsonValue(static_cast<double>(*r.maxInstanceAge)));
    obj->AddMember(attr::CURRENT_NR_OF_INSTANCES,
                   new JsonValue(static_cast<double>(r.currentNrOfInstances)));
    obj->AddMember(attr::CURRENT_BYTE_SIZE, new JsonValue(static_cast<double>(r.currentByteSize)));
    if (r.disableRetrieval.has_value())
        obj->AddMember(attr::DISABLE_RETRIEVAL,
                       new JsonValue(static_cast<boolean>(*r.disableRetrieval)));

    return obj;
}

JsonValue *JsonCodec::SerializeContentInstance(const ContentInstance &r) const
{
    JsonValue *obj = SerializeResourceBase(r);

    if (r.stateTag.has_value())
        obj->AddMember(attr::STATE_TAG, new JsonValue(static_cast<double>(*r.stateTag)));
    if (r.creator.has_value()) obj->AddMember(attr::CREATOR, new JsonValue(*r.creator));
    if (r.contentInfo.has_value())
        obj->AddMember(attr::CONTENT_INFO, new JsonValue(*r.contentInfo));
    obj->AddMember(attr::CONTENT_SIZE, new JsonValue(static_cast<double>(r.contentSize)));
    if (r.content.GetLength() > 0) {
        // Avoid treating unquoted alpha strings as JSON primitives (which
        // would be parsed to number 0). Only attempt to parse the content
        // as JSON when the first non-space character indicates a JSON value
        // (object, array, quoted string, digit, minus or literal starts).
        const char *p = r.content.c_str();
        while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
            ++p;
        bool tryParse = false;
        if (*p) {
            if (*p == '{' || *p == '[' || *p == '"' || *p == '-' || (*p >= '0' && *p <= '9'))
                tryParse = true;
            else if (strncmp(p, "true", 4) == 0 || strncmp(p, "false", 5) == 0 ||
                     strncmp(p, "null", 4) == 0)
                tryParse = true;
        }
        if (tryParse) {
            if (JsonValue *parsed = JsonDocument::Parse(r.content.c_str())) {
                obj->AddMember(attr::CONTENT, parsed);
            } else {
                obj->AddMember(attr::CONTENT, new JsonValue(r.content));
            }
        } else {
            obj->AddMember(attr::CONTENT, new JsonValue(r.content));
        }
    } else {
        obj->AddMember(attr::CONTENT, new JsonValue());
    }
    if (r.dataGenerationTime.has_value())
        obj->AddMember(attr::DATA_GENERATION_TIME, new JsonValue(*r.dataGenerationTime));

    return obj;
}

JsonValue *JsonCodec::SerializeNotification(const Notification &n) const
{
    JsonValue *obj = new JsonValue(JSON_OBJECT);

    obj->AddMember("sur", new JsonValue(n.subscriptionReference));

    if (n.verificationRequest.has_value())
        obj->AddMember("vrq", new JsonValue((boolean)*n.verificationRequest));

    if (n.subscriptionDeletion.has_value())
        obj->AddMember("sud", new JsonValue((boolean)*n.subscriptionDeletion));

    if (n.creator.has_value()) obj->AddMember("cr", new JsonValue(*n.creator));

    if (n.notificationForwardingURI.has_value())
        obj->AddMember("nfu", new JsonValue(*n.notificationForwardingURI));

    if (n.notificationTarget.has_value())
        obj->AddMember("ntt", new JsonValue(*n.notificationTarget));

    if (n.targetRemovalRequest.has_value())
        obj->AddMember("trr", new JsonValue((boolean)*n.targetRemovalRequest));

    if (n.targetRemovalAllowance.has_value())
        obj->AddMember("tra", new JsonValue((boolean)*n.targetRemovalAllowance));

    if (n.aeRegistrationPointChange.has_value())
        obj->AddMember("aerp", new JsonValue((boolean)*n.aeRegistrationPointChange));

    if (n.aeReferenceIDChange.has_value())
        obj->AddMember("aerid", new JsonValue((boolean)*n.aeReferenceIDChange));

    if (n.trackingID1.has_value()) obj->AddMember("tid1", new JsonValue(*n.trackingID1));
    if (n.trackingID2.has_value()) obj->AddMember("tid2", new JsonValue(*n.trackingID2));
    if (n.subscribedTo.has_value()) obj->AddMember("st", new JsonValue(*n.subscribedTo));

    // nev
    if (n.notificationEvent.has_value()) {
        JsonValue *nevObj = new JsonValue(JSON_OBJECT);

        nevObj->AddMember(
            "net",
            new JsonValue(static_cast<double>((u8)n.notificationEvent->notificationEventType)));

        // om
        if (n.notificationEvent->operationMonitor.has_value()) {
            JsonValue *omObj = new JsonValue(JSON_OBJECT);
            omObj->AddMember("op",
                             new JsonValue(static_cast<double>(
                                 (u8)n.notificationEvent->operationMonitor->operation)));
            omObj->AddMember("org",
                             new JsonValue(n.notificationEvent->operationMonitor->originator));
            nevObj->AddMember("om", omObj);
        }

        if (n.notificationEvent->representation) {
            JsonValue *repJson =
                SerializePrimitiveContentValue(*n.notificationEvent->representation);

            if (repJson) nevObj->AddMember("rep", repJson);
        }

        obj->AddMember("nev", nevObj);
    }

    //
    // idr
    //
    if (n.ipeDiscoveryRequest.has_value()) {
        JsonValue *idrObj = new JsonValue(JSON_OBJECT);

        idrObj->AddMember("org", new JsonValue(n.ipeDiscoveryRequest->originator));

        JsonValue *fcObj = new JsonValue(JSON_OBJECT);

        const auto &fc = n.ipeDiscoveryRequest->filterCriteria;

        if (!fc.notificationEventType.empty()) {
            JsonValue *arr = new JsonValue(JSON_ARRAY);

            for (auto v : fc.notificationEventType)
                arr->AppendElement(new JsonValue((double)(u8)v));

            fcObj->AddMember("net", arr);
        }

        if (!fc.childResourceType.empty()) {
            JsonValue *arr = new JsonValue(JSON_ARRAY);

            for (auto v : fc.childResourceType)
                arr->AppendElement(new JsonValue((double)(u8)v));

            fcObj->AddMember("chty", arr);
        }

        if (!fc.attributeList.empty()) fcObj->AddMember("atr", MakeStringArray(fc.attributeList));

        if (!fc.labels.empty()) fcObj->AddMember("lbl", MakeStringArray(fc.labels));

        idrObj->AddMember("fc", fcObj);

        obj->AddMember("idr", idrObj);
    }

    return obj;
}

JsonValue *JsonCodec::SerializeGroup(const Group &r) const
{
    JsonValue *obj = SerializeResourceBase(r);

    obj->AddMember(attr::MEMBER_TYPE, new JsonValue(static_cast<double>((u32)r.memberType)));
    obj->AddMember(attr::MEMBER_TYPE_VALIDATED,
                   new JsonValue(static_cast<boolean>(r.memberTypeValidated)));
    obj->AddMember(attr::CURRENT_NR_OF_MEMBERS,
                   new JsonValue(static_cast<double>(r.currentNrOfMembers)));
    obj->AddMember(attr::MAX_NR_OF_MEMBERS, new JsonValue(static_cast<double>(r.maxNrOfMembers)));
    obj->AddMember(attr::MEMBER_IDS, MakeStringArray(r.memberIDs));
    obj->AddMember(attr::CONSISTENCY_STRATEGY,
                   new JsonValue(static_cast<double>((u8)r.consistencyStrategy)));
    if (r.groupName.has_value()) obj->AddMember(attr::GROUP_NAME, new JsonValue(*r.groupName));

    return obj;
}

JsonValue *JsonCodec::SerializeSubscription(const Subscription &r) const
{
    JsonValue *obj = SerializeResourceBase(r);

    if (!r.notificationURI.empty())
        obj->AddMember(attr::NOTIFICATION_URI, MakeStringArray(r.notificationURI));
    if (r.batchNotify.has_value()) {
        JsonValue *bnObj = new JsonValue(JSON_OBJECT);
        if (r.batchNotify->number > 0)
            bnObj->AddMember(dt::NUMBER, new JsonValue(static_cast<double>(r.batchNotify->number)));
        if (r.batchNotify->duration.has_value()) {
            bnObj->AddMember(dt::DURATION, new JsonValue(*r.batchNotify->duration));
        } else if (r.batchNotify->number > 0) {
            bnObj->AddMember(dt::DURATION, new JsonValue(1.0));
        }
        obj->AddMember(attr::BATCH_NOTIFY, bnObj);
    }
    if (r.notificationContentType.has_value())
        obj->AddMember(attr::NOTIFICATION_CONTENT_TYPE,
                       new JsonValue(static_cast<double>((u8)*r.notificationContentType)));
    if (r.expirationCounter.has_value())
        obj->AddMember(attr::EXPIRATION_COUNTER,
                       new JsonValue(static_cast<double>(*r.expirationCounter)));
    if (r.latestNotify.has_value())
        obj->AddMember(attr::LATEST_NOTIFY, new JsonValue(static_cast<boolean>(*r.latestNotify)));
    if (r.subscriberURI.has_value())
        obj->AddMember(attr::SUBSCRIBER_URI, new JsonValue(*r.subscriberURI));
    if (r.creator.has_value() && r.creatorProvided.has_value() && *r.creatorProvided)
        obj->AddMember(attr::CREATOR, new JsonValue(*r.creator));

    // EventNotificationCriteria
    {
        const EventNotificationCriteria &enc    = r.eventNotificationCriteria;
        JsonValue                       *encObj = new JsonValue(JSON_OBJECT);

        if (!enc.notificationEventType.empty()) {
            JsonValue *netArr = new JsonValue(JSON_ARRAY);
            for (size_t i = 0; i < enc.notificationEventType.size(); ++i)
                netArr->AppendElement(
                    new JsonValue(static_cast<double>((u8)enc.notificationEventType[i])));
            encObj->AddMember(dt::NOTIFICATION_EVENT_TYPE, netArr);
        }
        if (!enc.childResourceType.empty()) {
            JsonValue *chtyArr = new JsonValue(JSON_ARRAY);
            for (size_t i = 0; i < enc.childResourceType.size(); ++i)
                chtyArr->AppendElement(
                    new JsonValue(static_cast<double>((u8)enc.childResourceType[i])));
            encObj->AddMember(dt::fc::CHILD_RESOURCE_TYPE, chtyArr);
        }
        if (!enc.attributeList.empty())
            encObj->AddMember(dt::fc::ATTRIBUTE, MakeStringArray(enc.attributeList));
        if (!enc.labels.empty()) encObj->AddMember(dt::fc::LABELS, MakeStringArray(enc.labels));

        obj->AddMember(attr::EVENT_NOTIFICATION_CRITERIA, encObj);
    }

    return obj;
}

JsonValue *JsonCodec::SerializeCSEBase(const CSEBase &r) const
{
    JsonValue *obj = SerializeResourceBase(r);

    if (!obj->GetMember(attr::PARENT_ID)) {
        obj->AddMember(attr::PARENT_ID, new JsonValue(r.parentID));
    }

    obj->AddMember(attr::CSE_TYPE, new JsonValue(static_cast<double>((u8)r.cseType)));
    obj->AddMember(attr::CSE_ID, new JsonValue(r.cseID));
    if (!r.supportedResourceType.empty()) {
        JsonValue *srtArr = new JsonValue(JSON_ARRAY);
        for (size_t i = 0; i < r.supportedResourceType.size(); ++i)
            srtArr->AppendElement(
                new JsonValue(static_cast<double>((u32)r.supportedResourceType[i])));
        obj->AddMember(attr::SUPPORTED_RESOURCE_TYPE, srtArr);
    }
    if (!r.pointOfAccess.empty())
        obj->AddMember(attr::POINT_OF_ACCESS, MakeStringArray(r.pointOfAccess));
    if (!r.supportedReleaseVersions.empty())
        obj->AddMember(attr::SUPPORTED_RELEASE_VERSIONS,
                       MakeStringArray(r.supportedReleaseVersions));
    if (r.currentTime.has_value())
        obj->AddMember(attr::CURRENT_TIME, new JsonValue(*r.currentTime));

    return obj;
}

JsonValue *JsonCodec::SerializeRemoteCSE(const RemoteCSE &r) const
{
    JsonValue *obj = SerializeResourceBase(r);

    obj->AddMember(attr::CSE_ID, new JsonValue(r.cseID));
    if (r.cseType.has_value())
        obj->AddMember(attr::CSE_TYPE, new JsonValue(static_cast<double>((u8)*r.cseType)));
    if (r.requestReachability.has_value())
        obj->AddMember(attr::REQUEST_REACHABILITY,
                       new JsonValue(static_cast<boolean>(*r.requestReachability)));
    if (!r.pointOfAccess.empty())
        obj->AddMember(attr::POINT_OF_ACCESS, MakeStringArray(r.pointOfAccess));
    if (!r.supportedReleaseVersions.empty())
        obj->AddMember(attr::SUPPORTED_RELEASE_VERSIONS,
                       MakeStringArray(r.supportedReleaseVersions));

    return obj;
}

JsonValue *JsonCodec::SerializeNode(const Node &r) const
{
    JsonValue *obj = SerializeResourceBase(r);

    obj->AddMember(attr::NODE_LINK, new JsonValue(r.nodeID));
    if (!r.hostedAELinks.empty()) obj->AddMember("hael", MakeStringArray(r.hostedAELinks));
    if (r.hostedCSELink.has_value()) obj->AddMember("hcl", new JsonValue(*r.hostedCSELink));

    return obj;
}

JsonValue *JsonCodec::SerializePollingChannel(const PollingChannel &r) const
{
    JsonValue *obj = SerializeResourceBase(r);

    if (r.requestAggregation.has_value())
        obj->AddMember("rqag", new JsonValue(static_cast<boolean>(*r.requestAggregation)));

    return obj;
}

JsonValue *JsonCodec::SerializeSchedule(const Schedule &r) const
{
    JsonValue *obj = SerializeResourceBase(r);

    JsonValue *seObj = new JsonValue(JSON_OBJECT);
    seObj->AddMember("sce", MakeStringArray(r.scheduleElement.scheduleEntry));
    obj->AddMember("se", seObj);

    return obj;
}

JsonValue *JsonCodec::SerializeMgmtCmd(const MgmtCmd &r) const
{
    JsonValue *obj = SerializeResourceBase(r);

    obj->AddMember(attr::CMD_TYPE, new JsonValue(r.cmdType));
    obj->AddMember(attr::EXEC_TARGET, new JsonValue(r.execTarget));
    obj->AddMember(attr::EXEC_ENABLE, new JsonValue(static_cast<boolean>(r.execEnable)));
    if (r.description.has_value()) obj->AddMember(attr::DESCRIPTION, new JsonValue(*r.description));
    if (r.execMode.has_value())
        obj->AddMember(attr::EXEC_MODE, new JsonValue(static_cast<double>((u8)*r.execMode)));
    if (r.execFrequency.has_value())
        obj->AddMember(attr::EXEC_FREQUENCY, new JsonValue(static_cast<double>(*r.execFrequency)));
    if (r.execDelay.has_value())
        obj->AddMember(attr::EXEC_DELAY, new JsonValue(static_cast<double>(*r.execDelay)));
    if (r.execNumber.has_value())
        obj->AddMember(attr::EXEC_NUMBER, new JsonValue(static_cast<double>(*r.execNumber)));

    return obj;
}

JsonValue *JsonCodec::SerializeExecInstance(const ExecInstance &r) const
{
    JsonValue *obj = SerializeResourceBase(r);

    obj->AddMember(attr::EXEC_STATUS, new JsonValue(static_cast<double>((u8)r.execStatus)));
    obj->AddMember(attr::EXEC_DISABLE, new JsonValue(static_cast<boolean>(r.execDisable)));
    obj->AddMember(attr::EXEC_TARGET, new JsonValue(r.execTarget));
    if (r.execResult.has_value()) obj->AddMember(attr::EXEC_RESULT, new JsonValue(*r.execResult));
    if (r.execMode.has_value())
        obj->AddMember(attr::EXEC_MODE, new JsonValue(static_cast<double>((u8)*r.execMode)));

    return obj;
}

JsonValue *JsonCodec::SerializeTimeSeries(const TimeSeries &r) const
{
    JsonValue *obj = SerializeResourceBase(r);

    obj->AddMember(attr::CURRENT_NR_OF_INSTANCES,
                   new JsonValue(static_cast<double>(r.currentNrOfInstances)));
    obj->AddMember(attr::CURRENT_BYTE_SIZE, new JsonValue(static_cast<double>(r.currentByteSize)));
    obj->AddMember(attr::MISSING_DATA_DETECT,
                   new JsonValue(static_cast<boolean>(r.missingDataDetect)));
    if (r.maxNrOfInstances.has_value())
        obj->AddMember(attr::MAX_NR_OF_INSTANCES,
                       new JsonValue(static_cast<double>(*r.maxNrOfInstances)));
    if (r.maxByteSize.has_value())
        obj->AddMember(attr::MAX_BYTE_SIZE, new JsonValue(static_cast<double>(*r.maxByteSize)));
    if (r.periodicInterval.has_value())
        obj->AddMember(attr::PERIODIC_INTERVAL,
                       new JsonValue(static_cast<double>(*r.periodicInterval)));
    if (r.periodicIntervalDelta.has_value())
        obj->AddMember(attr::PERIODIC_INTERVAL_DELTA,
                       new JsonValue(static_cast<double>(*r.periodicIntervalDelta)));

    return obj;
}

JsonValue *JsonCodec::SerializeTimeSeriesInstance(const TimeSeriesInstance &r) const
{
    JsonValue *obj = SerializeResourceBase(r);

    obj->AddMember(attr::CONTENT_SIZE, new JsonValue(static_cast<double>(r.contentSize)));
    obj->AddMember(attr::CONTENT, new JsonValue(r.content));
    if (r.contentInfo.has_value())
        obj->AddMember(attr::CONTENT_INFO, new JsonValue(*r.contentInfo));
    if (r.dataGenerationTime.has_value())
        obj->AddMember(attr::DATA_GENERATION_TIME, new JsonValue(*r.dataGenerationTime));
    if (r.sequenceNr.has_value())
        obj->AddMember(attr::SEQUENCE_NR, new JsonValue(static_cast<double>(*r.sequenceNr)));

    return obj;
}

JsonValue *JsonCodec::SerializeRequestResource(const RequestResource &r) const
{
    JsonValue *obj = SerializeResourceBase(r);

    obj->AddMember(attr::ORIGINATOR, new JsonValue(r.originator));
    obj->AddMember(attr::OPERATION, new JsonValue(static_cast<double>((u8)r.operation)));
    obj->AddMember(attr::REQUEST_ID, new JsonValue(r.requestID));
    obj->AddMember(attr::REQUEST_STATUS, new JsonValue(static_cast<double>(r.requestStatus)));
    if (r.operationResult.has_value())
        obj->AddMember(attr::OPERATION_RESULT, new JsonValue(*r.operationResult));
    if (r.target.has_value()) obj->AddMember("tg", new JsonValue(*r.target));

    return obj;
}

JsonValue *JsonCodec::SerializeFlexContainer(const FlexContainer &r) const
{
    JsonValue *obj = SerializeResourceBase(r);

    obj->AddMember(attr::CONTAINER_DEFINITION, new JsonValue(r.containerDefinition));
    obj->AddMember(attr::CURRENT_NR_OF_INSTANCES,
                   new JsonValue(static_cast<double>(r.currentNrOfInstances)));
    obj->AddMember(attr::CURRENT_BYTE_SIZE, new JsonValue(static_cast<double>(r.currentByteSize)));
    if (r.ontologyRef.has_value())
        obj->AddMember(attr::ONTOLOGY_REF, new JsonValue(*r.ontologyRef));

    // Custom attributes are appended directly into the same object
    for (size_t i = 0; i < r.customAttributes.size(); ++i)
        obj->AddMember((const char *)r.customAttributes[i].key,
                       new JsonValue(r.customAttributes[i].value));

    return obj;
}

JsonValue *JsonCodec::SerializeAccessControlPolicy(const AccessControlPolicy &r) const
{
    JsonValue *obj = SerializeResourceBase(r);

    auto serializeRules = [&](const Vector<AccessControlRule> &rules) -> JsonValue * {
        JsonValue *arr = new JsonValue(JSON_ARRAY);
        for (size_t i = 0; i < rules.size(); ++i) {
            const AccessControlRule &rule    = rules[i];
            JsonValue               *ruleObj = new JsonValue(JSON_OBJECT);
            ruleObj->AddMember(dt::ACCESS_CONTROL_ORIGINATORS,
                               MakeStringArray(rule.accessControlOriginators));
            ruleObj->AddMember(dt::ACCESS_CONTROL_OPERATIONS,
                               new JsonValue(static_cast<double>(rule.accessControlOperations)));
            arr->AppendElement(ruleObj);
        }
        return arr;
    };

    JsonValue *pvObj = new JsonValue(JSON_OBJECT);
    pvObj->AddMember(dt::ACCESS_CONTROL_RULE, serializeRules(r.privileges));
    obj->AddMember(attr::PRIVILEGES, pvObj);

    JsonValue *pvsObj = new JsonValue(JSON_OBJECT);
    pvsObj->AddMember(dt::ACCESS_CONTROL_RULE, serializeRules(r.selfPrivileges));
    obj->AddMember(attr::SELF_PRIVILEGES, pvsObj);

    return obj;
}

boolean JsonCodec::SerializeResource(const ResourceBase &input, CString &output) const
{
    if (!input.resourceType.has_value()) return false;

    const char *shortName = shortNameByResourceType((ResourceType)*input.resourceType);
    if (!shortName) return false;

    // Dispatch to the typed serialiser via resourceType
    JsonValue *inner = nullptr;
    switch ((ResourceType)*input.resourceType) {
        case ResourceType::AE:
            inner = SerializeAE(static_cast<const AE &>(input));
            break;
        case ResourceType::Container:
            inner = SerializeContainer(static_cast<const Container &>(input));
            break;
        case ResourceType::ContentInstance:
            inner = SerializeContentInstance(static_cast<const ContentInstance &>(input));
            break;
        case ResourceType::Group:
            inner = SerializeGroup(static_cast<const Group &>(input));
            break;
        case ResourceType::Subscription:
            inner = SerializeSubscription(static_cast<const Subscription &>(input));
            break;
        case ResourceType::CSEBase:
            inner = SerializeCSEBase(static_cast<const CSEBase &>(input));
            break;
        case ResourceType::RemoteCSE:
            inner = SerializeRemoteCSE(static_cast<const RemoteCSE &>(input));
            break;
        case ResourceType::Node:
            inner = SerializeNode(static_cast<const Node &>(input));
            break;
        case ResourceType::PollingChannel:
            inner = SerializePollingChannel(static_cast<const PollingChannel &>(input));
            break;
        case ResourceType::Schedule:
            inner = SerializeSchedule(static_cast<const Schedule &>(input));
            break;
        case ResourceType::MgmtCmd:
            inner = SerializeMgmtCmd(static_cast<const MgmtCmd &>(input));
            break;
        case ResourceType::ExecInstance:
            inner = SerializeExecInstance(static_cast<const ExecInstance &>(input));
            break;
        case ResourceType::TimeSeries:
            inner = SerializeTimeSeries(static_cast<const TimeSeries &>(input));
            break;
        case ResourceType::TimeSeriesInstance:
            inner = SerializeTimeSeriesInstance(static_cast<const TimeSeriesInstance &>(input));
            break;
        case ResourceType::Request:
            inner = SerializeRequestResource(static_cast<const RequestResource &>(input));
            break;
        case ResourceType::FlexContainer:
            inner = SerializeFlexContainer(static_cast<const FlexContainer &>(input));
            break;
        case ResourceType::AccessControlPolicy:
            inner = SerializeAccessControlPolicy(static_cast<const AccessControlPolicy &>(input));
            break;
        default:
            return false;
    }

    if (!inner) return false;

    // Wrap: { "m2m:<shortName>": { ... } }
    CString wrapKey;
    wrapKey.Append("m2m:");
    wrapKey.Append(shortName);

    JsonValue root(JSON_OBJECT);
    root.AddMember((const char *)wrapKey, inner); // root takes ownership
    output = root.Serialize();
    return true;
}

boolean JsonCodec::SerializePrimitiveContent(const PrimitiveContent &input, CString &output) const
{
    JsonValue *v = SerializePrimitiveContentValue(input);

    if (!v) return false;
    output = v->Serialize();

    delete v;
    return true;
}

JsonValue *JsonCodec::SerializePrimitiveContentValue(const PrimitiveContent &input) const
{
    if (input.empty()) return nullptr;

    JsonValue  *inner     = nullptr;
    const char *shortName = nullptr;

#define TRY_RESOURCE(T, SN, METHOD)                                                                \
    if (const auto *p = input.GetIf<T>()) {                                                        \
        inner     = METHOD(*p);                                                                    \
        shortName = SN;                                                                            \
    } else

    TRY_RESOURCE(AE, "ae", SerializeAE)
    TRY_RESOURCE(Container, "cnt", SerializeContainer)
    TRY_RESOURCE(ContentInstance, "cin", SerializeContentInstance)
    TRY_RESOURCE(Notification, "sgn", SerializeNotification)
    TRY_RESOURCE(Group, "grp", SerializeGroup)
    TRY_RESOURCE(Subscription, "sub", SerializeSubscription)
    TRY_RESOURCE(CSEBase, "cb", SerializeCSEBase)
    TRY_RESOURCE(RemoteCSE, "csr", SerializeRemoteCSE)
    TRY_RESOURCE(Node, "nod", SerializeNode)
    TRY_RESOURCE(PollingChannel, "pch", SerializePollingChannel)
    TRY_RESOURCE(Schedule, "sch", SerializeSchedule)
    TRY_RESOURCE(MgmtCmd, "mgc", SerializeMgmtCmd)
    TRY_RESOURCE(ExecInstance, "exin", SerializeExecInstance)
    TRY_RESOURCE(TimeSeries, "ts", SerializeTimeSeries)
    TRY_RESOURCE(TimeSeriesInstance, "tsi", SerializeTimeSeriesInstance)
    TRY_RESOURCE(RequestResource, "req", SerializeRequestResource)
    TRY_RESOURCE(FlexContainer, "fcnt", SerializeFlexContainer)
    TRY_RESOURCE(AccessControlPolicy, "acp", SerializeAccessControlPolicy)
    // Close the macro chain for resource wrappers
    {
    }
#undef TRY_RESOURCE
    // if a resource type matched, build the wrapper and return
    if (inner && shortName) {
        CString wrapKey;
        wrapKey.Append("m2m:");
        wrapKey.Append(shortName);
        JsonValue *wrapper = new JsonValue(JSON_OBJECT);
        wrapper->AddMember((const char *)wrapKey, inner);
        return wrapper;
    }
    return nullptr;
}

boolean JsonCodec::SerializeResponsePrimitive(const ResponsePrimitive &input, CString &output) const
{
    JsonValue *rsp = new JsonValue(JSON_OBJECT);

    rsp->AddMember(prim::RESPONSE_STATUS_CODE,
                   new JsonValue(static_cast<double>((u32)input.responseStatusCode)));
    rsp->AddMember(prim::TO, new JsonValue(input.to));
    rsp->AddMember(prim::FROM, new JsonValue(input.from));
    rsp->AddMember(prim::REQUEST_IDENTIFIER, new JsonValue(input.requestIdentifier));

    if (!input.content.empty()) {
        CString pcStr;
        if (SerializePrimitiveContent(input.content, pcStr)) {
            JsonValue *pc = SerializePrimitiveContentValue(input.content);
            if (pc) rsp->AddMember(prim::CONTENT, pc);
        }
    }

    if (input.originatingTimestamp.has_value())
        rsp->AddMember(prim::ORIGINATING_TIMESTAMP, new JsonValue(*input.originatingTimestamp));
    if (input.releaseVersionIndicator.has_value())
        rsp->AddMember(prim::RELEASE_VERSION_INDICATOR,
                       new JsonValue(*input.releaseVersionIndicator));
    if (input.vendorInformation.has_value())
        rsp->AddMember(prim::VENDOR_INFORMATION, new JsonValue(*input.vendorInformation));

    JsonValue root(JSON_OBJECT);
    root.AddMember(root::RESPONSE_PRIMITIVE, rsp);
    output = root.Serialize();
    return true;
}

// Deserialisers
boolean JsonCodec::DeserializeAE(const JsonValue &root, RequestPrimitive &out) const
{
    const JsonValue *ae = root.GetMember("m2m:ae");
    if (!ae) return false;

    AE r;
    r.resourceName = GetString(*ae, attr::RESOURCE_NAME);
    r.appID        = GetString(*ae, attr::APP_ID);
    r.aeID         = GetString(*ae, attr::AE_ID);
    GetOptString(*ae, attr::APP_NAME, r.appName);
    GetOptBool(*ae, attr::REQUEST_REACHABILITY, r.requestReachability);
    GetStringArray(*ae, attr::POINT_OF_ACCESS, r.pointOfAccess);
    GetStringArray(*ae, attr::CONTENT_SERIALIZATION, r.contentSerialization);
    GetStringArray(*ae, attr::SUPPORTED_RELEASE_VERSIONS, r.supportedReleaseVersions);
    GetOptString(*ae, attr::NODE_LINK, r.nodeLink);
    GetOptString(*ae, attr::EXPIRATION_TIME, r.expirationTime);
    GetStringArray(*ae, attr::LABELS, r.labels);

    // Detect out-of-spec attributes that must be rejected during Create (e.g. 'cr').
    // Propagate a marker in the primitive so service logic can return BadRequest.
    if (ae->GetMember(attr::CREATOR)) { out.vendorInformation = CString("has_creator"); }

    out.content = r;
    return true;
}

boolean JsonCodec::DeserializeContainer(const JsonValue &root, RequestPrimitive &out) const
{
    const JsonValue *cnt = root.GetMember("m2m:cnt");
    if (!cnt) return false;

    Container r;
    r.resourceName = GetString(*cnt, attr::RESOURCE_NAME);
    if (const JsonValue *cr = cnt->GetMember(attr::CREATOR)) {
        if (cr->GetType() == JSON_NULL) {
            out.vendorInformation = CString("cnt_creator_null");
        } else {
            out.vendorInformation = CString("cnt_creator_present");
        }
    }
    GetOptS64(*cnt, attr::MAX_NR_OF_INSTANCES, r.maxNrOfInstances);
    GetOptS64(*cnt, attr::MAX_BYTE_SIZE, r.maxByteSize);
    GetOptS64(*cnt, attr::MAX_INSTANCE_AGE, r.maxInstanceAge);
    GetOptString(*cnt, attr::EXPIRATION_TIME, r.expirationTime);
    GetStringArray(*cnt, attr::LABELS, r.labels);
    GetOptBool(*cnt, attr::DISABLE_RETRIEVAL, r.disableRetrieval);

    out.content = r;
    return true;
}

boolean JsonCodec::DeserializeContentInstance(const JsonValue &root, RequestPrimitive &out) const
{
    const JsonValue *cin = root.GetMember("m2m:cin");
    if (!cin) return false;

    ContentInstance r;
    r.resourceName = GetString(*cin, attr::RESOURCE_NAME);
    if (const JsonValue *cr = cin->GetMember(attr::CREATOR)) {
        if (cr->GetType() == JSON_NULL) {
            out.vendorInformation = CString("cin_creator_null");
        } else {
            out.vendorInformation = CString("cin_creator_present");
        }
    }
    if (cin->GetMember(attr::ACCESS_CONTROL_POLICY_IDS)) {
        out.vendorInformation = CString("cin_has_acpi");
    }
    GetOptString(*cin, attr::CONTENT_INFO, r.contentInfo);
    // Preserve the content for `con`. If the content is a JSON string, store the
    // raw string value (no surrounding quotes) so contentSize matches the
    // actual payload size expected by tests/spec. For non-string values keep the
    // serialized JSON so objects/arrays/numbers round-trip correctly.
    const JsonValue *conV = cin->GetMember(attr::CONTENT);
    if (conV) {
        if (conV->GetType() == JSON_STRING) {
            const CString *s = conV->GetString();
            if (s) r.content = *s;
            else r.content = CString();
        } else {
            r.content = conV->Serialize();
        }
    }
    const JsonValue *csV = cin->GetMember(attr::CONTENT_SIZE);
    if (csV) {
        auto n = csV->GetNumber();
        if (n.has_value()) r.contentSize = static_cast<s64>(*n);
    }
    // If content size not provided, estimate from serialized content
    if (!csV) {
        if (r.content.GetLength() > 0) r.contentSize = static_cast<s64>(r.content.GetLength());
        else r.contentSize = 0;
    }
    GetOptString(*cin, attr::DATA_GENERATION_TIME, r.dataGenerationTime);
    GetStringArray(*cin, attr::LABELS, r.labels);

    out.content = r;
    return true;
}

boolean JsonCodec::DeserializeNotification(const JsonValue &root, RequestPrimitive &out) const
{
    const JsonValue *sgn = root.GetMember("m2m:sgn");
    if (!sgn) return false;

    Notification n;

    n.subscriptionReference = GetString(*sgn, "sur");

    GetOptBool(*sgn, "vrq", n.verificationRequest);
    GetOptBool(*sgn, "sud", n.subscriptionDeletion);

    GetOptString(*sgn, "cr", n.creator);
    GetOptString(*sgn, "nfu", n.notificationForwardingURI);
    GetOptString(*sgn, "ntt", n.notificationTarget);

    GetOptBool(*sgn, "trr", n.targetRemovalRequest);
    GetOptBool(*sgn, "tra", n.targetRemovalAllowance);

    GetOptBool(*sgn, "aerp", n.aeRegistrationPointChange);
    GetOptBool(*sgn, "aerid", n.aeReferenceIDChange);

    GetOptString(*sgn, "tid1", n.trackingID1);
    GetOptString(*sgn, "tid2", n.trackingID2);

    GetOptString(*sgn, "st", n.subscribedTo);

    //
    // nev
    //
    if (const JsonValue *nevObj = sgn->GetMember("nev")) {
        NotificationEvent nev;

        const JsonValue *netV = nevObj->GetMember("net");
        if (netV) {
            auto v = netV->GetNumber();
            if (v.has_value()) {
                nev.notificationEventType = static_cast<NotificationEventType>(static_cast<u8>(*v));
            }
        }

        //
        // om
        //
        if (const JsonValue *omObj = nevObj->GetMember("om")) {
            OperationMonitor om;

            om.originator = GetString(*omObj, "org");

            const JsonValue *opV = omObj->GetMember("op");
            if (opV) {
                auto v = opV->GetNumber();
                if (v.has_value()) { om.operation = static_cast<Operation>(static_cast<u8>(*v)); }
            }

            nev.operationMonitor = om;
        }

        //
        // rep
        //
        if (const JsonValue *repObj = nevObj->GetMember("rep")) {

            boolean          ok = false;
            RequestPrimitive tmpReq;
            if (repObj->GetMember("m2m:ae")) ok = DeserializeAE(*repObj, tmpReq);
            else if (repObj->GetMember("m2m:cnt")) ok = DeserializeContainer(*repObj, tmpReq);
            else if (repObj->GetMember("m2m:cin")) ok = DeserializeContentInstance(*repObj, tmpReq);
            else if (repObj->GetMember("m2m:sgn")) ok = DeserializeNotification(*repObj, tmpReq);
            else if (repObj->GetMember("m2m:grp")) ok = DeserializeGroup(*repObj, tmpReq);
            else if (repObj->GetMember("m2m:sub")) ok = DeserializeSubscription(*repObj, tmpReq);
            else if (repObj->GetMember("m2m:ts")) ok = DeserializeTimeSeries(*repObj, tmpReq);
            else {
                CLogger::Get()->Write(
                    "JsonCodec", LogDebug, "DeserializeRequestBody: unrecognized JSON payload");
            }

            if (ok) {
                nev.representation = new PrimitiveContent(tmpReq.content);
            }
        }

        n.notificationEvent = nev;
    }

    //
    // idr
    //
    if (const JsonValue *idrObj = sgn->GetMember("idr")) {
        IPEDiscoveryRequest idr;

        idr.originator = GetString(*idrObj, "org");

        if (const JsonValue *fcObj = idrObj->GetMember("fc")) {
            ParseEventNotificationCriteria(*fcObj, idr.filterCriteria);
        }

        n.ipeDiscoveryRequest = idr;
    }

    out.content = n;
    return true;
}

boolean JsonCodec::DeserializeGroup(const JsonValue &root, RequestPrimitive &out) const
{
    const JsonValue *grp = root.GetMember("m2m:grp");
    if (!grp) return false;

    Group r;
    r.resourceName       = GetString(*grp, attr::RESOURCE_NAME);
    const JsonValue *csV = grp->GetMember(attr::MAX_NR_OF_MEMBERS);
    if (csV) {
        auto n = csV->GetNumber();
        if (n.has_value()) r.maxNrOfMembers = static_cast<s32>(*n);
    }
    GetStringArray(*grp, attr::MEMBER_IDS, r.memberIDs);
    GetStringArray(*grp, attr::LABELS, r.labels);

    // memberType
    const JsonValue *mt = grp->GetMember(attr::MEMBER_TYPE);
    if (mt) {
        auto n = mt->GetNumber();
        if (n.has_value()) r.memberType = static_cast<ResourceType>(static_cast<u32>(*n));
    }

    out.content = r;
    return true;
}

boolean JsonCodec::DeserializeSubscription(const JsonValue &root, RequestPrimitive &out) const
{
    const JsonValue *sub = root.GetMember("m2m:sub");
    if (!sub) return false;

    Subscription r;
    r.resourceName = GetString(*sub, attr::RESOURCE_NAME);
    GetStringArray(*sub, attr::NOTIFICATION_URI, r.notificationURI);
    const JsonValue *bnObj = sub->GetMember(attr::BATCH_NOTIFY);
    if (bnObj) {
        BatchNotify      bn;
        const JsonValue *numV = bnObj->GetMember(dt::NUMBER);
        if (numV) {
            auto n = numV->GetNumber();
            if (n.has_value()) bn.number = static_cast<s32>(*n);
        }
        GetOptString(*bnObj, dt::DURATION, bn.duration);
        r.batchNotify = bn;
    }
    GetOptString(*sub, attr::SUBSCRIBER_URI, r.subscriberURI);
    GetOptS32(*sub, attr::EXPIRATION_COUNTER, r.expirationCounter);
    GetOptBool(*sub, attr::LATEST_NOTIFY, r.latestNotify);
    GetStringArray(*sub, attr::LABELS, r.labels);

    // Detect explicit creator attribute for subscriptions so service logic
    // can decide whether to reject it (present) or accept null (set to
    // originator). Propagate a marker in the primitive via vendorInformation
    // and record that the client explicitly provided the creator (including
    // explicit null) on the Subscription content object.
    if (const JsonValue *cr = sub->GetMember(attr::CREATOR)) {
        r.creatorProvided = true;
        if (cr->GetType() == JSON_NULL) {
            out.vendorInformation = CString("sub_creator_null");
        } else {
            out.vendorInformation = CString("sub_creator_present");
            const CString *s      = cr->GetString();
            if (s) r.creator = *s;
        }
    }

    const JsonValue *encObj = sub->GetMember(attr::EVENT_NOTIFICATION_CRITERIA);
    if (encObj) ParseEventNotificationCriteria(*encObj, r.eventNotificationCriteria);

    // Parse explicit notificationContentType (nct) if present so the
    // service can decide compatibility for blocking-update subscriptions.
    if (const JsonValue *nctV = sub->GetMember(attr::NOTIFICATION_CONTENT_TYPE)) {
        auto n = nctV->GetNumber();
        if (n.has_value())
            r.notificationContentType = static_cast<NotificationContentType>(static_cast<u8>(*n));
    }

    out.content = r;
    return true;
}

boolean JsonCodec::DeserializeTimeSeries(const JsonValue &root, RequestPrimitive &out) const
{
    const JsonValue *ts = root.GetMember("m2m:ts");
    if (!ts) return false;

    TimeSeries r;
    r.resourceName = GetString(*ts, attr::RESOURCE_NAME);
    GetStringArray(*ts, attr::LABELS, r.labels);
    GetOptS64(*ts, attr::MAX_NR_OF_INSTANCES, r.maxNrOfInstances);
    GetOptS64(*ts, attr::MAX_BYTE_SIZE, r.maxByteSize);
    GetOptS64(*ts, attr::MAX_INSTANCE_AGE, r.maxInstanceAge);
    GetOptS32(*ts, attr::PERIODIC_INTERVAL, r.periodicInterval);
    GetOptS32(*ts, attr::PERIODIC_INTERVAL_DELTA, r.periodicIntervalDelta);
    GetOptString(*ts, attr::EXPIRATION_TIME, r.expirationTime);
    if (const JsonValue *mdd = ts->GetMember(attr::MISSING_DATA_DETECT)) {
        auto b = mdd->GetBoolean();
        if (b.has_value()) r.missingDataDetect = *b;
    }

    out.content = r;
    return true;
}

boolean JsonCodec::ParseEventNotificationCriteria(const JsonValue           &encObj,
                                                  EventNotificationCriteria &enc) const
{
    GetOptString(encObj, dt::fc::CREATED_BEFORE, enc.createdBefore);
    GetOptString(encObj, dt::fc::CREATED_AFTER, enc.createdAfter);
    GetOptString(encObj, dt::fc::MODIFIED_SINCE, enc.modifiedSince);
    GetOptString(encObj, dt::fc::UNMODIFIED_SINCE, enc.unmodifiedSince);
    GetOptS64(encObj, dt::fc::SIZE_ABOVE, enc.sizeAbove);
    GetOptS64(encObj, dt::fc::SIZE_BELOW, enc.sizeBelow);
    GetStringArray(encObj, dt::fc::ATTRIBUTE, enc.attributeList);
    GetStringArray(encObj, dt::fc::LABELS, enc.labels);

    const JsonValue *netArr = encObj.GetMember(dt::NOTIFICATION_EVENT_TYPE);
    if (netArr && netArr->GetType() == JSON_ARRAY) {
        for (size_t i = 0; i < netArr->GetArraySize(); ++i) {
            auto n = netArr->GetElement(i);
            if (n) {
                auto v = n->GetNumber();
                if (v.has_value())
                    enc.notificationEventType.push_back(
                        static_cast<NotificationEventType>(static_cast<u8>(*v)));
            }
        }
    }

    const JsonValue *chtyArr = encObj.GetMember(dt::fc::CHILD_RESOURCE_TYPE);
    if (chtyArr && chtyArr->GetType() == JSON_ARRAY) {
        for (size_t i = 0; i < chtyArr->GetArraySize(); ++i) {
            auto n = chtyArr->GetElement(i);
            if (n) {
                auto v = n->GetNumber();
                if (v.has_value())
                    enc.childResourceType.push_back(static_cast<ResourceType>(static_cast<u8>(*v)));
            }
        }
    }

    return true;
}

boolean JsonCodec::ParsePrimitiveFields(const JsonValue &obj, RequestPrimitive &out) const
{
    // Mandatory
    out.to                = GetString(obj, prim::TO);
    out.from              = GetString(obj, prim::FROM);
    out.requestIdentifier = GetString(obj, prim::REQUEST_IDENTIFIER);

    const JsonValue *opV = obj.GetMember(prim::OPERATION);
    if (opV) {
        auto n = opV->GetNumber();
        if (n.has_value()) out.op = static_cast<Operation>(static_cast<u8>(*n));
    }

    // Optional
    const JsonValue *tyV = obj.GetMember(prim::RESOURCE_TYPE);
    if (tyV) {
        auto n = tyV->GetNumber();
        if (n.has_value()) out.resourceType = static_cast<ResourceType>(static_cast<u32>(*n));
    }

    GetOptString(obj, prim::ORIGINATING_TIMESTAMP, out.originatingTimestamp);
    GetOptString(obj, prim::REQUEST_EXPIRATION_TIMESTAMP, out.requestExpirationTimestamp);
    GetOptString(obj, prim::RESULT_EXPIRATION_TIMESTAMP, out.resultExpirationTimestamp);
    GetOptString(obj, prim::RELEASE_VERSION_INDICATOR, out.releaseVersionIndicator);
    GetOptString(obj, prim::VENDOR_INFORMATION, out.vendorInformation);

    const JsonValue *rcnV = obj.GetMember(prim::RESULT_CONTENT);
    if (rcnV) {
        auto n = rcnV->GetNumber();
        if (n.has_value()) out.resultContent = static_cast<ResultContent>(static_cast<u8>(*n));
    }

    return out.to.GetLength() > 0 && out.requestIdentifier.GetLength() > 0;
}

boolean JsonCodec::DeserializeRequestBody(const CString &input, RequestPrimitive &out) const
{
    if (input.GetLength() == 0) return false;

    JsonValue *root = JsonDocument::Parse(input.c_str());
    if (!root) return false;

    boolean ok = false;
    if (root->GetMember("m2m:ae")) ok = DeserializeAE(*root, out);
    else if (root->GetMember("m2m:cnt")) ok = DeserializeContainer(*root, out);
    else if (root->GetMember("m2m:cin")) ok = DeserializeContentInstance(*root, out);
    else if (root->GetMember("m2m:sgn")) ok = DeserializeNotification(*root, out);
    else if (root->GetMember("m2m:grp")) ok = DeserializeGroup(*root, out);
    else if (root->GetMember("m2m:sub")) ok = DeserializeSubscription(*root, out);
    else if (root->GetMember("m2m:ts")) ok = DeserializeTimeSeries(*root, out);
    else {
        CLogger::Get()->Write(
            "JsonCodec", LogDebug, "DeserializeRequestBody: unrecognized JSON payload");
    }

    delete root;
    return ok;
}

boolean JsonCodec::DeserializeRequestPrimitive(const CString &input, RequestPrimitive &out) const
{
    if (input.GetLength() == 0) return false;

    JsonValue *doc = JsonDocument::Parse(input.c_str());
    if (!doc) return false;

    boolean ok = false;

    // Envelope may be bare or wrapped in "rqp"
    const JsonValue *rqp = doc->GetMember(root::REQUEST_PRIMITIVE);
    const JsonValue &obj = rqp ? *rqp : *doc;

    if (ParsePrimitiveFields(obj, out)) {
        // Parse the pc field if present
        const JsonValue *pc = obj.GetMember(prim::CONTENT);
        if (pc && pc->GetType() == JSON_OBJECT) {
            // Re-serialise pc and deserialise as a resource body
            CString pcStr = pc->Serialize();
            DeserializeRequestBody(pcStr, out); // fills out.content
        }
        ok = true;
    }

    delete doc;
    return ok;
}

boolean JsonCodec::DeserializeResponseBody(const CString &input, ResponsePrimitive &out) const
{
    if (input.GetLength() == 0) return false;

    JsonValue *root = JsonDocument::Parse(input.c_str());
    if (!root) return false;

    boolean ok = false;

    RequestPrimitive tmp;

    if (root->GetMember("m2m:ae")) ok = DeserializeAE(*root, tmp);
    else if (root->GetMember("m2m:cnt")) ok = DeserializeContainer(*root, tmp);
    else if (root->GetMember("m2m:cin")) ok = DeserializeContentInstance(*root, tmp);
    else if (root->GetMember("m2m:sgn")) ok = DeserializeNotification(*root, tmp);
    else if (root->GetMember("m2m:grp")) ok = DeserializeGroup(*root, tmp);
    else if (root->GetMember("m2m:sub")) ok = DeserializeSubscription(*root, tmp);
    else if (root->GetMember("m2m:ts")) ok = DeserializeTimeSeries(*root, tmp);

    if (ok) { out.content = tmp.content; }

    delete root;
    return ok;
}

boolean JsonCodec::DeserializeResponsePrimitive(const CString &input, ResponsePrimitive &out) const
{
    if (input.GetLength() == 0) return false;

    JsonValue *doc = JsonDocument::Parse(input.c_str());
    if (!doc) return false;

    boolean ok = false;

    const JsonValue *rsp = doc->GetMember(root::RESPONSE_PRIMITIVE);
    const JsonValue &obj = rsp ? *rsp : *doc;

    // rsc
    if (const JsonValue *rscV = obj.GetMember(prim::RESPONSE_STATUS_CODE)) {
        auto n = rscV->GetNumber();
        if (n.has_value()) {
            out.responseStatusCode = static_cast<ResponseStatusCode>(static_cast<u32>(*n));
        }
    }

    // mandatory/common fields
    out.to                = GetString(obj, prim::TO);
    out.from              = GetString(obj, prim::FROM);
    out.requestIdentifier = GetString(obj, prim::REQUEST_IDENTIFIER);

    // optional fields
    GetOptString(obj, prim::ORIGINATING_TIMESTAMP, out.originatingTimestamp);

    GetOptString(obj, prim::RELEASE_VERSION_INDICATOR, out.releaseVersionIndicator);

    GetOptString(obj, prim::VENDOR_INFORMATION, out.vendorInformation);

    // pc
    if (const JsonValue *pc = obj.GetMember(prim::CONTENT)) {
        CString pcStr = pc->Serialize();
        DeserializeResponseBody(pcStr, out);
    }

    ok = true;

    delete doc;
    return ok;
}

} // namespace zerom2m::serde::json