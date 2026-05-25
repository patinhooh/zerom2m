/*
 * headers.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

// HTTP header name constants  (TS-0009 Table 6.2.2-1)

namespace zerom2m::onem2m::bindings::http
{
// Request + Response
constexpr const char *ORIGIN          = "X-M2M-Origin"; // fr
constexpr const char *REQUEST_ID      = "X-M2M-RI";     // rqi
constexpr const char *RELEASE_VERSION = "X-M2M-RVI";    // rvi
constexpr const char *ORIG_TIMESTAMP  = "X-M2M-OT";     // ot
constexpr const char *RSC             = "X-M2M-RSC";    // rsc  (response only)
constexpr const char *VENDOR_INFO     = "X-M2M-VSI";    // vsi
constexpr const char *SERVICE_USER    = "X-M2M-MSU";    // msu

// Request only
constexpr const char *RESOURCE_TYPE  = "X-M2M-TY";   // ty   (Create only)
constexpr const char *EVENT_CATEGORY = "X-M2M-EC";   // ec
constexpr const char *REQ_EXP_TS     = "X-M2M-RQET"; // rqet
constexpr const char *RES_EXP_TS     = "X-M2M-RSET"; // rset
constexpr const char *OP_EXEC_TIME   = "X-M2M-OET";  // oet
constexpr const char *RESPONSE_TYPE  = "X-M2M-RT";   // rt
constexpr const char *RESULT_PERSIST = "X-M2M-RP";   // rp
constexpr const char *RESULT_CONTENT = "X-M2M-RCN";  // rcn
constexpr const char *DELIV_AGGR     = "X-M2M-DA";   // da
constexpr const char *GROUP_REQ_ID   = "X-M2M-GID";  // gid
constexpr const char *PRIM_PROFILE   = "X-M2M-PRPI"; // prpi
constexpr const char *TOKENS         = "X-M2M-TKNS"; // tkns
constexpr const char *TOKEN_IDS      = "X-M2M-TIDS"; // tids
constexpr const char *AUTH_SIGN_IND  = "X-M2M-ASI";  // asi
constexpr const char *AUTH_SIGNS     = "X-M2M-AUS";  // aus
constexpr const char *SEM_QUERY_IND  = "X-M2M-SQI";  // sqi

// Response only
constexpr const char *CONTENT_LOCATION = "Content-Location"; // URI of created resource
constexpr const char *ASSIGNED_TOKENS  = "X-M2M-ATI";        // ati
constexpr const char *TOKEN_REQ_INFO   = "X-M2M-TQF";        // tqf
constexpr const char *CONTENT_STATUS   = "X-M2M-CNST";       // cnst
constexpr const char *CONTENT_OFFSET   = "X-M2M-CNOT";       // cnot

// Standard
constexpr const char *CONTENT_TYPE = "Content-Type";
constexpr const char *ACCEPT       = "Accept";
} // namespace zerom2m::onem2m::bindings::http