// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_bluetooth_sapphire/fuchsia/host/controllers/helpers.h"

namespace bt::controllers {

pw::Status ZxStatusToPwStatus(zx_status_t status) {
  switch (status) {
    case ZX_OK:
      return PW_STATUS_OK;
    case ZX_ERR_INTERNAL:
      return pw::Status::Internal();
    case ZX_ERR_NOT_SUPPORTED:
      return pw::Status::Unimplemented();
    case ZX_ERR_NO_RESOURCES:
      return pw::Status::ResourceExhausted();
    case ZX_ERR_INVALID_ARGS:
      return pw::Status::InvalidArgument();
    case ZX_ERR_OUT_OF_RANGE:
      return pw::Status::OutOfRange();
    case ZX_ERR_CANCELED:
      return pw::Status::Cancelled();
    case ZX_ERR_PEER_CLOSED:
      return pw::Status::Unavailable();
    case ZX_ERR_NOT_FOUND:
      return pw::Status::NotFound();
    case ZX_ERR_ALREADY_EXISTS:
      return pw::Status::AlreadyExists();
    case ZX_ERR_UNAVAILABLE:
      return pw::Status::Unavailable();
    case ZX_ERR_ACCESS_DENIED:
      return pw::Status::PermissionDenied();
    case ZX_ERR_IO_DATA_LOSS:
      return pw::Status::DataLoss();
    default:
      return pw::Status::Unknown();
  }
}

}  // namespace bt::controllers
