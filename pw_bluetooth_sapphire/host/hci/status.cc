// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "status.h"

#include "util.h"

namespace btlib {
namespace common {

// static
std::string ProtocolErrorTraits<hci::StatusCode>::ToString(
    hci::StatusCode ecode) {
  return fxl::StringPrintf("%s (HCI %#.2x)", StatusCodeToString(ecode).c_str(),
                           static_cast<unsigned int>(ecode));
}

}  // namespace common

namespace hci {

using Base = common::Status<StatusCode>;

Status::Status(common::HostError ecode) : Base(ecode) {}

// HCI has a "success" status code which we specially handle while constructing
// a success Status.
Status::Status(hci::StatusCode proto_code)
    : Base(proto_code == StatusCode::kSuccess ? Base() : Base(proto_code)) {}

}  // namespace hci
}  // namespace btlib
