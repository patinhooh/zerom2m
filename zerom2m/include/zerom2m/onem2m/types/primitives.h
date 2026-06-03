/*
 * primitives.h
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
#include <zerom2m/onem2m/types/resources.h>

// Minimal replacement for std::is_same_v (STDLIB_SUPPORT=0 builds). This provides the
// is_same trait and is_same_v helper used below.
template <typename A, typename B> struct is_same {
    static constexpr bool value = false;
};
template <typename A> struct is_same<A, A> {
    static constexpr bool value = true;
};
template <typename A, typename B> inline constexpr bool is_same_v = is_same<A, B>::value;

namespace zerom2m::onem2m::types
{

using namespace zerom2m::compat;

// FilterCriteria (fc), Table 8.2.5-1 + TS-0001 clause 8
struct GeoQuery {
    Optional<u8>      geometryType;       // gmty
    Optional<CString> geometry;           // geom
    Optional<u8>      geoSpatialFunction; // gsf
};

struct FilterCriteria {
    Optional<CString>      createdBefore;       // crb
    Optional<CString>      createdAfter;        // cra
    Optional<CString>      modifiedSince;       // ms
    Optional<CString>      unmodifiedSince;     // us
    Optional<s32>          stateTagSmaller;     // sts
    Optional<s32>          stateTagBigger;      // stb
    Optional<CString>      expireBefore;        // exb
    Optional<CString>      expireAfter;         // exa
    Vector<CString>        labels;              // lbl
    Optional<CString>      labelsQuery;         // lbq
    Vector<ResourceType>   resourceType;        // ty
    Optional<s64>          sizeAbove;           // sza
    Optional<s64>          sizeBelow;           // szb
    Optional<CString>      contentType;         // cty
    Optional<s32>          limit;               // lim
    Vector<CString>        attribute;           // atr
    Optional<CString>      contentFilterSyntax; // cfs
    Optional<CString>      contentFilterQuery;  // cfq
    Optional<s32>          level;               // lvl
    Optional<s32>          offset;              // ofst
    Optional<GeoQuery>     geoQuery;            // gq
    Optional<FilterUsage>  filterUsage;         // fu
    Optional<u8>           filterOperation;     // fo  (0=AND, 1=OR)
    Optional<CString>      semanticsFilter;     // smf
    Optional<ResourceType> childResourceType;   // chty
    Optional<ResourceType> specializationType;  // spty
};

// ResponseTypeInfo, for Non-Blocking requests
struct ResponseTypeInfo {
    ResponseType    responseTypeValue; // rtv
    Vector<CString> notificationURI;   // nu
};

// PrimitiveContent, the "pc" payload; holds one resource or response wrapper
enum class PrimitiveContentKind : u8 {
    None = 0,
    AE,
    Container,
    ContentInstance,
    Group,
    Subscription,
    AccessControlPolicy,
    CSEBase,
    RemoteCSE,
    MgmtCmd,
    ExecInstance,
    TimeSeries,
    TimeSeriesInstance,
    Schedule,
    RequestResource,
    PollingChannel,
    Node,
    FlexContainer,
    Notification,
};

struct PrimitiveContent {
    PrimitiveContent() = default;
    PrimitiveContent(const PrimitiveContent &other) { *this = other; }
    PrimitiveContent(PrimitiveContent &&other)
    {
        *this = other;
        other.Reset();
    }

    ~PrimitiveContent() { Reset(); }

    PrimitiveContent &operator=(const PrimitiveContent &other)
    {
        if (this == &other) { return *this; }
        Reset();
        kind_ = other.kind_;
        switch (other.kind_) {
            case PrimitiveContentKind::AE:
                data_ = new AE(*static_cast<AE *>(other.data_));
                break;
            case PrimitiveContentKind::Container:
                data_ = new Container(*static_cast<Container *>(other.data_));
                break;
            case PrimitiveContentKind::ContentInstance:
                data_ = new ContentInstance(*static_cast<ContentInstance *>(other.data_));
                break;
            case PrimitiveContentKind::Notification:
                data_ = new Notification(*static_cast<Notification *>(other.data_));
                break;
            case PrimitiveContentKind::Group:
                data_ = new Group(*static_cast<Group *>(other.data_));
                break;
            case PrimitiveContentKind::Subscription:
                data_ = new Subscription(*static_cast<Subscription *>(other.data_));
                break;
            case PrimitiveContentKind::AccessControlPolicy:
                data_ = new AccessControlPolicy(*static_cast<AccessControlPolicy *>(other.data_));
                break;
            case PrimitiveContentKind::CSEBase:
                data_ = new CSEBase(*static_cast<CSEBase *>(other.data_));
                break;
            case PrimitiveContentKind::RemoteCSE:
                data_ = new RemoteCSE(*static_cast<RemoteCSE *>(other.data_));
                break;
            case PrimitiveContentKind::MgmtCmd:
                data_ = new MgmtCmd(*static_cast<MgmtCmd *>(other.data_));
                break;
            case PrimitiveContentKind::ExecInstance:
                data_ = new ExecInstance(*static_cast<ExecInstance *>(other.data_));
                break;
            case PrimitiveContentKind::TimeSeries:
                data_ = new TimeSeries(*static_cast<TimeSeries *>(other.data_));
                break;
            case PrimitiveContentKind::TimeSeriesInstance:
                data_ = new TimeSeriesInstance(*static_cast<TimeSeriesInstance *>(other.data_));
                break;
            case PrimitiveContentKind::Schedule:
                data_ = new Schedule(*static_cast<Schedule *>(other.data_));
                break;
            case PrimitiveContentKind::RequestResource:
                data_ = new RequestResource(*static_cast<RequestResource *>(other.data_));
                break;
            case PrimitiveContentKind::PollingChannel:
                data_ = new PollingChannel(*static_cast<PollingChannel *>(other.data_));
                break;
            case PrimitiveContentKind::Node:
                data_ = new Node(*static_cast<Node *>(other.data_));
                break;
            case PrimitiveContentKind::FlexContainer:
                data_ = new FlexContainer(*static_cast<FlexContainer *>(other.data_));
                break;
            case PrimitiveContentKind::None:
            default:
                data_ = nullptr;
                break;
        }
        return *this;
    }

    PrimitiveContent &operator=(PrimitiveContent &&other)
    {
        if (this == &other) { return *this; }
        Reset();
        kind_       = other.kind_;
        data_       = other.data_;
        other.kind_ = PrimitiveContentKind::None;
        other.data_ = nullptr;
        return *this;
    }

    PrimitiveContent &operator=(const AE &value) { return Set(value, PrimitiveContentKind::AE); }
    PrimitiveContent &operator=(const Container &value)
    { return Set(value, PrimitiveContentKind::Container); }
    PrimitiveContent &operator=(const ContentInstance &value)
    { return Set(value, PrimitiveContentKind::ContentInstance); }
    PrimitiveContent &operator=(const Notification &value)
    { return Set(value, PrimitiveContentKind::Notification); }
    PrimitiveContent &operator=(const Group &value)
    { return Set(value, PrimitiveContentKind::Group); }
    PrimitiveContent &operator=(const Subscription &value)
    { return Set(value, PrimitiveContentKind::Subscription); }
    PrimitiveContent &operator=(const AccessControlPolicy &value)
    { return Set(value, PrimitiveContentKind::AccessControlPolicy); }
    PrimitiveContent &operator=(const CSEBase &value)
    { return Set(value, PrimitiveContentKind::CSEBase); }
    PrimitiveContent &operator=(const RemoteCSE &value)
    { return Set(value, PrimitiveContentKind::RemoteCSE); }
    PrimitiveContent &operator=(const MgmtCmd &value)
    { return Set(value, PrimitiveContentKind::MgmtCmd); }
    PrimitiveContent &operator=(const ExecInstance &value)
    { return Set(value, PrimitiveContentKind::ExecInstance); }
    PrimitiveContent &operator=(const TimeSeries &value)
    { return Set(value, PrimitiveContentKind::TimeSeries); }
    PrimitiveContent &operator=(const TimeSeriesInstance &value)
    { return Set(value, PrimitiveContentKind::TimeSeriesInstance); }
    PrimitiveContent &operator=(const Schedule &value)
    { return Set(value, PrimitiveContentKind::Schedule); }
    PrimitiveContent &operator=(const RequestResource &value)
    { return Set(value, PrimitiveContentKind::RequestResource); }
    PrimitiveContent &operator=(const PollingChannel &value)
    { return Set(value, PrimitiveContentKind::PollingChannel); }
    PrimitiveContent &operator=(const Node &value)
    { return Set(value, PrimitiveContentKind::Node); }
    PrimitiveContent &operator=(const FlexContainer &value)
    { return Set(value, PrimitiveContentKind::FlexContainer); }

    bool                 empty() const { return kind_ == PrimitiveContentKind::None; }
    PrimitiveContentKind kind() const { return kind_; }

    template <typename T> const T *GetIf() const
    {
        if constexpr (is_same_v<T, AE>) {
            return kind_ == PrimitiveContentKind::AE ? static_cast<const T *>(data_) : nullptr;
        } else if constexpr (is_same_v<T, Container>) {
            return kind_ == PrimitiveContentKind::Container ? static_cast<const T *>(data_)
                                                            : nullptr;
        } else if constexpr (is_same_v<T, ContentInstance>) {
            return kind_ == PrimitiveContentKind::ContentInstance ? static_cast<const T *>(data_)
                                                                  : nullptr;
        } else if constexpr (is_same_v<T, Notification>) {
            return kind_ == PrimitiveContentKind::Notification ? static_cast<const T *>(data_)
                                                               : nullptr;
        } else if constexpr (is_same_v<T, Group>) {
            return kind_ == PrimitiveContentKind::Group ? static_cast<const T *>(data_) : nullptr;
        } else if constexpr (is_same_v<T, Subscription>) {
            return kind_ == PrimitiveContentKind::Subscription ? static_cast<const T *>(data_)
                                                               : nullptr;
        } else if constexpr (is_same_v<T, AccessControlPolicy>) {
            return kind_ == PrimitiveContentKind::AccessControlPolicy
                       ? static_cast<const T *>(data_)
                       : nullptr;
        } else if constexpr (is_same_v<T, CSEBase>) {
            return kind_ == PrimitiveContentKind::CSEBase ? static_cast<const T *>(data_) : nullptr;
        } else if constexpr (is_same_v<T, RemoteCSE>) {
            return kind_ == PrimitiveContentKind::RemoteCSE ? static_cast<const T *>(data_)
                                                            : nullptr;
        } else if constexpr (is_same_v<T, MgmtCmd>) {
            return kind_ == PrimitiveContentKind::MgmtCmd ? static_cast<const T *>(data_) : nullptr;
        } else if constexpr (is_same_v<T, ExecInstance>) {
            return kind_ == PrimitiveContentKind::ExecInstance ? static_cast<const T *>(data_)
                                                               : nullptr;
        } else if constexpr (is_same_v<T, TimeSeries>) {
            return kind_ == PrimitiveContentKind::TimeSeries ? static_cast<const T *>(data_)
                                                             : nullptr;
        } else if constexpr (is_same_v<T, TimeSeriesInstance>) {
            return kind_ == PrimitiveContentKind::TimeSeriesInstance ? static_cast<const T *>(data_)
                                                                     : nullptr;
        } else if constexpr (is_same_v<T, Schedule>) {
            return kind_ == PrimitiveContentKind::Schedule ? static_cast<const T *>(data_)
                                                           : nullptr;
        } else if constexpr (is_same_v<T, RequestResource>) {
            return kind_ == PrimitiveContentKind::RequestResource ? static_cast<const T *>(data_)
                                                                  : nullptr;
        } else if constexpr (is_same_v<T, PollingChannel>) {
            return kind_ == PrimitiveContentKind::PollingChannel ? static_cast<const T *>(data_)
                                                                 : nullptr;
        } else if constexpr (is_same_v<T, Node>) {
            return kind_ == PrimitiveContentKind::Node ? static_cast<const T *>(data_) : nullptr;
        } else if constexpr (is_same_v<T, FlexContainer>) {
            return kind_ == PrimitiveContentKind::FlexContainer ? static_cast<const T *>(data_)
                                                                : nullptr;
        } else {
            return nullptr;
        }
    }

    void Reset()
    {
        if (data_ == nullptr) {
            kind_ = PrimitiveContentKind::None;
            return;
        }

        switch (kind_) {
            case PrimitiveContentKind::AE:
                delete static_cast<AE *>(data_);
                break;
            case PrimitiveContentKind::Container:
                delete static_cast<Container *>(data_);
                break;
            case PrimitiveContentKind::ContentInstance:
                delete static_cast<ContentInstance *>(data_);
                break;
            case PrimitiveContentKind::Notification:
                delete static_cast<Notification *>(data_);
                break;
            case PrimitiveContentKind::Group:
                delete static_cast<Group *>(data_);
                break;
            case PrimitiveContentKind::Subscription:
                delete static_cast<Subscription *>(data_);
                break;
            case PrimitiveContentKind::AccessControlPolicy:
                delete static_cast<AccessControlPolicy *>(data_);
                break;
            case PrimitiveContentKind::CSEBase:
                delete static_cast<CSEBase *>(data_);
                break;
            case PrimitiveContentKind::RemoteCSE:
                delete static_cast<RemoteCSE *>(data_);
                break;
            case PrimitiveContentKind::MgmtCmd:
                delete static_cast<MgmtCmd *>(data_);
                break;
            case PrimitiveContentKind::ExecInstance:
                delete static_cast<ExecInstance *>(data_);
                break;
            case PrimitiveContentKind::TimeSeries:
                delete static_cast<TimeSeries *>(data_);
                break;
            case PrimitiveContentKind::TimeSeriesInstance:
                delete static_cast<TimeSeriesInstance *>(data_);
                break;
            case PrimitiveContentKind::Schedule:
                delete static_cast<Schedule *>(data_);
                break;
            case PrimitiveContentKind::RequestResource:
                delete static_cast<RequestResource *>(data_);
                break;
            case PrimitiveContentKind::PollingChannel:
                delete static_cast<PollingChannel *>(data_);
                break;
            case PrimitiveContentKind::Node:
                delete static_cast<Node *>(data_);
                break;
            case PrimitiveContentKind::FlexContainer:
                delete static_cast<FlexContainer *>(data_);
                break;
            case PrimitiveContentKind::None:
            default:
                break;
        }

        data_ = nullptr;
        kind_ = PrimitiveContentKind::None;
    }

private:
    template <typename T> PrimitiveContent &Set(const T &value, PrimitiveContentKind kind)
    {
        Reset();
        kind_ = kind;
        data_ = new T(value);
        return *this;
    }

    PrimitiveContentKind kind_{PrimitiveContentKind::None};
    void                *data_{nullptr};
};

// Request Primitive (rqp), Table 8.2.2-1
struct RequestPrimitive {
    // Mandatory
    Operation op;                // op
    CString   to;                // to
    CString   from;              // fr
    CString   requestIdentifier; // rqi

    // Optional, common
    Optional<ResourceType>                resourceType;                // ty
    PrimitiveContent                      content;                     // pc
    Optional<CString>                     originatingTimestamp;        // ot
    Optional<CString>                     requestExpirationTimestamp;  // rqet
    Optional<CString>                     resultExpirationTimestamp;   // rset
    Optional<CString>                     operationExecutionTime;      // oet
    Optional<ResponseTypeInfo>            responseType;                // rt
    Optional<CString>                     resultPersistence;           // rp
    Optional<ResultContent>               resultContent;               // rcn
    Optional<u8>                          eventCategory;               // ec
    Optional<bool>                        deliveryAggregation;         // da
    Optional<CString>                     groupRequestIdentifier;      // gid
    Optional<FilterCriteria>              filterCriteria;              // fc
    Optional<DesiredIdentifierResultType> desiredIdentifierResultType; // drt

    // Security / tokens
    Optional<CString> tokens;                // tkns
    Optional<CString> tokenIDs;              // tids
    Optional<bool>    tokenRequestIndicator; // tqi
    Optional<CString> localTokenIDs;         // ltids

    // Group operations
    Optional<Vector<CString>> groupRequestTargetMembers; // grtm
    Optional<s32>             groupSomecastTargetNumber; // gstn

    // Authorization
    Optional<bool>    authorSignIndicator; // asi
    Optional<CString> authorSigns;         // aus
    Optional<bool>    authorRelIndicator;  // auri

    // Semantic
    Optional<bool>    semanticQueryIndicator;   // sqi
    Optional<CString> ontologyMappingResources; // omr

    // Release / vendor
    Optional<CString> releaseVersionIndicator;    // rvi
    Optional<CString> vendorInformation;          // vsi
    Optional<CString> primitiveProfileIdentifier; // prpi
    Optional<CString> m2mServiceUser;             // msu
};

// Response Primitive (rsp), Table 8.2.2-1
struct ResponsePrimitive {
    ResponseStatusCode responseStatusCode; // rsc
    CString            to;                 // to
    CString            from;               // fr
    CString            requestIdentifier;  // rqi
    PrimitiveContent   content;            // pc

    Optional<CString> originatingTimestamp;      // ot
    Optional<CString> resultExpirationTimestamp; // rset
    Optional<u8>      eventCategory;             // ec
    Optional<CString> releaseVersionIndicator;   // rvi
    Optional<CString> vendorInformation;         // vsi
    Optional<CString> m2mServiceUser;            // msu

    // Token responses
    Optional<CString> assignedTokenIdentifiers; // ati
    Optional<CString> tokenRequestInformation;  // tqf

    // Content streaming
    Optional<u8>  contentStatus; // cnst
    Optional<s32> contentOffset; // cnot

    // Authorization
    Optional<CString> authorSignReqInfo; // asri
};

} // namespace zerom2m::onem2m::types
