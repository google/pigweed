// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_HCI_DEFS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_HCI_DEFS_H_

#include <cstdint>

namespace bt::hci {

// Indicates the priority of ACL data on a link. Corresponds to priorities in the ACL priority
// vendor command.
enum class AclPriority : uint8_t {
  // Default. Do not prioritize data.
  kNormal,
  // Prioritize receiving data in the inbound direction.
  kSink,
  // Prioritize sending data in the outbound direction.
  kSource,
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_HCI_DEFS_H_
