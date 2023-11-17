// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/gap/types.h"

namespace bt::gap {

bool SecurityPropertiesMeetRequirements(
    sm::SecurityProperties properties, BrEdrSecurityRequirements requirements) {
  bool auth_ok = !requirements.authentication || properties.authenticated();
  bool sc_ok =
      !requirements.secure_connections || properties.secure_connections();
  return auth_ok && sc_ok;
}

}  // namespace bt::gap
