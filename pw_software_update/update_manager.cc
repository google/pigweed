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

#include "pw_software_update/update_manager.h"

#include <string_view>

#include "pw_software_update/manifest_accessor.h"
#include "pw_software_update/update_bundle.pwpb.h"

namespace pw::software_update {

Status BundledUpdateManager::ApplyUpdate() {
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

  return backend_.FinalizeApply();
}

Result<uint32_t> BundledUpdateManager::GetTransferId() {
  if (!transfer_id_.has_value()) {
    Result<uint32_t> possible_transfer_id =
        backend_.EnableBundleTransferHandler();
    PW_TRY(possible_transfer_id.status());
    transfer_id_ = possible_transfer_id.value();
  }
  return transfer_id_.value();
}

pw::Status BundledUpdateManager::VerifyManifest() {
  return pw::Status::Unimplemented();
}

pw::Status BundledUpdateManager::WriteManifest() {
  return pw::Status::Unimplemented();
}

pw::Status BundledUpdateManager::BeforeUpdate() {
  return backend_.BeforeUpdateStart();
}

void BundledUpdateManager::DisableTransferId() {
  if (!transfer_id_.has_value()) {
    return;  // Nothing to do, already disabled.
  }
  backend_.DisableBundleTransferHandler();
}

pw::Status BundledUpdateManager::Abort() {
  DisableTransferId();
  PW_TRY(backend_.BeforeUpdateAbort());
  if (bundle_open_) {
    bundle_.Close();
  }
  return OkStatus();
}

pw::Status BundledUpdateManager::VerifyUpdate() {
  DisableTransferId();
  PW_TRY(backend_.BeforeBundleVerify());
  ManifestAccessor manifest;  // TODO(pwbug/456): Place-holder for now.
  PW_TRY(bundle_.OpenAndVerify(manifest));
  bundle_open_ = true;
  return backend_.AfterBundleVerified();
}

}  // namespace pw::software_update
