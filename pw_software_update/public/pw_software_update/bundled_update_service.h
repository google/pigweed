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

#include "pw_software_update/bundled_update.rpc.pb.h"
#include "pw_software_update/update_manager.h"
#include "pw_status/status.h"

namespace pw::software_update {

// Implementation class for pw_software_update.BundledUpdate.
class BundledUpdateService
    : public generated::BundledUpdate<BundledUpdateService> {
 public:
  explicit constexpr BundledUpdateService(
      pw::software_update::BundledUpdateManager& manager)
      : manager_(manager) {}

  Status Abort(ServerContext&,
               const pw_protobuf_Empty& request,
               pw_software_update_OperationResult& response);

  Status SoftwareUpdateState(ServerContext&,
                             const pw_protobuf_Empty& request,
                             pw_software_update_OperationResult& response);

  void GetCurrentManifest(ServerContext&,
                          const pw_protobuf_Empty& request,
                          ServerWriter<pw_software_update_Manifest>& writer);

  Status VerifyCurrentManifest(ServerContext&,
                               const pw_protobuf_Empty& request,
                               pw_software_update_OperationResult& response);

  Status GetStagedManifest(ServerContext&,
                           const pw_protobuf_Empty& request,
                           pw_software_update_Manifest& response);

  Status PrepareForUpdate(ServerContext&,
                          const pw_protobuf_Empty& request,
                          pw_software_update_PrepareUpdateResult& response);

  Status VerifyAndApplyStagedBundle(
      ServerContext&,
      const pw_protobuf_Empty& request,
      pw_software_update_OperationResult& response);

  Status VerifyStagedBundle(ServerContext&,
                            const pw_protobuf_Empty& request,
                            pw_software_update_OperationResult& response);

  Status ApplyStagedBundle(ServerContext&,
                           const pw_protobuf_Empty& request,
                           pw_software_update_OperationResult& response);

 private:
  pw::software_update::BundledUpdateManager& manager_;
};

}  // namespace pw::software_update
