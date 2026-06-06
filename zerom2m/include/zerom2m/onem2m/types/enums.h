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

// Operation (op),
enum class Operation : u8 {
    Create      = 1,
    Retrieve    = 2,
    Update      = 3,
    Delete      = 4,
    Notify      = 5,
    Unsupported = 0 // internal value for unsupported operations
};

// ResourceType (ty),
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

    // XXX: Notification does not have a ty value in the oneM2M spec, but we need it for internal
    // handling
    Notification = 10000,

    // Internal
    None = 0,
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
    OK                                                = 2000,
    DELETED                                           = 2002,
    UPDATED                                           = 2004,
    CREATED                                           = 2001,
    ACCEPTED                                          = 1000,
    ACCEPTED_NON_BLOCKING_REQUEST_SYNCH               = 1001,
    ACCEPTED_NON_BLOCKING_REQUEST_ASYNCH              = 1002,
    BAD_REQUEST                                       = 4000,
    CONTENTS_UNACCEPTABLE                             = 4102,
    GROUP_MEMBER_TYPE_INCONSISTENT                    = 4110,
    INVALID_SEMANTICS                                 = 4120,
    INVALID_TRIGGER_PURPOSE                           = 4122,
    ILLEGAL_TRANSACTION_STATE_TRANSITION_ATTEMPTED    = 4123,
    ONTOLOGY_MAPPING_POLICY_NOT_MATCHED               = 4131,
    BAD_FACT_INPUTS_FOR_REASONING                     = 4133,
    BAD_RULE_INPUTS_FOR_REASONING                     = 4134,
    PRIMITIVE_PROFILE_BAD_REQUEST                     = 4137,
    INVALID_PROCESS_CONFIGURATION                     = 4142,
    INVALID_SPARQL_QUERY                              = 4143,
    MAX_NUMBER_OF_MEMBER_EXCEEDED                     = 6010,
    INVALID_CMDTYPE                                   = 6022,
    INVALID_ARGUMENTS                                 = 6023,
    INSUFFICIENT_ARGUMENTS                            = 6024,
    SUBSCRIPTION_CREATOR_HAS_NO_PRIVILEGE             = 4101,
    ORIGINATOR_HAS_NO_PRIVILEGE                       = 4103,
    RECEIVER_HAS_NO_PRIVILEGE                         = 5105,
    TARGET_NOT_SUBSCRIBABLE                           = 5203,
    SUBSCRIPTION_HOST_HAS_NO_PRIVILEGE                = 5205,
    ORIGINATOR_NOT_AUTHENTICATED                      = 4106,
    SECURITY_ASSOCIATION_REQUIRED                     = 4107,
    INVALID_CHILD_RESOURCE_TYPE                       = 4108,
    NO_MEMBERS                                        = 4109,
    LinkedSubscriptionNotExist                        = 4103,
    ESPRIM_UNSUPPORTED_OPTION                         = 4111,
    ESPRIM_UNKNOWN_KEY_ID                             = 4112,
    ESPRIM_UNKNOWN_ORIG_RAND_ID                       = 4113,
    ESPRIM_UNKNOWN_RECV_RAND_ID                       = 4114,
    ESPRIM_BAD_MAC                                    = 4115,
    ESPRIM_IMPERSONATION_ERROR                        = 4116,
    ORIGINATOR_HAS_ALREADY_REGISTERED                 = 4117,
    APP_RULE_VALIDATION_FAILED                        = 4126,
    OPERATION_DENIED_BY_REMOTE_ENTITY                 = 4127,
    SERVICE_SUBSCRIPTION_NOT_ESTABLISHED              = 4128,
    DISCOVERY_LIMIT_EXCEEDED                          = 4135,
    PRIMITIVE_PROFILE_NOT_ACCESSIBLE                  = 4136,
    UNAUTHORIZED_USER                                 = 4138,
    SERVICE_SUBSCRIPTION_NOT_ACTIVE                   = 4139,
    DISCOVERY_DENIED_BY_IPE                           = 5208,
    TARGET_HAS_NO_SESSION_CAPABILITY                  = 5214,
    SESSION_IS_ONLINE                                 = 5215,
    TRIGGERING_DISABLED_FOR_RECIPIENT                 = 5218,
    TRANSACTION_PROCESSING_IS_INCOMPLETE              = 5222,
    REQUESTED_ACTIVITY_PATTERN_NOT_PERMITTED          = 6034,
    NOT_FOUND                                         = 4004,
    ONTOLOGY_NOT_AVAILABLE                            = 4118,
    LINKED_SEMANTICS_NOT_AVAILABLE                    = 4119,
    MASHUP_MEMBER_NOT_FOUND                           = 4121,
    ONTOLOGY_MAPPING_ALGORITHM_NOT_AVAILABLE          = 4130,
    ONTOLOGY_MAPPING_NOT_AVAILABLE                    = 4132,
    TARGET_NOT_REACHABLE                              = 5103,
    REMOTE_ENTITY_NOT_REACHABLE                       = 5107,
    EXTERNAL_OBJECT_NOT_REACHABLE                     = 6003,
    EXTERNAL_OBJECT_NOT_FOUND                         = 6005,
    OPERATION_NOT_ALLOWED                             = 4005,
    NOT_ACCEPTABLE                                    = 5207,
    GROUP_REQUEST_IDENTIFIER_EXISTS                   = 4104,
    CONFLICT                                          = 4105,
    BLOCKING_SUBSCRIPTION_ALREADY_EXISTS              = 4124,
    SOFTWARE_CAMPAIGN_CONFLICT                        = 4140,
    ALREADY_EXISTS                                    = 5106,
    UNABLE_TO_REPLACE_REQUEST                         = 5219,
    UNABLE_TO_RECALL_REQUEST                          = 5220,
    FilterCriteriaNotImplemented                      = 5229,
    ALREADY_COMPLETE                                  = 6028,
    MGMT_COMMAND_NOT_CANCELLABLE                      = 6029,
    UNSUPPORTED_MEDIA_TYPE                            = 4015,
    Unsupported                                       = 4015,
    INTERNAL_SERVER_ERROR                             = 5000,
    SUBSCRIPTION_VERIFICATION_INITIATION_FAILED       = 5204,
    GROUP_MEMBERS_NOT_RESPONDED                       = 5209,
    ESPRIM_DECRYPTION_ERROR                           = 5210,
    ESPRIM_ENCRYPTION_ERROR                           = 5211,
    SPARQL_UPDATE_ERROR                               = 5212,
    JOIN_MULTICAST_GROUP_FAILED                       = 5216,
    LEAVE_MULTICAST_GROUP_FAILED                      = 5217,
    CROSS_RESOURCE_OPERATION_FAILURE                  = 5221,
    ONTOLOGY_MAPPING_ALGORITHM_FAILED                 = 5230,
    ONTOLOGY_CONVERSION_FAILED                        = 5231,
    REASONING_PROCESSING_FAILED                       = 5232,
    MGMT_SESSION_CANNOT_BE_ESTABLISHED                = 6020,
    MGMT_SESSION_ESTABLISHMENT_TIMEOUT                = 6021,
    MGMT_CONVERSION_ERROR                             = 6025,
    MGMT_CANCELLATION_FAILED                          = 6026,
    NETWORK_QOS_CONFIG_ERROR                          = 6033,
    RELEASE_VERSION_NOT_SUPPORTED                     = 4001,
    SPECIALIZATION_SCHEMA_NOT_FOUND                   = 4125,
    NOT_IMPLEMENTED                                   = 5001,
    NON_BLOCKING_SYNCH_REQUEST_NOT_SUPPORTED          = 5206,
    REQUEST_TIMEOUT                                   = 4008,
    RequestTimeout                                    = 4008,
    EXTERNAL_OBJECT_NOT_REACHABLE_BEFORE_RQET_TIMEOUT = 6030,
    EXTERNAL_OBJECT_NOT_REACHABLE_BEFORE_OET_TIMEOUT  = 6031,

    // Internal
    UNKNOWN_STATUS = 0,
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
    AllAttributes      = 1,
    ModifiedAttributes = 2,
    ResourceID         = 3,
    TriggerPayload     = 4,
    TimeSeries         = 5,
};

// NotificationEventType (net)
enum class NotificationEventType : u8 {
    UpdateOfResource                               = 1,
    DeleteOfResource                               = 2,
    CreateOfDirectChildResource                    = 3,
    DeleteOfDirectChildResource                    = 4,
    RetrieveOfContainerResourceWithNoChildResource = 5,
    TriggerReceivedForAEResource                   = 6,
    BlockingUpdate                                 = 7,
    ReportOnMissingDataPoints                      = 8
};

// MemberType (mt), for <group>
enum class MemberType : u32 {
    Mixed = 0, // heterogeneous
    // Values correspond to ResourceType values
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
