/*
 * resources.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <zerom2m/compat/types.h>
#include <zerom2m/onem2m/types/enums.h>
#include <zerom2m/onem2m/types/primitives.h>

namespace zerom2m::onem2m::types
{

using zerom2m::compat::Optional;
using zerom2m::compat::Vector;
struct PrimitiveContent;

struct CustomAttribute {
    CString key;
    CString value;
};

// Common base for all resources (attributes present on every resource type)
struct ResourceBase {
    CString                resourceName;           // rn
    CString                resourceID;             // ri
    Optional<ResourceType> resourceType;           // ty  (set by concrete type)
    CString                parentID;               // pi
    CString                creationTime;           // ct  (xs:dateTime UTC, e.g. "20240101T120000")
    CString                lastModifiedTime;       // lt
    Optional<CString>      expirationTime;         // et
    Vector<CString>        labels;                 // lbl
    Vector<CString>        accessControlPolicyIDs; // acpi
    Optional<CString>      custodian;              // cstn
    Vector<CString>        dynamicAuthConsultIDs;  // daci
    Optional<CString>      announceTo;             // at
    Vector<CString>        announcedAttribute;     // aa
    Optional<u8>           announceSyncType;       // ast

    virtual ~ResourceBase() = default;
};

// CSEBase (cb), ty=5
struct CSEBase : ResourceBase {
    CSEType              cseType;                  // cst
    CString              cseID;                    // csi
    Vector<ResourceType> supportedResourceType;    // srt
    Optional<CString>    notifCongestionPolicy;    // ncp
    Optional<CString>    currentTime;              // ctm
    Vector<CString>      pointOfAccess;            // poa
    Optional<CString>    nodeLink;                 // nl
    Vector<CString>      supportedReleaseVersions; // srv
    Optional<CString>    e2eSecInfo;               // esi
};

// AE (ae), ty=2
struct AE : ResourceBase {
    CString           appID;                    // api
    CString           aeID;                     // aei
    Optional<CString> appName;                  // apn
    Vector<CString>   pointOfAccess;            // poa
    Optional<CString> ontologyRef;              // or
    Optional<CString> nodeLink;                 // nl
    Vector<CString>   contentSerialization;     // csz
    Optional<u8>      registrationStatus;       // regs
    Optional<boolean> requestReachability;      // rr
    Optional<CString> m2mExtID;                 // mei
    Optional<CString> triggerRecipientID;       // tri
    Optional<u32>     triggerReferenceNumber;   // trn
    Vector<CString>   supportedReleaseVersions; // srv
    Optional<CString> e2eSecInfo;               // esi
    Optional<boolean> triggerEnable;            // tren

    // internal
    CString originator;
};

// Container (cnt), ty=3
struct Container : ResourceBase {
    Optional<u32>     stateTag;                 // st
    Optional<CString> creator;                  // cr
    Optional<s64>     maxNrOfInstances;         // mni  (null = unlimited)
    Optional<s64>     maxByteSize;              // mbs
    Optional<s64>     maxByteSizePerInstance;   // mbis
    Optional<s64>     maxInstanceAge;           // mia
    s64               currentNrOfInstances = 0; // cni
    s64               currentByteSize      = 0; // cbs
    Optional<CString> ontologyRef;              // or
    Optional<boolean> disableRetrieval;         // disr
    Optional<CString> locationID;               // li
    Optional<CString> location;                 // loc
};

// ContentInstance (cin), ty=4
struct ContentInstance : ResourceBase {
    Optional<u32>     stateTag;           // st
    Optional<CString> creator;            // cr
    Optional<CString> contentInfo;        // cnf  e.g. "application/json:1"
    s64               contentSize = 0;    // cs
    Optional<CString> contentRef;         // conr
    CString           content;            // con  (may be base64)
    Optional<CString> ontologyRef;        // or
    Optional<CString> dataGenerationTime; // dgt
    Optional<u32>     deletionCnt;        // dcnt
};

// Group (grp), ty=9
struct Group : ResourceBase {
    Optional<CString>      creator;                                                  // cr
    Optional<u32>          stateTag;                                                 // st
    ResourceType           memberType          = ResourceType::AE;                   // mt
    boolean                memberTypeValidated = false;                              // mtv
    ConsistencyStrategy    consistencyStrategy = ConsistencyStrategy::AbandonMember; // csy
    s32                    currentNrOfMembers  = 0;                                  // cnm
    s32                    maxNrOfMembers      = 0;                                  // mnm
    Vector<CString>        memberIDs;                                                // mid
    Vector<CString>        membersAccessControlPolicyIDs;                            // macp
    Optional<CString>      groupName;                                                // gn
    Optional<boolean>      semanticSupportIndicator;                                 // ssi
    Optional<boolean>      notifyAggregation;                                        // nar
    Optional<ResourceType> specializationType;                                       // spty
};

// Subscription (sub), ty=23
struct BatchNotify {
    s32               number = 0;
    Optional<CString> duration; // ISO8601 duration (dur)
};

struct RateLimit {
    Optional<s32> maxNrOfNotify;
    Optional<s32> timeWindow; // seconds
};

struct EventNotificationCriteria {
    Optional<CString>             createdBefore;
    Optional<CString>             createdAfter;
    Optional<CString>             modifiedSince;
    Optional<CString>             unmodifiedSince;
    Optional<s32>                 stateTagSmaller;
    Optional<s32>                 stateTagBigger;
    Optional<CString>             expireBefore;
    Optional<CString>             expireAfter;
    Vector<CString>               labels;
    Optional<s64>                 sizeAbove;
    Optional<s64>                 sizeBelow;
    Vector<NotificationEventType> notificationEventType; // net
    Vector<ResourceType>          childResourceType;     // chty
    Vector<CString>               attributeList;         // atr
    Optional<FilterUsage>         filterUsage;
    Optional<CString>             contentFilterQuery;
    Optional<CString>             contentFilterSyntax;
    Optional<boolean>             missingData;
};

struct Subscription : ResourceBase {
    EventNotificationCriteria         eventNotificationCriteria;   // enc
    Optional<s32>                     expirationCounter;           // exc
    Vector<CString>                   notificationURI;             // nu
    Optional<CString>                 groupID;                     // gpi
    Optional<CString>                 notificationForwardingURI;   // nfu
    Optional<BatchNotify>             batchNotify;                 // bn
    Optional<RateLimit>               rateLimit;                   // rl
    Optional<s32>                     preSubscriptionNotify;       // psn
    Optional<u8>                      pendingNotification;         // pn
    Optional<s32>                     notificationStoragePriority; // nsp
    Optional<boolean>                 latestNotify;                // ln
    Optional<NotificationContentType> notificationContentType;     // nct
    Optional<u8>                      notificationEventCat;        // nec
    Optional<CString>                 subscriberURI;               // su
    Optional<CString>                 creator;                     // cr
    Optional<boolean> creatorProvided; // internal flag: cr was explicitly provided (including null)
    Optional<CString> groupName;       // gn
    Optional<CString> associatedCrossResourceSub; // acrs
};

// RemoteCSE (csr), ty=16
struct RemoteCSE : ResourceBase {
    Optional<CSEType> cseType;                  // cst
    CString           cseID;                    // csi
    Optional<CString> cseBase;                  // cb
    Optional<boolean> requestReachability;      // rr
    Vector<CString>   pointOfAccess;            // poa
    Optional<CString> nodeLink;                 // nl
    Optional<CString> m2mExtID;                 // mei
    Optional<CString> triggerRecipientID;       // tri
    Optional<u32>     triggerReferenceNumber;   // trn
    Optional<boolean> triggerEnable;            // tren
    Vector<CString>   supportedReleaseVersions; // srv
    Optional<CString> e2eSecInfo;               // esi
};

// AccessControlPolicy (acp), ty=1
struct AccessControlIpAddresses {
    Vector<CString> ipv4; // ipv4
    Vector<CString> ipv6; // ipv6
};

struct AccessControlLocationRegion {
    Vector<CString>   countryCode; // accc
    Optional<CString> circRegion;  // accr
};

struct AccessControlContexts {
    Vector<CString>                       accessControlWindow;         // actw
    Optional<AccessControlIpAddresses>    accessControlIpAddresses;    // acip
    Optional<AccessControlLocationRegion> accessControlLocationRegion; // aclr
};

struct AccessControlRule {
    Vector<CString>                 accessControlOriginators;    // acor
    u8                              accessControlOperations = 0; // acop bitmask
    Optional<AccessControlContexts> accessControlContexts;       // acco
    Optional<boolean>               accessControlAuthFlag;       // acaf
};

struct AccessControlPolicy : ResourceBase {
    Vector<AccessControlRule> privileges;         // pv
    Vector<AccessControlRule> selfPrivileges;     // pvs
    Optional<CString>         authDecisionResIDs; // adri
    Optional<CString>         authPolicyResIDs;   // apri
    Optional<CString>         authInfoResIDs;     // airi
};

// MgmtCmd (mgc), ty=12
struct MgmtCmd : ResourceBase {
    Optional<CString>  description;        // dc
    CString            cmdType;            // cmt
    CString            execTarget;         // ext
    Optional<ExecMode> execMode;           // exm
    Optional<s32>      execFrequency;      // exf
    Optional<s32>      execDelay;          // exy
    Optional<s32>      execNumber;         // exn
    Optional<CString>  execReqArgs;        // exra
    boolean            execEnable = false; // exe
};

// ExecInstance (exin), ty=8
struct ExecInstance : ResourceBase {
    ExecStatus         execStatus = ExecStatus::Initiated; // exs
    Optional<CString>  execResult;                         // exr
    boolean            execDisable = false;                // exd
    CString            execTarget;                         // ext
    Optional<ExecMode> execMode;                           // exm
    Optional<s32>      execFrequency;                      // exf
    Optional<s32>      execDelay;                          // exy
    Optional<s32>      execNumber;                         // exn
    Optional<CString>  execReqArgs;                        // exra
};

// TimeSeries (ts), ty=29
struct TimeSeries : ResourceBase {
    Optional<CString> creator;                   // cr
    Optional<s64>     maxNrOfInstances;          // mni
    Optional<s64>     maxByteSize;               // mbs
    Optional<s64>     maxInstanceAge;            // mia
    s64               currentNrOfInstances = 0;  // cni
    s64               currentByteSize      = 0;  // cbs
    Optional<CString> ontologyRef;               // or
    Optional<s32>     periodicInterval;          // pei  (ms)
    Optional<s32>     periodicIntervalDelta;     // peid (ms)
    boolean           missingDataDetect = false; // mdd
    Optional<s32>     missingDataMaxNr;          // mdn
    Optional<s32>     missingDataCurrentNr;      // mdc
    Optional<CString> missingDataDetectTimer;    // mdt
};

// TimeSeriesInstance (tsi), ty=30
struct TimeSeriesInstance : ResourceBase {
    Optional<CString> contentInfo;        // cnf
    s64               contentSize = 0;    // cs
    CString           content;            // con
    Optional<CString> dataGenerationTime; // dgt
    Optional<u32>     sequenceNr;         // snr
};

// Schedule (sch), ty=18
struct ScheduleElement {
    Vector<CString>   scheduleEntry;      // sce  (cron-like expressions)
    Optional<boolean> networkCoordinated; // nco
};

struct Schedule : ResourceBase {
    ScheduleElement scheduleElement; // se
};

// Request resource (req), ty=17
// (stores results of non-blocking request processing)
struct RequestResource : ResourceBase {
    CString           originator;                      // org
    Optional<CString> metaInformation;                 // mi
    u8                requestStatus = 0;               // rs
    Optional<CString> operationResult;                 // ors
    Operation         operation = Operation::Retrieve; // op
    CString           requestID;                       // rid
    Optional<CString> target;                          // tg
};

// PollingChannel (pch), ty=15
struct PollingChannel : ResourceBase {
    Optional<boolean> requestAggregation; // rqag
};

// Node (nod), ty=14
struct Node : ResourceBase {
    CString           nodeID;             // ni
    Optional<CString> hostedCSELink;      // hcl
    Optional<CString> mgmtClientAddress;  // mgca
    Vector<CString>   hostedAELinks;      // hael
    Vector<CString>   hostedServiceLinks; // hsl
    Optional<CString> networkID;          // nid
    Optional<boolean> roamingStatus;      // rms
    Optional<u8>      nodeType;           // nty
};

// FlexContainer, ty=28001+ (specialization)
// Content is untyped; use a key-value map for custom attributes.
struct FlexContainer : ResourceBase {
    CString           containerDefinition;      // cnd
    Optional<CString> ontologyRef;              // or
    Optional<s64>     maxNrOfInstances;         // mni
    Optional<s64>     maxByteSize;              // mbs
    Optional<s64>     maxInstanceAge;           // mia
    s64               currentNrOfInstances = 0; // cni
    s64               currentByteSize      = 0; // cbs
    Optional<CString> creator;                  // cr
    Optional<u32>     stateTag;                 // st
    // Custom attributes specific to the FlexContainer definition
    Vector<CustomAttribute> customAttributes; // arbitrary short-named attrs
};

struct OperationMonitor {
    Operation operation;  // op
    CString   originator; // org
};

// notificationEvent (nev)
struct NotificationEvent {
    PrimitiveContent          *representation;        // rep
    Optional<OperationMonitor> operationMonitor;      // om
    NotificationEventType      notificationEventType; // net
};

// idr
struct IPEDiscoveryRequest {
    CString originator; // org

    // TS-0004 says filterCriteria is mandatory
    // You already have most of these fields in EventNotificationCriteria
    EventNotificationCriteria filterCriteria; // fc
};

// m2m:notification (sgn)
struct Notification {
    Optional<NotificationEvent> notificationEvent; // nev

    Optional<boolean> verificationRequest;  // vrq
    Optional<boolean> subscriptionDeletion; // sud

    CString subscriptionReference; // sur

    Optional<CString> creator;                   // cr
    Optional<CString> notificationForwardingURI; // nfu
    Optional<CString> notificationTarget;        // ntt

    Optional<boolean> targetRemovalRequest;   // trr
    Optional<boolean> targetRemovalAllowance; // tra

    Optional<IPEDiscoveryRequest> ipeDiscoveryRequest; // idr

    Optional<boolean> aeRegistrationPointChange; // aerp
    Optional<boolean> aeReferenceIDChange;       // aerid

    Optional<CString> trackingID1; // tid1
    Optional<CString> trackingID2; // tid2

    // Present in newer releases
    Optional<CString> subscribedTo; // st
};

} // namespace zerom2m::onem2m::types
