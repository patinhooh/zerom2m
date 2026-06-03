/*
 * json_codec.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

#include "json.h"

#include <zerom2m/serde/codec.h>

namespace zerom2m::serde::json
{

/**
 * @brief JsonCodec implements ICodec for JSON serialisation and deserialisation of OneM2M
 * resources and primitives.
 */
class JsonCodec final : public ICodec
{
public:
    static JsonCodec &Get()
    {
        static JsonCodec instance;
        return instance;
    }

    boolean DeserializeRequestBody(const CString &input, RequestPrimitive &output) const override;

    boolean DeserializeRequestPrimitive(const CString    &input,
                                        RequestPrimitive &output) const override;

    boolean DeserializeResponseBody(const CString &input, ResponsePrimitive &output) const override;

    boolean DeserializeResponsePrimitive(const CString     &input,
                                         ResponsePrimitive &output) const override;

    boolean SerializeResource(const ResourceBase &input, CString &output) const override;

    boolean SerializePrimitiveContent(const PrimitiveContent &input,
                                      CString                &output) const override;

    boolean SerializeResponsePrimitive(const ResponsePrimitive &input,
                                       CString                 &output) const override;

private:
    JsonCodec() = default;

    JsonValue *SerializePrimitiveContentValue(const PrimitiveContent &input) const;

    JsonValue *SerializeResourceBase(const ResourceBase &r) const;
    JsonValue *SerializeAE(const AE &r) const;
    JsonValue *SerializeContainer(const Container &r) const;
    JsonValue *SerializeContentInstance(const ContentInstance &r) const;
    JsonValue *SerializeNotification(const Notification &r) const;
    JsonValue *SerializeGroup(const Group &r) const;
    JsonValue *SerializeSubscription(const Subscription &r) const;
    JsonValue *SerializeCSEBase(const CSEBase &r) const;
    JsonValue *SerializeRemoteCSE(const RemoteCSE &r) const;
    JsonValue *SerializeNode(const Node &r) const;
    JsonValue *SerializePollingChannel(const PollingChannel &r) const;
    JsonValue *SerializeSchedule(const Schedule &r) const;
    JsonValue *SerializeMgmtCmd(const MgmtCmd &r) const;
    JsonValue *SerializeExecInstance(const ExecInstance &r) const;
    JsonValue *SerializeTimeSeries(const TimeSeries &r) const;
    JsonValue *SerializeTimeSeriesInstance(const TimeSeriesInstance &r) const;
    JsonValue *SerializeRequestResource(const RequestResource &r) const;
    JsonValue *SerializeFlexContainer(const FlexContainer &r) const;
    JsonValue *SerializeAccessControlPolicy(const AccessControlPolicy &r) const;

    boolean DeserializeAE(const JsonValue &root, RequestPrimitive &out) const;
    boolean DeserializeContainer(const JsonValue &root, RequestPrimitive &out) const;
    boolean DeserializeContentInstance(const JsonValue &root, RequestPrimitive &out) const;
    boolean DeserializeNotification(const JsonValue &root, RequestPrimitive &out) const;
    boolean DeserializeGroup(const JsonValue &root, RequestPrimitive &out) const;
    boolean DeserializeSubscription(const JsonValue &root, RequestPrimitive &out) const;
    boolean DeserializeTimeSeries(const JsonValue &root, RequestPrimitive &out) const;

    // Primitive-envelope helpers
    boolean ParsePrimitiveFields(const JsonValue &obj, RequestPrimitive &out) const;
    boolean ParseFilterCriteria(const JsonValue &fcObj, FilterCriteria &fc) const;
    boolean ParseEventNotificationCriteria(const JsonValue           &encObj,
                                           EventNotificationCriteria &enc) const;

    // Low-level helpers

    // Append a JSON string array from a Vector<CString>
    JsonValue *MakeStringArray(const Vector<CString> &v) const;

    // Extract a string field from a JSON object; returns empty CString if absent.
    CString GetString(const JsonValue &obj, const char *key) const;

    // Extract an optional string; sets opt only when key is present.
    void GetOptString(const JsonValue &obj, const char *key, Optional<CString> &opt) const;

    // Extract an optional bool.
    void GetOptBool(const JsonValue &obj, const char *key, Optional<boolean> &opt) const;

    // Extract an optional s32.
    void GetOptS32(const JsonValue &obj, const char *key, Optional<s32> &opt) const;

    // Extract an optional s64.
    void GetOptS64(const JsonValue &obj, const char *key, Optional<s64> &opt) const;

    // Populate a Vector<CString> from a JSON array of strings.
    void GetStringArray(const JsonValue &obj, const char *key, Vector<CString> &out) const;
};

} // namespace zerom2m::serde::json