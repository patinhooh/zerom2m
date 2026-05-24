/*
 * enums.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include <circle/types.h>
#include <circle/util.h>

namespace zerom2m::onem2m::types
{

// Content-Type media type strings
namespace mime
{
constexpr const char *JSON = "application/json";
constexpr const char *XML  = "application/xml";
constexpr const char *CBOR = "application/cbor";
// oneM2M-specific structured Content-Types
constexpr const char *JSON_M2M = "application/vnd.onem2m-res+json";
constexpr const char *XML_M2M  = "application/vnd.onem2m-res+xml";
constexpr const char *CBOR_M2M = "application/vnd.onem2m-res+cbor";
} // namespace mime

// Operation (op), Table 8.2.2-1, value definitions from TS-0001
enum class Operation : u8 {
    Create      = 1,
    Retrieve    = 2,
    Update      = 3,
    Delete      = 4,
    Notify      = 5,
    Unsupported = 0 // internal value for unsupported operations
};

// ResourceType (ty), Table 8.2.4-1
enum class ResourceType : u32 {
    // Core resource types
    AccessControlPolicy             = 1,
    AE                              = 2,
    Container                       = 3,
    ContentInstance                 = 4,
    CSEBase                         = 5,
    Delivery                        = 6,
    EventConfig                     = 7,
    ExecInstance                    = 8,
    Group                           = 9,
    LocationPolicy                  = 10,
    M2MServiceSubscriptionProfile   = 11,
    MgmtCmd                         = 12,
    MgmtObj                         = 13, // virtual
    Node                            = 14,
    PollingChannel                  = 15,
    RemoteCSE                       = 16,
    Request                         = 17,
    Schedule                        = 18,
    ServiceSubscribedAppRule        = 19,
    ServiceSubscribedNode           = 20,
    StatsCollect                    = 21,
    StatsConfig                     = 22,
    Subscription                    = 23,
    SemanticDescriptor              = 24,
    NotificationTargetMgmtPolicyRef = 28,
    NotificationTargetPolicy        = 29,
    PolicyDeletionRules             = 30,
    FlexContainer                   = 28001, // specialization base
    TimeSeries                      = 29,
    TimeSeriesInstance              = 30,
    Role                            = 31,
    Token                           = 32,
    // Management resources
    Firmware          = 1001,
    Software          = 1002,
    Memory            = 1003,
    AreaNwkInfo       = 1004,
    AreaNwkDeviceInfo = 1005,
    Battery           = 1006,
    DeviceInfo        = 1007,
    DeviceCapability  = 1008,
    Reboot            = 1009,
    EventLog          = 1010,
    // CMDH
    CmdhPolicy             = 2001,
    ActiveCmdhPolicy       = 2002,
    CmdhDefaults           = 2003,
    CmdhDefEcValue         = 2004,
    CmdhEcDefParamValues   = 2005,
    CmdhLimits             = 2006,
    CmdhNetworkAccessRules = 2007,
    CmdhNwAccessRule       = 2008,
    CmdhBuffer             = 2009,
    // Dynamic authorization
    DynamicAuthorizationConsultation = 33,
    AuthorizationDecision            = 3001,
    AuthorizationPolicy              = 3002,
    AuthorizationInformation         = 3003,
    // Ontology / Semantic
    OntologyRepository                 = 3501,
    Ontology                           = 3502,
    OntologyMapping                    = 3503,
    OntologyMappingAlgorithm           = 3504,
    OntologyMappingAlgorithmRepository = 3505,
    SemanticMashupJobProfile           = 3506,
    SemanticMashupInstance             = 3507,
    SemanticMashupResult               = 3508,
    SemanticRuleRepository             = 3509,
    ReasoningRules                     = 3510,
    ReasoningJobInstance               = 3511,
    // Network / Comms
    AEContactList             = 4001,
    AEContactListPerCSE       = 4002,
    LocalMulticastGroup       = 4003,
    MultimediaSession         = 4004,
    TriggerRequest            = 4005,
    CrossResourceSubscription = 4006,
    BackgroundDataTransfer    = 4007,
    Transaction               = 4008,
    TransactionMgmt           = 4009,
    E2eQosSession             = 4010,
    TimeSyncBeacon            = 4011,
    NwMonitoringReq           = 4012,
    PrimitiveProfile          = 4013,
    // Process management
    ProcessManagement = 4014,
    State             = 4015,
    Action            = 4016,
    Dependency        = 4017,
    // Service subscriptions
    ServiceSubscribedUserProfile = 4018,
    SoftwareCampaign             = 4019,
};

struct ResourceTypeName {
    const char  *shortName;
    ResourceType type;
};

inline constexpr ResourceTypeName kResourceTypeNames[] = {
    {"acp", ResourceType::AccessControlPolicy},
    {"ae", ResourceType::AE},
    {"cnt", ResourceType::Container},
    {"cin", ResourceType::ContentInstance},
    {"cb", ResourceType::CSEBase},
    {"dlv", ResourceType::Delivery},
    {"evcg", ResourceType::EventConfig},
    {"exin", ResourceType::ExecInstance},
    {"grp", ResourceType::Group},
    {"lcp", ResourceType::LocationPolicy},
    {"mssp", ResourceType::M2MServiceSubscriptionProfile},
    {"mgc", ResourceType::MgmtCmd},
    {"nod", ResourceType::Node},
    {"pch", ResourceType::PollingChannel},
    {"csr", ResourceType::RemoteCSE},
    {"req", ResourceType::Request},
    {"sch", ResourceType::Schedule},
    {"asar", ResourceType::ServiceSubscribedAppRule},
    {"svsn", ResourceType::ServiceSubscribedNode},
    {"stcl", ResourceType::StatsCollect},
    {"stcg", ResourceType::StatsConfig},
    {"sub", ResourceType::Subscription},
    {"smd", ResourceType::SemanticDescriptor},
    {"ts", ResourceType::TimeSeries},
    {"tsi", ResourceType::TimeSeriesInstance},
    {"rol", ResourceType::Role},
    {"tk", ResourceType::Token},
    {"fwr", ResourceType::Firmware},
    {"swr", ResourceType::Software},
    {"mem", ResourceType::Memory},
    {"ani", ResourceType::AreaNwkInfo},
    {"andi", ResourceType::AreaNwkDeviceInfo},
    {"bat", ResourceType::Battery},
    {"dvi", ResourceType::DeviceInfo},
    {"dvc", ResourceType::DeviceCapability},
    {"rbo", ResourceType::Reboot},
    {"evl", ResourceType::EventLog},
    {"dac", ResourceType::DynamicAuthorizationConsultation},
    {"crs", ResourceType::CrossResourceSubscription},
    {"bdt", ResourceType::BackgroundDataTransfer},
    {"trac", ResourceType::Transaction},
    {"tram", ResourceType::TransactionMgmt},
    {"prp", ResourceType::PrimitiveProfile},
};

inline bool resourceTypeFromShortName(const char *shortName, ResourceType &type)
{
    if (shortName == nullptr) return false;
    for (const auto &entry : kResourceTypeNames) {
        if (strcmp(entry.shortName, shortName) == 0) {
            type = entry.type;
            return true;
        }
    }
    return false;
}

inline const char *shortNameByResourceType(ResourceType type)
{
    for (const auto &entry : kResourceTypeNames) {
        if (entry.type == type) return entry.shortName;
    }
    return nullptr;
}

// ResponseStatusCode (rsc), TS-0001 clause 7
enum class ResponseStatusCode : u32 {
    // 2xxx, Success
    OK      = 2000,
    Created = 2001,
    Deleted = 2002,
    Updated = 2003,
    // 4xxx, Client errors
    BadRequest                   = 4000,
    NotPermitted                 = 4001,
    NotFound                     = 4004,
    OperationNotAllowed          = 4005,
    RequestTimeout               = 4008,
    Unsupported                  = 4015,
    Subscriptioncreatorhasnoself = 4101,
    LinkedSubscriptionNotExist   = 4103,
    GroupMemberTypeInconsistent  = 4107,
    // 5xxx, Server errors
    InternalServerError               = 5000,
    NotImplemented                    = 5001,
    TargetNotReachable                = 5103,
    NoPrivilege                       = 5105,
    AlreadyExists                     = 5106,
    TargetNotSubscribable             = 5203,
    SubscriptionVerificationInitiated = 5204,
    SubscriptionHostHasNoSelf         = 5205,
    NONBlockingRequestNotSupported    = 5206,
    ExternalObjectNotReachable        = 5207,
    ExternalObjectNotFound            = 5208,
    MaxNrOfChildresourcesExceeded     = 5209,
    MaxNrOfMemberExceeded             = 5214,
    MgmtSessionCannotBeEstablished    = 5220,
    MgmtSessionEstablishmentTimeout   = 5221,
    InvalidCmdType                    = 5222,
    InvalidArguments                  = 5223,
    InsufficientArguments             = 5224,
    MgmtConversionError               = 5225,
    MgmtCancelAlreadyInProgress       = 5226,
    FilterCriteriaNotImplemented      = 5229,
    OntologyNotAvailable              = 5231,
    LinkedSemanticResourceNotFound    = 5232,
    SemanticValidationFailed          = 5233,
    ThrottlingLimitExceeded           = 6003,
    // 6xxx, Request/response
    CrossResourceOperationFailed = 6010,
    // Internal
    UnknownStatus = 0,
};

// ResultContent (rcn), TS-0001
enum class ResultContent : u8 {
    Nothing                              = 0,
    Attributes                           = 1,
    HierarchicalAddress                  = 2,
    HierarchicalAddressAndAttributes     = 3,
    AttributesAndChildResources          = 4,
    AttributesAndChildResourceReferences = 5,
    ChildResourceReferences              = 6,
    OriginalResource                     = 7,
    ChildResources                       = 8,
    ModifiedAttributes                   = 9,
    SemanticContent                      = 10,
    DiscoveryResultReferences            = 11,
};

// FilterUsage (fu), TS-0001
enum class FilterUsage : u8 {
    DiscoveryCriteria    = 1,
    ConditionalRetrieval = 2,
    IPEOnDemandDiscovery = 3,
};

// DesiredIdentifierResultType (drt)
enum class DesiredIdentifierResultType : u8 {
    StructuredCSERelativeResourceID   = 1,
    UnstructuredCSERelativeResourceID = 2,
};

// ResponseType (rt)
enum class ResponseType : u8 {
    NonBlockingRequestSynch  = 1,
    NonBlockingRequestAsynch = 2,
    BlockingRequest          = 3, // default
    FlexBlocking             = 4,
    NoResponse               = 5,
};

// NotificationContentType (nct)
enum class NotificationContentType : u8 {
    ModifiedAttributes = 1,
    WholeResource      = 2,
    ReferenceOnly      = 3,
    TriggeringPayload  = 4,
    SemanticSignalling = 5,
};

// NotificationEventType (net)
enum class NotificationEventType : u8 {
    UpdateOfResource                   = 1,
    DeleteOfResource                   = 2,
    CreateOfDirectChildResource        = 3,
    DeleteOfDirectChildResource        = 4,
    RetrieveOfContainerOldestValue     = 5,
    TriggerReceivedForAE               = 6,
    BlockingUpdate                     = 7,
    MissingData                        = 8,
    SemanticTrigger                    = 9,
    TimeSyncNotification               = 10,
    NotifyContainer                    = 11,
    OperationExecutionEnded            = 12,
    ReportOnGeneratedMissingDataPoints = 13,
    CrossResourceSubscriptionCreation  = 14,
    CrossResourceSubscriptionDeletion  = 15,
};

// MemberType (mt), for <group>
enum class MemberType : u32 {
    Mixed = 0, // heterogeneous
    // Values correspond to ResourceType values
    // TODO: CHECK THIS
};

// ConsistencyStrategy (csy)
enum class ConsistencyStrategy : u8 {
    AbandonMember = 1,
    AbandonGroup  = 2,
    SetMixed      = 3,
};

// CSEType (cst)
enum class CSEType : u8 {
    IN_CSE  = 1,
    MN_CSE  = 2,
    ASN_CSE = 3,
};

// AccessControlOperations bitmask
enum class AccessControlOperation : u8 {
    Create   = 1,
    Retrieve = 2,
    Update   = 4,
    Delete   = 8,
    Notify   = 16,
    Discover = 32,
};

// ExecStatus
enum class ExecStatus : u8 {
    Initiated  = 1,
    Pending    = 2,
    Finished   = 3,
    Cancelling = 4,
    Cancelled  = 5,
    Status6    = 6, // reserved
};

// ExecMode
enum class ExecMode : u8 {
    ImmediateOnce      = 1,
    ImmediateAndRepeat = 2,
    RandomOnce         = 3,
    RandomAndRepeat    = 4,
};

} // namespace zerom2m::onem2m::types
