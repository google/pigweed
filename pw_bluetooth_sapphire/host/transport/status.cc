// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "status.h"

#include "src/connectivity/bluetooth/core/bt-host/hci-spec/util.h"

namespace bt {

// static
std::string ProtocolErrorTraits<hci_spec::StatusCode>::ToString(hci_spec::StatusCode ecode) {
  return fxl::StringPrintf("%s (HCI %#.2x)", hci_spec::StatusCodeToString(ecode).c_str(),
                           static_cast<unsigned int>(ecode));
}

namespace hci {

using Base = bt::Status<hci_spec::StatusCode>;

Status::Status(HostError ecode) : Base(ecode) {}

// HCI has a "success" status code which we specially handle while constructing
// a success Status.
Status::Status(hci_spec::StatusCode proto_code)
    : Base(proto_code == hci_spec::StatusCode::kSuccess ? Base() : Base(proto_code)) {}

}  // namespace hci
}  // namespace bt
