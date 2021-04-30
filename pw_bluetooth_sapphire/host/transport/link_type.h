// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_LINK_TYPE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_LINK_TYPE_H_

#include <string>

namespace bt {

// This defines the various connection types. These do not exactly correspond
// to the baseband logical/physical link types but instead provide a
// high-level abstraction.
enum class LinkType {
  // Represents a BR/EDR baseband link (ACL-U).
  kACL,

  // BR/EDR synchronous links (SCO-S, eSCO-S).
  kSCO,
  kESCO,

  // A LE logical link (LE-U).
  kLE,
};
std::string LinkTypeToString(LinkType type);

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_LINK_TYPE_H_
