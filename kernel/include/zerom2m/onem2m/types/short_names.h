/*
 * short_names.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

namespace zerom2m::onem2m::types::sn
{

// Table 8.2.2-1 : Primitive parameter short names
namespace prim
{
constexpr const char *OPERATION                      = "op";
constexpr const char *TO                             = "to";
constexpr const char *FROM                           = "fr";
constexpr const char *REQUEST_IDENTIFIER             = "rqi";
constexpr const char *RESOURCE_TYPE                  = "ty";
constexpr const char *CONTENT                        = "pc";
constexpr const char *ROLE_IDS                       = "rids";
constexpr const char *ORIGINATING_TIMESTAMP          = "ot";
constexpr const char *REQUEST_EXPIRATION_TIMESTAMP   = "rqet";
constexpr const char *RESULT_EXPIRATION_TIMESTAMP    = "rset";
constexpr const char *OPERATION_EXECUTION_TIME       = "oet";
constexpr const char *RESPONSE_TYPE                  = "rt";
constexpr const char *RESULT_PERSISTENCE             = "rp";
constexpr const char *RESULT_CONTENT                 = "rcn";
constexpr const char *EVENT_CATEGORY                 = "ec";
constexpr const char *DELIVERY_AGGREGATION           = "da";
constexpr const char *GROUP_REQUEST_IDENTIFIER       = "gid";
constexpr const char *FILTER_CRITERIA                = "fc";
constexpr const char *DESIRED_IDENTIFIER_RESULT_TYPE = "drt";
constexpr const char *RESPONSE_STATUS_CODE           = "rsc";
constexpr const char *TOKENS                         = "tkns";
constexpr const char *TOKEN_IDS                      = "tids";
constexpr const char *TOKEN_REQUEST_INDICATOR        = "tqi";
constexpr const char *LOCAL_TOKEN_IDS                = "ltids";
constexpr const char *GROUP_REQUEST_TARGET_MEMBERS   = "grtm";
constexpr const char *GROUP_SOMECAST_TARGET_NUMBER   = "gstn";
constexpr const char *ASSIGNED_TOKEN_IDENTIFIERS     = "ati";
constexpr const char *TOKEN_REQUEST_INFORMATION      = "tqf";
constexpr const char *CONTENT_STATUS                 = "cnst";
constexpr const char *CONTENT_OFFSET                 = "cnot";
constexpr const char *AUTHOR_SIGN_INDICATOR          = "asi";
constexpr const char *AUTHOR_SIGN_REQ_INFO           = "asri";
constexpr const char *AUTHOR_SIGNS                   = "aus";
constexpr const char *AUTHOR_REL_INDICATOR           = "auri";
constexpr const char *SEMANTIC_QUERY_INDICATOR       = "sqi";
constexpr const char *ONTOLOGY_MAPPING_RESOURCES     = "omr";
constexpr const char *RELEASE_VERSION_INDICATOR      = "rvi";
constexpr const char *VENDOR_INFORMATION             = "vsi";
constexpr const char *PRIMITIVE_PROFILE_IDENTIFIER   = "prpi";
constexpr const char *M2M_SERVICE_USER               = "msu";
} // namespace prim

// Table 8.2.2-2 : Primitive root element short names
namespace root
{
constexpr const char *REQUEST_PRIMITIVE  = "rqp";
constexpr const char *RESPONSE_PRIMITIVE = "rsp";
} // namespace root

// Table 8.2.3 : Resource attribute short names (selected subset)
namespace attr
{
// Common to all/most resources
constexpr const char *ACCESS_CONTROL_POLICY_IDS = "acpi";
constexpr const char *ANNOUNCED_ATTRIBUTE       = "aa";
constexpr const char *ANNOUNCE_TO               = "at";
constexpr const char *ANNOUNCE_SYNC_TYPE        = "ast";
constexpr const char *CREATION_TIME             = "ct";
constexpr const char *EXPIRATION_TIME           = "et";
constexpr const char *LABELS                    = "lbl";
constexpr const char *LAST_MODIFIED_TIME        = "lt";
constexpr const char *LINK                      = "lnk";
constexpr const char *PARENT_ID                 = "pi";
constexpr const char *RESOURCE_ID               = "ri";
constexpr const char *RESOURCE_TYPE             = "ty";
constexpr const char *STATE_TAG                 = "st";
constexpr const char *RESOURCE_NAME             = "rn";
constexpr const char *PRIVILEGES                = "pv";
constexpr const char *SELF_PRIVILEGES           = "pvs";
constexpr const char *CUSTODIAN                 = "cstn";
constexpr const char *DYNAMIC_AUTH_CONSULT_IDS  = "daci";

// AE
constexpr const char *APP_ID                    = "api";
constexpr const char *AE_ID                     = "aei";
constexpr const char *APP_NAME                  = "apn";
constexpr const char *POINT_OF_ACCESS           = "poa";
constexpr const char *ONTOLOGY_REF              = "or";
constexpr const char *NODE_LINK                 = "nl";
constexpr const char *CONTENT_SERIALIZATION     = "csz";
constexpr const char *REGISTRATION_STATUS       = "regs";
constexpr const char *TRACK_REGISTRATION_POINTS = "trps";
constexpr const char *SESSION_CAPABILITIES      = "scp";
constexpr const char *TRIGGER_ENABLE            = "tren";

// Container / TimeSeries / FlexContainer
constexpr const char *CREATOR                 = "cr";
constexpr const char *MAX_NR_OF_INSTANCES     = "mni";
constexpr const char *MAX_BYTE_SIZE           = "mbs";
constexpr const char *MAX_BYTE_SIZE_PER_INST  = "mbis";
constexpr const char *MAX_INSTANCE_AGE        = "mia";
constexpr const char *CURRENT_NR_OF_INSTANCES = "cni";
constexpr const char *CURRENT_BYTE_SIZE       = "cbs";
constexpr const char *LOCATION_ID             = "li";
constexpr const char *DISABLE_RETRIEVAL       = "disr";
constexpr const char *CONTAINER_DEFINITION    = "cnd";
constexpr const char *LOCATION                = "loc";

// ContentInstance
constexpr const char *CONTENT_INFO         = "cnf";
constexpr const char *CONTENT_SIZE         = "cs";
constexpr const char *CONTENT_REF          = "conr";
constexpr const char *CONTENT              = "con";
constexpr const char *DELETION_CNT         = "dcnt";
constexpr const char *DATA_GENERATION_TIME = "dgt";

// CSEBase / remoteCSE
constexpr const char *CSE_TYPE                   = "cst";
constexpr const char *CSE_ID                     = "csi";
constexpr const char *SUPPORTED_RESOURCE_TYPE    = "srt";
constexpr const char *NOTIF_CONGESTION_POLICY    = "ncp";
constexpr const char *CURRENT_TIME               = "ctm";
constexpr const char *CSE_BASE                   = "cb";
constexpr const char *REQUEST_REACHABILITY       = "rr";
constexpr const char *SUPPORTED_RELEASE_VERSIONS = "srv";
constexpr const char *E2E_SEC_INFO               = "esi";

// Group
constexpr const char *MEMBER_TYPE           = "mt";
constexpr const char *CURRENT_NR_OF_MEMBERS = "cnm";
constexpr const char *MAX_NR_OF_MEMBERS     = "mnm";
constexpr const char *MEMBER_IDS            = "mid";
constexpr const char *MEMBERS_ACP_IDS       = "macp";
constexpr const char *MEMBER_TYPE_VALIDATED = "mtv";
constexpr const char *CONSISTENCY_STRATEGY  = "csy";
constexpr const char *GROUP_NAME            = "gn";

// Subscription
constexpr const char *EVENT_NOTIFICATION_CRITERIA   = "enc";
constexpr const char *EXPIRATION_COUNTER            = "exc";
constexpr const char *NOTIFICATION_URI              = "nu";
constexpr const char *GROUP_ID                      = "gpi";
constexpr const char *NOTIFICATION_FORWARDING_URI   = "nfu";
constexpr const char *BATCH_NOTIFY                  = "bn";
constexpr const char *RATE_LIMIT                    = "rl";
constexpr const char *PRE_SUBSCRIPTION_NOTIFY       = "psn";
constexpr const char *PENDING_NOTIFICATION          = "pn";
constexpr const char *NOTIFICATION_STORAGE_PRIORITY = "nsp";
constexpr const char *LATEST_NOTIFY                 = "ln";
constexpr const char *NOTIFICATION_CONTENT_TYPE     = "nct";
constexpr const char *NOTIFICATION_EVENT_CAT        = "nec";
constexpr const char *SUBSCRIBER_URI                = "su";

// Request resource
constexpr const char *ORIGINATOR       = "org";
constexpr const char *META_INFORMATION = "mi";
constexpr const char *REQUEST_STATUS   = "rs";
constexpr const char *OPERATION_RESULT = "ors";
constexpr const char *OPERATION        = "op";
constexpr const char *REQUEST_ID       = "rid";

// TimeSeries
constexpr const char *PERIODIC_INTERVAL       = "pei";
constexpr const char *PERIODIC_INTERVAL_DELTA = "peid";
constexpr const char *MISSING_DATA_DETECT     = "mdd";
constexpr const char *MISSING_DATA_MAX_NR     = "mdn";
constexpr const char *MISSING_DATA_LIST       = "mdlt";
constexpr const char *MISSING_DATA_CURRENT_NR = "mdc";
constexpr const char *SEQUENCE_NR             = "snr";

// mgmtCmd / execInstance
constexpr const char *DESCRIPTION    = "dc";
constexpr const char *CMD_TYPE       = "cmt";
constexpr const char *EXEC_STATUS    = "exs";
constexpr const char *EXEC_RESULT    = "exr";
constexpr const char *EXEC_DISABLE   = "exd";
constexpr const char *EXEC_TARGET    = "ext";
constexpr const char *EXEC_MODE      = "exm";
constexpr const char *EXEC_FREQUENCY = "exf";
constexpr const char *EXEC_DELAY     = "exy";
constexpr const char *EXEC_NUMBER    = "exn";
constexpr const char *EXEC_REQ_ARGS  = "exra";
constexpr const char *EXEC_ENABLE    = "exe";

// AccessControlPolicy
constexpr const char *AUTH_DECISION_RES_IDS    = "adri";
constexpr const char *AUTH_POLICY_RES_IDS      = "apri";
constexpr const char *AUTH_INFORMATION_RES_IDS = "airi";
} // namespace attr

// Table 8.2.5-1 : Complex data type member short names (key subset)
namespace dt
{

namespace fc
{
// FilterCriteria / EventNotificationCriteria
constexpr const char *CREATED_BEFORE        = "crb";
constexpr const char *CREATED_AFTER         = "cra";
constexpr const char *MODIFIED_SINCE        = "ms";
constexpr const char *UNMODIFIED_SINCE      = "us";
constexpr const char *STATE_TAG_SMALLER     = "sts";
constexpr const char *STATE_TAG_BIGGER      = "stb";
constexpr const char *EXPIRE_BEFORE         = "exb";
constexpr const char *EXPIRE_AFTER          = "exa";
constexpr const char *LABELS                = "lbl";
constexpr const char *LABELS_QUERY          = "lbq";
constexpr const char *RESOURCE_TYPE         = "ty";
constexpr const char *SIZE_ABOVE            = "sza";
constexpr const char *SIZE_BELOW            = "szb";
constexpr const char *CONTENT_TYPE          = "cty";
constexpr const char *LIMIT                 = "lim";
constexpr const char *ATTRIBUTE             = "atr";
constexpr const char *CONTENT_FILTER_SYNTAX = "cfs";
constexpr const char *CONTENT_FILTER_QUERY  = "cfq";
constexpr const char *LEVEL                 = "lvl";
constexpr const char *OFFSET                = "ofst";
constexpr const char *GEO_QUERY             = "gq";
constexpr const char *FILTER_USAGE          = "fu";
constexpr const char *FILTER_OPERATION      = "fo";
constexpr const char *SEMANTICS_FILTER      = "smf";
} // namespace fc

// Notification
constexpr const char *NOTIFICATION           = "sgn";
constexpr const char *NOTIFICATION_EVENT     = "nev";
constexpr const char *VERIFICATION_REQUEST   = "vrq";
constexpr const char *SUBSCRIPTION_DELETION  = "sud";
constexpr const char *SUBSCRIPTION_REFERENCE = "sur";

// AccessControlRule
constexpr const char *ACCESS_CONTROL_RULE        = "acr";
constexpr const char *ACCESS_CONTROL_ORIGINATORS = "acor";
constexpr const char *ACCESS_CONTROL_OPERATIONS  = "acop";
constexpr const char *ACCESS_CONTROL_CONTEXTS    = "acco";
constexpr const char *ACCESS_CONTROL_WINDOW      = "actw";
constexpr const char *ACCESS_CONTROL_IP_ADDRS    = "acip";
constexpr const char *IPV4_ADDRESSES             = "ipv4";
constexpr const char *IPV6_ADDRESSES             = "ipv6";
constexpr const char *ACCESS_CONTROL_LOC_REGION  = "aclr";

// BatchNotify / MissingData
constexpr const char *NUMBER   = "num";
constexpr const char *DURATION = "dur";

// ChildResource
constexpr const char *CHILD_RESOURCE = "ch";
constexpr const char *NAME           = "nm";
constexpr const char *TYPE           = "typ";
constexpr const char *VALUE          = "val";

// Response content wrappers
constexpr const char *AGGREGATED_RESPONSE     = "agr";
constexpr const char *RESOURCE                = "rce";
constexpr const char *URI_LIST                = "uril";
constexpr const char *DEBUG_INFO              = "dbg";
constexpr const char *AGGREGATED_NOTIFICATION = "agn";
constexpr const char *ATTRIBUTE_LIST          = "atrl";

// EventNotificationCriteria
constexpr const char *NOTIFICATION_EVENT_TYPE = "net";
constexpr const char *OPERATION_MONITOR       = "om";
constexpr const char *MISSING_DATA            = "md";

// EventCategory
constexpr const char *EVENT_CAT_TYPE = "ect";
constexpr const char *EVENT_CAT_NO   = "ecn";

// RateLimit
constexpr const char *MAX_NR_OF_NOTIFY = "mnn";
constexpr const char *TIME_WINDOW      = "tww";
} // namespace dt

} // namespace zerom2m::onem2m::types::sn
