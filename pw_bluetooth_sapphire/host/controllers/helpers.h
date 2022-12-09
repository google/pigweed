// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_CONTROLLERS_HELPERS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_CONTROLLERS_HELPERS_H_

#include "pw_status/status.h"
#include "zircon/status.h"

namespace bt::controllers {

pw::Status ZxStatusToPwStatus(zx_status_t status);

}

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_CONTROLLERS_HELPERS_H_
