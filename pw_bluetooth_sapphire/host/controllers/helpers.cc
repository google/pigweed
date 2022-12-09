// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "helpers.h"

namespace bt::controllers {

pw::Status ZxStatusToPwStatus(zx_status_t status) {
  switch (status) {
    case ZX_OK:
      return PW_STATUS_OK;
    case ZX_ERR_INVALID_ARGS:
      return pw::Status::InvalidArgument();
    case ZX_ERR_ALREADY_EXISTS:
      return pw::Status::AlreadyExists();
    case ZX_ERR_PEER_CLOSED:
      return pw::Status::Unavailable();
    case ZX_ERR_NOT_SUPPORTED:
      return pw::Status::Unimplemented();
    default:
      return pw::Status::Unknown();
  }
}

}  // namespace bt::controllers
