// Copyright 2021 The Pigweed Authors
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

#pragma once

#include "pw_software_update/bundled_update_backend.h"
#include "pw_software_update/update_bundle_accessor.h"

namespace pw::software_update {

class BundledUpdateManager {
 public:
  constexpr BundledUpdateManager(UpdateBundleAccessor& bundle,
                                 BundledUpdateBackend& backend)

      : backend_(backend), bundle_(bundle), bundle_open_(false) {}

  pw::Status Abort();

  // Will enable the transfer_id if needed via the BundledUpdateBackend.
  Result<uint32_t> GetTransferId();

  pw::Status VerifyManifest();

  pw::Status WriteManifest();

  pw::Status ApplyUpdate();

  pw::Status BeforeUpdate();

  pw::Status VerifyUpdate();

 private:
  BundledUpdateBackend& backend_;
  UpdateBundleAccessor& bundle_;

  std::optional<uint32_t> transfer_id_;
  bool bundle_open_;

  // Will disable the transfer_id if needed via the BundledUpdateBackend.
  void DisableTransferId();
};

}  // namespace pw::software_update
