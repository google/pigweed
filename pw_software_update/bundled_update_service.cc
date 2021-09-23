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

#include "pw_software_update/bundled_update_service.h"

#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace pw::software_update {

Status BundledUpdateService::Abort(ServerContext&,
                                   const pw_protobuf_Empty&,
                                   pw_software_update_OperationResult&) {
  return manager_.Abort();
}

Status BundledUpdateService::SoftwareUpdateState(
    ServerContext&,
    const pw_protobuf_Empty&,
    pw_software_update_OperationResult&) {
  return Status::Unimplemented();
}

void BundledUpdateService::GetCurrentManifest(
    ServerContext&,
    const pw_protobuf_Empty&,
    ServerWriter<pw_software_update_Manifest>& writer) {
  writer.Finish(Status::Unimplemented());
}

Status BundledUpdateService::VerifyCurrentManifest(
    ServerContext&,
    const pw_protobuf_Empty&,
    pw_software_update_OperationResult&) {
  return Status::Unimplemented();
}

Status BundledUpdateService::GetStagedManifest(ServerContext&,
                                               const pw_protobuf_Empty&,
                                               pw_software_update_Manifest&) {
  return Status::Unimplemented();
}

Status BundledUpdateService::PrepareForUpdate(
    ServerContext&,
    [[maybe_unused]] const pw_protobuf_Empty& request,
    pw_software_update_PrepareUpdateResult& response) {
  pw_software_update_OperationResult result;
  result.has_extended_status = false;
  result.has_state = true;
  pw_software_update_BundledUpdateState state;
  state.manager_state =
      pw_software_update_BundledUpdateState_State_READY_FOR_UPDATE;
  result.state = state;
  response.has_result = true;
  response.result = result;
  response.has_transfer_id = true;
  const pw::Result<uint32_t> possible_transfer_id = manager_.GetTransferId();
  PW_TRY(possible_transfer_id.status());
  response.transfer_id = possible_transfer_id.value();
  return manager_.BeforeUpdate();
}

// TODO: dedupe this with VerifyStagedBundle.
Status BundledUpdateService::VerifyAndApplyStagedBundle(
    ServerContext&,
    const pw_protobuf_Empty&,
    pw_software_update_OperationResult& response) {
  // TODO: upstream verification logic
  response.has_extended_status = false;
  response.has_state = true;
  pw_software_update_BundledUpdateState state;
  state.manager_state =
      pw_software_update_BundledUpdateState_State_VERIFIED_AND_READY_TO_APPLY;
  response.state = state;
  // TODO: defer this handling to the work queue so we can respond here.
  PW_TRY(manager_.VerifyUpdate());
  return manager_.ApplyUpdate();
}

Status BundledUpdateService::VerifyStagedBundle(
    ServerContext&,
    const pw_protobuf_Empty&,
    pw_software_update_OperationResult& response) {
  // TODO: upstream verification logic
  response.has_extended_status = false;
  response.has_state = true;
  pw_software_update_BundledUpdateState state;
  state.manager_state =
      pw_software_update_BundledUpdateState_State_VERIFIED_AND_READY_TO_APPLY;
  response.state = state;
  // TODO: defer this handling to the work queue so we can respond here.
  return manager_.VerifyUpdate();
}

Status BundledUpdateService::ApplyStagedBundle(
    ServerContext&,
    const pw_protobuf_Empty&,
    pw_software_update_OperationResult&) {
  // TODO: defer this handling to the work queue so we can respond here.
  return manager_.ApplyUpdate();
}

}  // namespace pw::software_update
