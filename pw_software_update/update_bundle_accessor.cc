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
  PW_TRY(bundle_.Init());
  PW_TRY(bundle_reader_.Open());
  decoder_ =
      protobuf::Message(bundle_reader_, bundle_reader_.ConservativeReadLimit());
  (void)backend_;
  // TODO(pw_bug/456): Implement verification logic.
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
    stream::Writer& staged_manifest_writer) {
  protobuf::StringToMessageMap signed_targets_metadata_map =
      decoder_.AsStringToMessageMap(static_cast<uint32_t>(
          pw::software_update::UpdateBundle::Fields::TARGETS_METADATA));
  PW_TRY(signed_targets_metadata_map.status());

  // There should only be one element in the map, which is the top-level
  // targets metadata.
  protobuf::Message signed_targets_metadata =
      signed_targets_metadata_map[kTopLevelTargetsName];
  PW_TRY(signed_targets_metadata.status());

  protobuf::Bytes metadata = signed_targets_metadata.AsBytes(
      static_cast<uint32_t>(pw::software_update::SignedTargetsMetadata::Fields::
                                SERIALIZED_TARGETS_METADATA));
  PW_TRY(metadata.status());

  stream::MemoryReader name_reader(
      std::as_bytes(std::span(kTopLevelTargetsName)));
  stream::IntervalReader metadata_reader = metadata.GetBytesReader();

  std::byte stream_pipe_buffer[WRITE_MANIFEST_STREAM_PIPE_BUFFER_SIZE];
  return protobuf::WriteProtoStringToBytesMapEntry(
      static_cast<uint32_t>(
          pw::software_update::Manifest::Fields::TARGETS_METADATA),
      name_reader,
      kTopLevelTargetsName.size(),
      metadata_reader,
      metadata_reader.interval_size(),
      stream_pipe_buffer,
      staged_manifest_writer);
}

Status UpdateBundleAccessor::Close() {
  // TODO(pwbug/456): To be implemented.
  return bundle_reader_.Close();
}

}  // namespace pw::software_update
