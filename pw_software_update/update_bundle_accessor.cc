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

#include "pw_software_update/update_bundle_accessor.h"

#include <cstddef>
#include <cstring>
#include <string_view>

#include "pw_crypto/ecdsa.h"
#include "pw_crypto/sha256.h"
#include "pw_log/log.h"
#include "pw_protobuf/message.h"
#include "pw_result/result.h"
#include "pw_software_update/config.h"
#include "pw_software_update/update_bundle.pwpb.h"
#include "pw_stream/interval_reader.h"
#include "pw_stream/memory_stream.h"

#define PW_LOG_LEVEL PW_SOFTWARE_UPDATE_CONFIG_LOG_LEVEL

namespace pw::software_update {
namespace {

constexpr std::string_view kTopLevelTargetsName = "targets";

Result<bool> VerifyEcdsaSignature(protobuf::Bytes public_key,
                                  ConstByteSpan digest,
                                  protobuf::Bytes signature) {
  // TODO(pwbug/456): Move this logic into an variant of the API in
  // pw_crypto:ecdsa that takes readers as inputs.
  std::byte public_key_bytes[65];
  std::byte signature_bytes[64];
  stream::IntervalReader key_reader = public_key.GetBytesReader();
  stream::IntervalReader sig_reader = signature.GetBytesReader();
  PW_TRY(key_reader.Read(public_key_bytes));
  PW_TRY(sig_reader.Read(signature_bytes));
  Status status = crypto::ecdsa::VerifyP256Signature(
      public_key_bytes, digest, signature_bytes);
  if (!status.ok()) {
    return false;
  }

  return true;
}

// Convert an integer from [0, 16) to a hex char
char IntToHex(uint8_t val) {
  PW_ASSERT(val < 16);
  return val >= 10 ? (val - 10) + 'a' : val + '0';
}

void LogKeyId(ConstByteSpan key_id) {
  char key_id_str[pw::crypto::sha256::kDigestSizeBytes * 2 + 1] = {0};
  for (size_t i = 0; i < pw::crypto::sha256::kDigestSizeBytes; i++) {
    uint8_t value = std::to_integer<uint8_t>(key_id[i]);
    key_id_str[i * 2] = IntToHex((value >> 4) & 0xf);
    key_id_str[i * 2 + 1] = IntToHex(value & 0xf);
  }

  PW_LOG_DEBUG("key_id: %s", key_id_str);
}

// Verifies signatures of a TUF metadata.
Result<bool> VerifyMetadataSignatures(
    protobuf::Bytes message,
    protobuf::RepeatedMessages signatures,
    protobuf::Message signature_requirement,
    protobuf::StringToMessageMap key_mapping) {
  // Gets the threshold -- at least `threshold` number of signatures must
  // pass verification in order to trust this metadata.
  protobuf::Uint32 threshold = signature_requirement.AsUint32(
      static_cast<uint32_t>(SignatureRequirement::Fields::THRESHOLD));
  PW_TRY(threshold.status());

  // Gets the ids of keys that are allowed for verifying the signatures.
  protobuf::RepeatedBytes allowed_key_ids =
      signature_requirement.AsRepeatedBytes(
          static_cast<uint32_t>(SignatureRequirement::Fields::KEY_IDS));
  PW_TRY(allowed_key_ids.status());

  // Verifies the signatures. Check that at least `threshold` number of
  // signatures can be verified using the allowed keys.
  size_t verified_count = 0;
  for (protobuf::Message signature : signatures) {
    protobuf::Bytes key_id =
        signature.AsBytes(static_cast<uint32_t>(Signature::Fields::KEY_ID));
    PW_TRY(key_id.status());

    // Reads the key id into a buffer, so that we can check whether it is
    // listed as allowed and look up the key value later.
    std::byte key_id_buf[pw::crypto::sha256::kDigestSizeBytes];
    stream::IntervalReader key_id_reader = key_id.GetBytesReader();
    Result<ByteSpan> key_id_read_res = key_id_reader.Read(key_id_buf);
    PW_TRY(key_id_read_res.status());
    if (key_id_read_res.value().size() != sizeof(key_id_buf)) {
      return Status::Internal();
    }

    // Verify that the `key_id` is listed in `allowed_key_ids`.
    // Note that the function assumes that the key id is properly derived
    // from the key (via sha256).
    bool key_id_is_allowed = false;
    for (protobuf::Bytes trusted : allowed_key_ids) {
      Result<bool> key_id_equal = trusted.Equal(key_id_buf);
      PW_TRY(key_id_equal.status());
      if (key_id_equal.value()) {
        key_id_is_allowed = true;
        break;
      }
    }

    if (!key_id_is_allowed) {
      PW_LOG_DEBUG("Skipping a key id not listed in allowed key ids.");
      LogKeyId(key_id_buf);
      continue;
    }

    // Retrieves the signature bytes.
    protobuf::Bytes sig =
        signature.AsBytes(static_cast<uint32_t>(Signature::Fields::SIG));
    PW_TRY(sig.status());

    // Extracts the key type, scheme and value information.
    std::string_view key_id_str(reinterpret_cast<const char*>(key_id_buf),
                                sizeof(key_id_buf));
    protobuf::Message key_info = key_mapping[key_id_str];
    PW_TRY(key_info.status());

    protobuf::Bytes key_val =
        key_info.AsBytes(static_cast<uint32_t>(Key::Fields::KEYVAL));
    PW_TRY(key_val.status());

    // The function assume that all keys are ECDSA keys. This is guaranteed
    // by the fact that all trusted roots have undergone content check.

    // computes the sha256 hash
    std::byte sha256_digest[32];
    stream::IntervalReader bytes_reader = message.GetBytesReader();
    PW_TRY(crypto::sha256::Hash(bytes_reader, sha256_digest));
    Result<bool> res = VerifyEcdsaSignature(key_val, sha256_digest, sig);
    PW_TRY(res.status());
    if (res.value()) {
      verified_count++;
      if (verified_count == threshold.value()) {
        return true;
      }
    }
  }

  PW_LOG_DEBUG(
      "Not enough number of signatures verified. Requires at least %u, "
      "verified %u",
      threshold.value(),
      verified_count);
  return false;
}

// Verifies the signatures of a signed new root metadata against a given
// trusted root. The helper function extracts the corresponding key maping
// signature requirement, signatures from the trusted root and passes them
// to VerifyMetadataSignatures().
//
// Precondition: The trusted root metadata has undergone content validity check.
Result<bool> VerifyRootMetadataSignatures(protobuf::Message trusted_root,
                                          protobuf::Message new_root) {
  // Retrieves the trusted root metadata content message.
  protobuf::Message trusted = trusted_root.AsMessage(static_cast<uint32_t>(
      SignedRootMetadata::Fields::SERIALIZED_ROOT_METADATA));
  PW_TRY(trusted.status());

  // Retrieves the serialized new root metadata bytes.
  protobuf::Bytes serialized = new_root.AsBytes(static_cast<uint32_t>(
      SignedRootMetadata::Fields::SERIALIZED_ROOT_METADATA));
  PW_TRY(serialized.status());

  // Gets the key mapping from the trusted root metadata.
  protobuf::StringToMessageMap key_mapping = trusted.AsStringToMessageMap(
      static_cast<uint32_t>(RootMetadata::Fields::KEYS));
  PW_TRY(key_mapping.status());

  // Gets the signatures of the new root.
  protobuf::RepeatedMessages signatures = new_root.AsRepeatedMessages(
      static_cast<uint32_t>(SignedRootMetadata::Fields::SIGNATURES));
  PW_TRY(signatures.status());

  // Gets the signature requirement from the trusted root metadata.
  protobuf::Message signature_requirement = trusted.AsMessage(
      static_cast<uint32_t>(RootMetadata::Fields::ROOT_SIGNATURE_REQUIREMENT));
  PW_TRY(signature_requirement.status());

  // Verifies the signatures.
  return VerifyMetadataSignatures(
      serialized, signatures, signature_requirement, key_mapping);
}

Result<uint32_t> GetMetadataVersion(protobuf::Message& metadata,
                                    uint32_t common_metatdata_field_number) {
  // message [Root|Targets]Metadata {
  //   ...
  //   CommonMetadata common_metadata = <field_number>;
  //   ...
  // }
  //
  // message CommonMetadata {
  //   ...
  //   uint32 version = <field_number>;
  //   ...
  // }
  protobuf::Message common_metadata =
      metadata.AsMessage(common_metatdata_field_number);
  PW_TRY(common_metadata.status());
  protobuf::Uint32 res = common_metadata.AsUint32(
      static_cast<uint32_t>(software_update::CommonMetadata::Fields::VERSION));
  PW_TRY(res.status());
  return res.value();
}

// Gets the list of targets in the top-level targets metadata
protobuf::RepeatedMessages GetTopLevelTargets(protobuf::Message bundle) {
  // Get signed targets metadata map.
  //
  // message UpdateBundle {
  //   ...
  //   map<string, SignedTargetsMetadata> target_metadata = <id>;
  //   ...
  // }
  protobuf::StringToMessageMap signed_targets_metadata_map =
      bundle.AsStringToMessageMap(
          static_cast<uint32_t>(UpdateBundle::Fields::TARGETS_METADATA));
  PW_TRY(signed_targets_metadata_map.status());

  // Get the top-level signed targets metadata.
  protobuf::Message signed_targets_metadata =
      signed_targets_metadata_map[kTopLevelTargetsName];
  PW_TRY(signed_targets_metadata.status());

  // Get the targets metadata.
  //
  // message SignedTargetsMetadata {
  //   ...
  //   bytes serialized_target_metadata = <id>;
  //   ...
  // }
  protobuf::Message targets_metadata =
      signed_targets_metadata.AsMessage(static_cast<uint32_t>(
          SignedTargetsMetadata::Fields::SERIALIZED_TARGETS_METADATA));
  PW_TRY(targets_metadata.status());

  // Return the target file list
  //
  // message TargetsMetadata {
  //   ...
  //   repeated TargetFile target_files = <id>;
  //   ...
  // }
  return targets_metadata.AsRepeatedMessages(
      static_cast<uint32_t>(TargetsMetadata::Fields::TARGET_FILES));
}

// Verifies a given target payload against a given hash.
Result<bool> VerifyTargetPayloadHash(protobuf::Message hash_info,
                                     protobuf::Bytes target_payload) {
  // Get the hash function field
  //
  // message Hash {
  //   ...
  //   HashFunction function = <id>;
  //   ...
  // }
  protobuf::Uint32 hash_function =
      hash_info.AsUint32(static_cast<uint32_t>(Hash::Fields::FUNCTION));
  PW_TRY(hash_function.status());

  // enum HashFunction {
  //   UNKNOWN_HASH_FUNCTION = 0;
  //   SHA256 = 1;
  // }
  if (hash_function.value() != static_cast<uint32_t>(HashFunction::SHA256)) {
    // Unknown hash function
    PW_LOG_DEBUG("Unknown hash function, %d", hash_function.value());
    return Status::InvalidArgument();
  }

  // Get the hash bytes field
  //
  // message Hash {
  //   ...
  //   bytes hash = <id>;
  //   ...
  // }
  protobuf::Bytes hash_bytes =
      hash_info.AsBytes(static_cast<uint32_t>(Hash::Fields::HASH));
  PW_TRY(hash_bytes.status());

  std::byte digest[crypto::sha256::kDigestSizeBytes];
  stream::IntervalReader target_payload_reader =
      target_payload.GetBytesReader();
  PW_TRY(crypto::sha256::Hash(target_payload_reader, digest));
  return hash_bytes.Equal(digest);
}

// Reads a protobuf::String into a buffer and returns a std::string_view.
Result<std::string_view> ReadProtoString(protobuf::String str,
                                         std::span<char> buffer) {
  stream::IntervalReader reader = str.GetBytesReader();
  if (reader.interval_size() > buffer.size()) {
    return Status::ResourceExhausted();
  }

  Result<ByteSpan> res = reader.Read(std::as_writable_bytes(buffer));
  PW_TRY(res.status());
  return std::string_view(buffer.data(), res.value().size());
}

}  // namespace

Status UpdateBundleAccessor::OpenAndVerify() {
  PW_TRY(DoOpen());
  PW_TRY(DoVerify());
  return OkStatus();
}

// Get the target element corresponding to `target_file`
stream::IntervalReader UpdateBundleAccessor::GetTargetPayload(
    std::string_view target_file_name) {
  if (!bundle_verified_) {
    PW_LOG_DEBUG("Bundled has not passed verification yet");
    return Status::FailedPrecondition();
  }

  protobuf::StringToBytesMap target_payloads =
      decoder_.AsStringToBytesMap(static_cast<uint32_t>(
          pw::software_update::UpdateBundle::Fields::TARGET_PAYLOADS));
  PW_TRY(target_payloads.status());
  protobuf::Bytes payload = target_payloads[target_file_name];
  PW_TRY(payload.status());
  return payload.GetBytesReader();
}

protobuf::Message UpdateBundleAccessor::GetDecoder() {
  if (!bundle_verified_) {
    PW_LOG_DEBUG("Bundled has not passed verification yet");
    return Status::FailedPrecondition();
  }

  return decoder_;
}

Result<bool> UpdateBundleAccessor::IsTargetPayloadIncluded(
    std::string_view target_file_name) {
  if (!bundle_verified_) {
    PW_LOG_DEBUG("Bundled has not passed verification yet");
    return Status::FailedPrecondition();
  }
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

Status UpdateBundleAccessor::PersistManifest(
    stream::Writer& staged_manifest_writer) {
  if (!bundle_verified_) {
    PW_LOG_DEBUG(
        "Bundle has not passed verification. Refuse to write manifest");
    return Status::FailedPrecondition();
  }

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
  PW_TRY(protobuf::WriteProtoStringToBytesMapEntry(
      static_cast<uint32_t>(
          pw::software_update::Manifest::Fields::TARGETS_METADATA),
      name_reader,
      kTopLevelTargetsName.size(),
      metadata_reader,
      metadata_reader.interval_size(),
      stream_pipe_buffer,
      staged_manifest_writer));

  // Write `user_manifest` file if there is one.
  Result<bool> user_manifest_exists =
      IsTargetPayloadIncluded(kUserManifestTargetFileName);
  PW_TRY(user_manifest_exists);
  if (user_manifest_exists.value()) {
    stream::IntervalReader user_manifest_reader =
        GetTargetPayload(kUserManifestTargetFileName);
    PW_TRY(user_manifest_reader.status());
    protobuf::StreamEncoder encoder(staged_manifest_writer, {});
    PW_TRY(encoder.WriteBytesFromStream(
        static_cast<uint32_t>(Manifest::Fields::USER_MANIFEST),
        user_manifest_reader,
        user_manifest_reader.interval_size(),
        stream_pipe_buffer));
  }

  return OkStatus();
}

Status UpdateBundleAccessor::Close() {
  bundle_verified_ = false;
  return bundle_reader_.IsOpen() ? bundle_reader_.Close() : OkStatus();
}

Status UpdateBundleAccessor::DoOpen() {
  PW_TRY(bundle_.Init());
  PW_TRY(bundle_reader_.Open());
  decoder_ =
      protobuf::Message(bundle_reader_, bundle_reader_.ConservativeReadLimit());
  return OkStatus();
}

Status UpdateBundleAccessor::DoVerify() {
#if PW_SOFTWARE_UPDATE_DISABLE_BUNDLE_VERIFICATION
  PW_LOG_WARN("Update bundle verification is disabled.");
  bundle_verified_ = true;
  return OkStatus();
#else   // PW_SOFTWARE_UPDATE_DISABLE_BUNDLE_VERIFICATION
  bundle_verified_ = false;
  if (disable_verification_) {
    PW_LOG_WARN("Update bundle verification is disabled.");
    bundle_verified_ = true;
    return OkStatus();
  }

  // Verify and upgrade the on-device trust to the incoming root metadata if
  // one is included.
  PW_TRY(UpgradeRoot());

  // TODO(pwbug/456): Verify the targets metadata against the current trusted
  // root.
  PW_TRY(VerifyTargetsMetadata());

  // TODO(pwbug/456): Investigate whether targets payload verification should
  // be performed here or deferred until a specific target is requested.
  PW_TRY(VerifyTargetsPayloads());

  // TODO(pwbug/456): Invoke the backend to do downstream verification of the
  // bundle (e.g. compatibility and manifest completeness checks).

  bundle_verified_ = true;
  return OkStatus();
#endif  // PW_SOFTWARE_UPDATE_DISABLE_BUNDLE_VERIFICATION
}

protobuf::Message UpdateBundleAccessor::GetOnDeviceTrustedRoot() {
  Result<stream::SeekableReader*> res = backend_.GetRootMetadataReader();
  PW_TRY(res.status());
  PW_CHECK_NOTNULL(res.value());
  // Seek to the beginning so that ConservativeReadLimit() returns the correct
  // value.
  PW_TRY(res.value()->Seek(0, stream::Stream::Whence::kBeginning));
  return protobuf::Message(*res.value(), res.value()->ConservativeReadLimit());
}

Status UpdateBundleAccessor::UpgradeRoot() {
  protobuf::Message new_root = decoder_.AsMessage(
      static_cast<uint32_t>(UpdateBundle::Fields::ROOT_METADATA));
  if (new_root.status().IsNotFound()) {
    return OkStatus();
  }

  PW_TRY(new_root.status());

  // Get the trusted root and prepare for verification.
  protobuf::Message trusted_root = GetOnDeviceTrustedRoot();
  PW_TRY(trusted_root.status());

  // TODO(pwbug/456): Check whether the bundle contains a root metadata that
  // is different from the on-device trusted root.

  // Verify the signatures against the trusted root metadata.
  Result<bool> verify_res =
      VerifyRootMetadataSignatures(trusted_root, new_root);
  PW_TRY(verify_res.status());
  if (!verify_res.value()) {
    PW_LOG_INFO("Fail to verify signatures against the current root");
    return Status::Unauthenticated();
  }

  // TODO(pwbug/456): Verifiy the content of the new root metadata, including:
  //    1) Check role magic field.
  //    2) Check signature requirement. Specifically, check that no key is
  //       reused across different roles and keys are unique in the same
  //       requirement.
  //    3) Check key mapping. Specifically, check that all keys are unique,
  //       ECDSA keys, and the key ids are exactly the SHA256 of `key type +
  //       key scheme + key value`.

  // Verify the signatures against the new root metadata.
  verify_res = VerifyRootMetadataSignatures(new_root, new_root);
  PW_TRY(verify_res.status());
  if (!verify_res.value()) {
    PW_LOG_INFO("Fail to verify signatures against the new root");
    return Status::Unauthenticated();
  }

  // TODO(pwbug/456): Check rollback.
  // Retrieves the trusted root metadata content message.
  protobuf::Message trusted_root_content =
      trusted_root.AsMessage(static_cast<uint32_t>(
          SignedRootMetadata::Fields::SERIALIZED_ROOT_METADATA));
  PW_TRY(trusted_root_content.status());
  Result<uint32_t> trusted_root_version = GetMetadataVersion(
      trusted_root_content,
      static_cast<uint32_t>(RootMetadata::Fields::COMMON_METADATA));
  PW_TRY(trusted_root_version.status());

  // Retrieves the serialized new root metadata message.
  protobuf::Message new_root_content = new_root.AsMessage(static_cast<uint32_t>(
      SignedRootMetadata::Fields::SERIALIZED_ROOT_METADATA));
  PW_TRY(new_root_content.status());
  Result<uint32_t> new_root_version = GetMetadataVersion(
      new_root_content,
      static_cast<uint32_t>(RootMetadata::Fields::COMMON_METADATA));
  PW_TRY(new_root_version.status());

  if (trusted_root_version.value() > new_root_version.value()) {
    PW_LOG_DEBUG("Root attempts to rollback from %u to %u.",
                 trusted_root_version.value(),
                 new_root_version.value());
    return Status::Unauthenticated();
  }

  // Persist the root immediately after it is successfully verified. This is
  // to make sure the trust anchor is up-to-date in storage as soon as
  // we are confident. Although targets metadata and product-specific
  // verification have not been done yet. They should be independent from and
  // not gate the upgrade of root key. This allows timely revokation of
  // compromise keys.
  stream::IntervalReader new_root_reader = new_root.ToBytes().GetBytesReader();
  PW_TRY(backend_.SafelyPersistRootMetadata(new_root_reader));

  // TODO(pwbug/456): Implement key change detection to determine whether
  // rotation has occured or not. Delete the persisted targets metadata version
  // if any of the targets keys has been rotated.

  return OkStatus();
}

Status UpdateBundleAccessor::VerifyTargetsMetadata() {
  // Retrieve the signed targets metadata map.
  //
  // message UpdateBundle {
  //   ...
  //   map<string, SignedTargetsMetadata> target_metadata = <id>;
  //   ...
  // }
  protobuf::StringToMessageMap signed_targets_metadata_map =
      decoder_.AsStringToMessageMap(
          static_cast<uint32_t>(UpdateBundle::Fields::TARGETS_METADATA));
  PW_TRY(signed_targets_metadata_map.status());

  // The top-level targets metadata is identified by key name "targets" in the
  // map.
  protobuf::Message signed_top_level_targets_metadata =
      signed_targets_metadata_map[kTopLevelTargetsName];
  PW_TRY(signed_top_level_targets_metadata.status());

  // Retrieve the serialized metadata.
  //
  // message SignedTargetsMetadata {
  //   ...
  //   bytes serialized_target_metadata = <id>;
  //   ...
  // }
  protobuf::Message top_level_targets_metadata =
      signed_top_level_targets_metadata.AsMessage(static_cast<uint32_t>(
          SignedTargetsMetadata::Fields::SERIALIZED_TARGETS_METADATA));

  // Get the sigantures from the signed targets metadata.
  protobuf::RepeatedMessages signatures =
      signed_top_level_targets_metadata.AsRepeatedMessages(
          static_cast<uint32_t>(SignedTargetsMetadata::Fields::SIGNATURES));
  PW_TRY(signatures.status());

  // Get the trusted root and prepare for verification.
  protobuf::Message signed_trusted_root = GetOnDeviceTrustedRoot();
  PW_TRY(signed_trusted_root.status());

  // Retrieve the trusted root metadata message.
  protobuf::Message trusted_root =
      signed_trusted_root.AsMessage(static_cast<uint32_t>(
          SignedRootMetadata::Fields::SERIALIZED_ROOT_METADATA));
  PW_TRY(trusted_root.status());

  // Get the key_mapping from the trusted root metadata.
  protobuf::StringToMessageMap key_mapping = trusted_root.AsStringToMessageMap(
      static_cast<uint32_t>(RootMetadata::Fields::KEYS));
  PW_TRY(key_mapping.status());

  // Get the targest metadtata siganture requirement from the trusted root.
  protobuf::Message signature_requirement =
      trusted_root.AsMessage(static_cast<uint32_t>(
          RootMetadata::Fields::TARGETS_SIGNATURE_REQUIREMENT));
  PW_TRY(signature_requirement.status());

  // Verify the sigantures
  Result<bool> sig_res =
      VerifyMetadataSignatures(top_level_targets_metadata.ToBytes(),
                               signatures,
                               signature_requirement,
                               key_mapping);

  PW_TRY(sig_res.status());
  if (!sig_res.value()) {
    PW_LOG_DEBUG("Fail to verify targets metadata signatures");
    return Status::Unauthenticated();
  }

  // TODO(pwbug/456): Check targets metadtata content.

  // Get on-device manifest.
  Result<stream::SeekableReader*> manifest_reader =
      backend_.GetCurrentManifestReader();
  PW_TRY(manifest_reader.status());
  PW_CHECK_NOTNULL(manifest_reader.value());
  protobuf::Message manifest(*manifest_reader.value(),
                             manifest_reader.value()->ConservativeReadLimit());

  // Retrieves the targest metdata map from the manifest
  //
  // message Manifest {
  //   ...
  //   map<string, TargetsMetadata> targets_metadata = <id>;
  //   ...
  // }
  protobuf::StringToMessageMap manifest_targets_metadata_map =
      manifest.AsStringToMessageMap(
          static_cast<uint32_t>(Manifest::Fields::TARGETS_METADATA));
  PW_TRY(manifest_targets_metadata_map.status());

  // Retrieves the top-level targets metadata from the map and get the version
  uint32_t current_ver;
  protobuf::Message manifest_top_level_targets_metadata =
      manifest_targets_metadata_map[kTopLevelTargetsName];
  if (manifest_top_level_targets_metadata.status().IsNotFound()) {
    // If the top-level targets metadata is missing, then either the device has
    // never received any prior update, or manifest has been reset in the case
    // of key rotation. In this case, current version is assumed to be 0.
    PW_LOG_DEBUG(
        "Cannot find top-level targets metadata from the current manifest. "
        "Current rollback index is treated as 0");
    current_ver = 0;
  } else {
    PW_TRY(manifest_top_level_targets_metadata.status());
    Result<uint32_t> version = GetMetadataVersion(
        manifest_top_level_targets_metadata,
        static_cast<uint32_t>(
            software_update::TargetsMetadata::Fields::COMMON_METADATA));
    PW_TRY(version.status());
    current_ver = version.value();
  }

  // Retrieves the version from the new metadata
  Result<uint32_t> new_version = GetMetadataVersion(
      top_level_targets_metadata,
      static_cast<uint32_t>(
          software_update::TargetsMetadata::Fields::COMMON_METADATA));
  PW_TRY(new_version.status());
  if (current_ver > new_version.value()) {
    PW_LOG_DEBUG("Targets attempt to rollback from %u to %u.",
                 current_ver,
                 new_version.value());
    return Status::Unauthenticated();
  }

  return OkStatus();
}

Status UpdateBundleAccessor::VerifyTargetsPayloads() {
  // Gets the list of targets.
  protobuf::RepeatedMessages target_files = GetTopLevelTargets(decoder_);
  PW_TRY(target_files.status());

  // Gets the list of payloads.
  //
  // message UpdateBundle {
  //   ...
  //   map<string, bytes> target_payloads = <id>;
  //   ...
  // }
  protobuf::StringToBytesMap target_payloads = decoder_.AsStringToBytesMap(
      static_cast<uint32_t>(UpdateBundle::Fields::TARGET_PAYLOADS));
  PW_TRY(target_payloads.status());

  // Checks hashes for all targets.
  for (protobuf::Message target_file : target_files) {
    // Extract `file_name`, `length` and `hashes` for each target in the
    // metadata.
    //
    // message TargetFile {
    //   ...
    //   string file_name = <id>;
    //   uint64 length = <id>;
    //   ...
    // }
    protobuf::String target_name = target_file.AsString(
        static_cast<uint32_t>(TargetFile::Fields::FILE_NAME));
    PW_TRY(target_name.status());

    protobuf::Uint64 target_length =
        target_file.AsUint64(static_cast<uint32_t>(TargetFile::Fields::LENGTH));
    PW_TRY(target_length.status());

    char target_name_read_buf[MAX_TARGET_NAME_LENGTH] = {0};
    Result<std::string_view> target_name_sv =
        ReadProtoString(target_name, target_name_read_buf);
    PW_TRY(target_name_sv.status());

    // Finds the target in the target payloads
    protobuf::Bytes target_payload = target_payloads[target_name_sv.value()];
    if (target_payload.status().IsNotFound()) {
      PW_LOG_DEBUG(
          "target payload for %s does not exist. Assumed personalized out",
          target_name_read_buf);
      // Invoke backend specific check
      PW_TRY(backend_.VerifyTargetFile(GetManifestAccessor(),
                                       target_name_sv.value()));
      continue;
    }

    PW_TRY(target_payload.status());
    // Payload size must matches file length
    if (target_payload.GetBytesReader().interval_size() !=
        target_length.value()) {
      PW_LOG_DEBUG("Target payload size mismatch");
      return Status::Unauthenticated();
    }

    // Gets the list of hashes
    //
    // message TargetFile {
    //   ...
    //   repeated Hash hashes = <id>;
    //   ...
    // }
    protobuf::RepeatedMessages hashes = target_file.AsRepeatedMessages(
        static_cast<uint32_t>(TargetFile::Fields::HASHES));
    PW_TRY(hashes.status());

    // Check all hashes
    size_t num_hashes = 0;
    for (protobuf::Message hash : hashes) {
      num_hashes++;
      Result<bool> hash_verify_res =
          VerifyTargetPayloadHash(hash, target_payload);
      PW_TRY(hash_verify_res.status());
      if (!hash_verify_res.value()) {
        PW_LOG_DEBUG("sha256 hash mismatch for file %s", target_name_read_buf);
        return Status::Unauthenticated();
      }
    }  // for (protobuf::Message hash : hashes)

    // The packet does not contain any hash
    if (!num_hashes) {
      PW_LOG_DEBUG("No hash for file %s", target_name_read_buf);
      return Status::Unauthenticated();
    }
  }  // for (protobuf::Message target_file : target_files)

  return OkStatus();
}

}  // namespace pw::software_update
