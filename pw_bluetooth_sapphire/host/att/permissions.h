// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_PERMISSIONS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_PERMISSIONS_H_

#include "src/connectivity/bluetooth/core/bt-host/att/attribute.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"

namespace bt {
namespace att {

ErrorCode CheckReadPermissions(const AccessRequirements&,
                               const sm::SecurityProperties&);
ErrorCode CheckWritePermissions(const AccessRequirements&,
                                const sm::SecurityProperties&);

}  // namespace att
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_PERMISSIONS_H_
