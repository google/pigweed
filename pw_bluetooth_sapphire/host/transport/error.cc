// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/transport/error.h"

#include "pw_bluetooth_sapphire/internal/host/hci-spec/util.h"

namespace bt {

std::string ProtocolErrorTraits<pw::bluetooth::emboss::StatusCode>::ToString(
    pw::bluetooth::emboss::StatusCode ecode) {
  return bt_lib_cpp_string::StringPrintf(
      "%s (HCI %#.2x)",
      hci_spec::StatusCodeToString(ecode).c_str(),
      static_cast<unsigned int>(ecode));
}

}  // namespace bt
