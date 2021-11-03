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

#include <cstddef>
#include <string_view>

#include "pw_log/log.h"
#include "pw_protobuf/message.h"
#include "pw_result/result.h"
#include "pw_software_update/update_bundle.pwpb.h"
#include "pw_software_update/update_bundle_accessor.h"
#include "pw_stream/interval_reader.h"
#include "pw_stream/memory_stream.h"

namespace pw::software_update {
namespace {

constexpr std::string_view kTopLevelTargetsName = "targets";

}

Status UpdateBundleAccessor::OpenAndVerify(const ManifestAccessor&) {
  PW_TRY(DoOpen());
  PW_TRY(DoVerify());
  return OkStatus();
}

// Get the target element corresponding to `target_file`
stream::IntervalReader UpdateBundleAccessor::GetTargetPayload(
    std::string_view target_file_name) {
  protobuf::StringToBytesMap target_payloads =
      decoder_.AsStringToBytesMap(static_cast<uint32_t>(
          pw::software_update::UpdateBundle::Fields::TARGET_PAYLOADS));
  PW_TRY(target_payloads.status());
  protobuf::Bytes payload = target_payloads[target_file_name];
  PW_TRY(payload.status());
  return payload.GetBytesReader();
}

Result<bool> UpdateBundleAccessor::IsTargetPayloadIncluded(
    std::string_view target_file_name) {
  // TODO(pwbug/456): Perform personalization check first. If the target
  // is personalized out. Don't need to proceed.

  protobuf::StringToMessageMap signed_targets_metadata_map =
      decoder_.AsStringToMessageMap(static_cast<uint32_t>(
          pw::software_update::UpdateBundle::Fields::TARGETS_METADATA));
  PW_TRY(signed_targets_metadata_map.status());

  // There should only be one element in the map, which is the top-level
  // targets metadata.
  protobuf::Message signed_targets_metadata =
      signed_targets_metadata_map[kTopLevelTargetsName];
  PW_TRY(signed_targets_metadata.status());

  protobuf::Message metadata = signed_targets_metadata.AsMessage(
      static_cast<uint32_t>(pw::software_update::SignedTargetsMetadata::Fields::
                                SERIALIZED_TARGETS_METADATA));
  PW_TRY(metadata.status());

  protobuf::RepeatedMessages target_files =
      metadata.AsRepeatedMessages(static_cast<uint32_t>(
          pw::software_update::TargetsMetadata::Fields::TARGET_FILES));
  PW_TRY(target_files.status());

  for (protobuf::Message target_file : target_files) {
    protobuf::String name = target_file.AsString(static_cast<uint32_t>(
        pw::software_update::TargetFile::Fields::FILE_NAME));
    PW_TRY(name.status());
    Result<bool> file_name_matches = name.Equal(target_file_name);
    PW_TRY(file_name_matches.status());
    if (file_name_matches.value()) {
      return true;
    }
  }

  return false;
}

Status UpdateBundleAccessor::WriteManifest(
    [[maybe_unused]] stream::Writer& staged_manifest_writer) {
  return Status::Unimplemented();
}

Status UpdateBundleAccessor::Close() {
  // TODO(pwbug/456): To be implemented.
  return bundle_reader_.Close();
}

Status UpdateBundleAccessor::DoOpen() {
  PW_TRY(bundle_.Init());
  PW_TRY(bundle_reader_.Open());
  decoder_ =
      protobuf::Message(bundle_reader_, bundle_reader_.ConservativeReadLimit());
  (void)backend_;
  return OkStatus();
}

Status UpdateBundleAccessor::DoVerify() {
  if (disable_verification_) {
    PW_LOG_WARN("Update bundle verification is disabled.");
    return OkStatus();
  }

  // Verify and upgrade the on-device trust to the incoming root metadata if
  // one is included.
  PW_TRY(DoUpgradeRoot());

  // TODO(pwbug/456): Verify the targets metadata against the current trusted
  // root.

  // TODO(pwbug/456): Investigate whether targets payload verification should
  // be performed here or deferred until a specific target is requested.

  // TODO(pwbug/456): Invoke the backend to do downstream verification of the
  // bundle (e.g. compatibility and manifest completeness checks).

  return OkStatus();
}

Status UpdateBundleAccessor::DoUpgradeRoot() {
  // TODO(pwbug/456): Check whether the bundle contains a root metadata that
  // is different from the on-device trusted root.

  // TODO(pwbug/456) Verify the signatures against the trusted root metadata.

  // TODO(pwbug/456): Verifiy the content of the new root metadata, including:
  //    1) Check role magic field.
  //    2) Check signature requirement. Specifically, check that no key is
  //       reused across different roles and keys are unique in the same
  //       requirement.
  //    3) Check key mapping. Specifically, check that all keys are unique,
  //       ECDSA keys, and the key ids are exactly the SHA256 of `key type +
  //       key scheme + key value`.

  // TODO(pwbug/456). Verify the signatures against the new root metadata.

  // TODO(pwbug/456): Check rollback.

  // TODO(pwbug/456): Persist new root.

  // TODO(pwbug/456): Implement key change detection to determine whether
  // rotation has occured or not. Delete the persisted targets metadata version
  // if any of the targets keys has been rotated.

  return OkStatus();
}

}  // namespace pw::software_update
