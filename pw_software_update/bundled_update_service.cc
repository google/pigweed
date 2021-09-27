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

#include "pw_software_update/config.h"

#define PW_LOG_LEVEL PW_SOFTWARE_UPDATE_CONFIG_LOG_LEVEL

#include <string_view>

#include "pw_log/log.h"
#include "pw_result/result.h"
#include "pw_software_update/bundled_update_service.h"
#include "pw_software_update/manifest_accessor.h"
#include "pw_software_update/update_bundle.pwpb.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace pw::software_update {

void BundledUpdateService::DisableTransferId() {
  if (!transfer_id_.has_value()) {
    return;  // Nothing to do, already disabled.
  }
  backend_.DisableBundleTransferHandler();
  transfer_id_.reset();
}

pw::Status BundledUpdateService::VerifyUpdate() {
  DisableTransferId();
  PW_TRY(backend_.BeforeBundleVerify());
  ManifestAccessor manifest;  // TODO(pwbug/456): Place-holder for now.
  PW_TRY(bundle_.OpenAndVerify(manifest));
  bundle_open_ = true;
  return backend_.AfterBundleVerified();
}

Status BundledUpdateService::ApplyUpdate() {
  // Note that the backend's FinalizeApply as part of DoApplyUpdate() shall be
  // configured such that this RPC can send out the reply before the device
  // reboots.
  return work_queue_.PushWork([this] {
    const Status status = this->DoApplyUpdate();
    if (!status.ok()) {
      PW_LOG_CRITICAL(
          "The target failed to apply the update; Currently this requires a "
          "manual device reboot to retry");
    }
  });
}

Status BundledUpdateService::DoApplyUpdate() {
  PW_LOG_DEBUG("Attempting to apply the update");
  protobuf::StringToMessageMap signed_targets_metadata_map =
      bundle_.GetDecoder().AsStringToMessageMap(static_cast<uint32_t>(
          pw::software_update::UpdateBundle::Fields::TARGETS_METADATA));
  if (const Status status = signed_targets_metadata_map.status();
      !status.ok()) {
    PW_LOG_ERROR("Update bundle does not contain the targets_metadata map: %d",
                 static_cast<int>(status.code()));
    return status;
  }

  // There should only be one element in the map, which is the top-level
  // targets metadata.
  constexpr std::string_view kTopLevelTargetsName = "targets";
  protobuf::Message signed_targets_metadata =
      signed_targets_metadata_map[kTopLevelTargetsName];
  if (const Status status = signed_targets_metadata.status(); !status.ok()) {
    PW_LOG_ERROR(
        "The targets_metadata map does not contain the targets entry: %d",
        static_cast<int>(status.code()));
    return status;
  }

  protobuf::Message targets_metadata = signed_targets_metadata.AsMessage(
      static_cast<uint32_t>(pw::software_update::SignedTargetsMetadata::Fields::
                                SERIALIZED_TARGETS_METADATA));
  if (const Status status = targets_metadata.status(); !status.ok()) {
    PW_LOG_ERROR(
        "The targets targets_metadata entry does not contain the "
        "serialized_target_metadata: %d",
        static_cast<int>(status.code()));
    return status;
  }

  protobuf::RepeatedMessages target_files =
      targets_metadata.AsRepeatedMessages(static_cast<uint32_t>(
          pw::software_update::TargetsMetadata::Fields::TARGET_FILES));
  if (const Status status = target_files.status(); !status.ok()) {
    PW_LOG_ERROR(
        "The serialized_target_metadata does not contain target_files: %d",
        static_cast<int>(status.code()));
    return status;
  }

  for (pw::protobuf::Message file_name : target_files) {
    // TODO: Use a config.h parameter for this.
    constexpr size_t kFileNameMaxSize = 32;
    std::array<std::byte, kFileNameMaxSize> buf = {};
    protobuf::String name = file_name.AsString(static_cast<uint32_t>(
        pw::software_update::TargetFile::Fields::FILE_NAME));
    PW_TRY(name.status());
    const Result<ByteSpan> read_result = name.GetBytesReader().Read(buf);
    PW_TRY(read_result.status());
    const ConstByteSpan file_name_span = read_result.value();
    const std::string_view file_name_view(
        reinterpret_cast<const char*>(file_name_span.data()),
        file_name_span.size_bytes());
    stream::IntervalReader file_reader =
        bundle_.GetTargetPayload(file_name_view);
    if (const Status status =
            backend_.ApplyTargetFile(file_name_view, file_reader);
        !status.ok()) {
      PW_LOG_ERROR("Failed to apply target file: %d",
                   static_cast<int>(status.code()));
      return status;
    }
  }

  // TODO(davidrogers): Ensure the backend documentation and API contract is
  // clear in regards to the flushing expectations for RPCs and logs surrounding
  // the reboot inside of this call.
  return backend_.FinalizeApply();
}

Status BundledUpdateService::Abort(ServerContext&,
                                   const pw_protobuf_Empty&,
                                   pw_software_update_OperationResult&) {
  if (state_ == pw_software_update_BundledUpdateState_State_APPLYING_UPDATE) {
    PW_LOG_WARN("Cannot abort an in-progress apply");
    return Status::Unavailable();
  }

  PW_TRY(backend_.BeforeUpdateAbort());
  DisableTransferId();
  if (bundle_open_) {
    bundle_.Close();
  }
  state_ = pw_software_update_BundledUpdateState_State_INACTIVE;
  return OkStatus();
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
  if (state_ != pw_software_update_BundledUpdateState_State_INACTIVE) {
    PW_LOG_WARN(
        "PrepareForUpdate can only be called from INACTIVE state, "
        "current state: %d. Abort must be called first.",
        state_);
    return Status::FailedPrecondition();
  }

  pw_software_update_OperationResult result;
  result.has_extended_status = false;
  result.has_state = true;
  pw_software_update_BundledUpdateState state;
  state_ = pw_software_update_BundledUpdateState_State_READY_FOR_UPDATE;
  state.manager_state = state_;
  result.state = state;
  response.has_result = true;
  response.result = result;
  response.has_transfer_id = true;

  if (!transfer_id_.has_value()) {
    Result<uint32_t> possible_transfer_id =
        backend_.EnableBundleTransferHandler();
    PW_TRY(possible_transfer_id.status());
    transfer_id_ = possible_transfer_id.value();
  }
  response.transfer_id = transfer_id_.value();

  return backend_.BeforeUpdateStart();
}

// TODO: dedupe this with VerifyStagedBundle.
Status BundledUpdateService::VerifyAndApplyStagedBundle(
    ServerContext&,
    const pw_protobuf_Empty&,
    pw_software_update_OperationResult& response) {
  // TODO(ewout): Fix this to be a proper precondition once we refactor the
  // state enum.
  if (state_ == pw_software_update_BundledUpdateState_State_APPLYING_UPDATE) {
    PW_LOG_WARN("Cannot verify and apply during an in-progress apply");
    return Status::Unavailable();
  }

  PW_TRY(VerifyUpdate());
  state_ =
      pw_software_update_BundledUpdateState_State_VERIFIED_AND_READY_TO_APPLY;

  // TODO: upstream verification logic
  response.has_extended_status = false;
  response.has_state = true;
  pw_software_update_BundledUpdateState state;
  state_ = pw_software_update_BundledUpdateState_State_APPLYING_UPDATE;
  state.manager_state = state_;
  response.state = state;
  // TODO: defer this handling to the work queue so we can respond here.
  return ApplyUpdate();
}

// TODO: Consider a config.h parameter to require VerifyAndApplyStagedBundle.
Status BundledUpdateService::VerifyStagedBundle(
    ServerContext&,
    const pw_protobuf_Empty&,
    pw_software_update_OperationResult& response) {
  // TODO(ewout): Fix this to be a proper precondition once we refactor the
  // state enum.
  if (state_ == pw_software_update_BundledUpdateState_State_APPLYING_UPDATE) {
    PW_LOG_WARN("Cannot verify during an in-progress apply");
    return Status::Unavailable();
  }

  // TODO: upstream verification logic
  response.has_extended_status = false;
  response.has_state = true;
  pw_software_update_BundledUpdateState state;
  state_ =
      pw_software_update_BundledUpdateState_State_VERIFIED_AND_READY_TO_APPLY;
  state.manager_state = state_;
  response.state = state;
  return VerifyUpdate();
}

// TODO: Consider a config.h parameter to require VerifyAndApplyStagedBundle.
Status BundledUpdateService::ApplyStagedBundle(
    ServerContext&,
    const pw_protobuf_Empty&,
    pw_software_update_OperationResult& response) {
  if (state_ !=
      pw_software_update_BundledUpdateState_State_VERIFIED_AND_READY_TO_APPLY) {
    return Status::FailedPrecondition();
  }

  // TODO: upstream verification logic
  response.has_extended_status = false;
  response.has_state = true;
  pw_software_update_BundledUpdateState state;
  state_ = pw_software_update_BundledUpdateState_State_APPLYING_UPDATE;
  state.manager_state = state_;
  response.state = state;

  // TODO: Add a mechanism to restore the state if apply failed with a proper
  // threadsafe state machine including status checking/reporting.
  return ApplyUpdate();
}

}  // namespace pw::software_update
