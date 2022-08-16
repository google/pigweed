// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_TRACE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_TRACE_H_

#include <cstdint>

#ifndef NTRACE
#include <lib/trace/event.h>
#else
typedef uint64_t trace_flow_id_t;
#define TRACE_NONCE() (0u)
#define TRACE_ENABLED() (false)
#define TRACE_FLOW_BEGIN(...)
#define TRACE_FLOW_END(...)
#define TRACE_DURATION(...)
#endif  // NTRACE

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_TRACE_H_
