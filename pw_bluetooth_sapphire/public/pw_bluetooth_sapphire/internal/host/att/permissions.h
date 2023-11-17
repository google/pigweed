// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_PERMISSIONS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_PERMISSIONS_H_

#include "pw_bluetooth_sapphire/internal/host/att/attribute.h"
#include "pw_bluetooth_sapphire/internal/host/sm/types.h"

namespace bt::att {

fit::result<ErrorCode> CheckReadPermissions(const AccessRequirements&,
                                            const sm::SecurityProperties&);
fit::result<ErrorCode> CheckWritePermissions(const AccessRequirements&,
                                             const sm::SecurityProperties&);

}  // namespace bt::att

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_PERMISSIONS_H_
