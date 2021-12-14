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

#include <cstddef>

#include "pw_blob_store/blob_store.h"
#include "pw_protobuf/map_utils.h"
#include "pw_protobuf/message.h"
#include "pw_software_update/bundled_update_backend.h"
#include "pw_software_update/manifest_accessor.h"
#include "pw_stream/memory_stream.h"

namespace pw::software_update {
class BundledUpdateBackend;

constexpr std::string_view kUserManifestTargetFileName = "user_manifest";

// UpdateBundleAccessor is responsible for parsing, verifying and providing
// target payload access of a software update bundle. It takes the following as
// inputs:
//
// 1. A software update bundle via `BlobStore`.
// 2. A `BundledUpdateBackend`, which implements project-specific update
//    operations such as enforcing project update policies and
//    verifying/applying target files on device.
//
// The verification is done according to TUF process. Payload can only be
// accessed after successful verification.
//
// Exmple of use:
//
// UpdateBundleAccessor bundle(blob,helper);
// auto status = bundle.OpenAndVerify();
// if (!status.ok()) {
//   // handle error
//   ...
// }
//
// // Examine and use payload.
// auto exist = bundle.IsTargetPayloadIncluded("audio");
// if (!exist.ok() || !exist.value()) {
//   // handle error
//   ...
// }
//
// auto payload_reader = bundle.GetTargetPayload("audio");
// // Process payload
// ...
//
// // Get bundle's manifest and write it to the given writer.
// status = bundle.PersistManifest(staged_manifest_writer);
// if (!status.ok()) {
//   // handle error
//   ...
// }
//
// status = bundle.Close();
// if (!status.ok()) {
//   // handle error
//   ...
// }
class UpdateBundleAccessor {
 public:
  // UpdateBundleAccessor
  // bundle - The software update bundle data on storage.
  // backend - Project-specific BundledUpdateBackend.
  // disable_verification - Disable verification.
  constexpr UpdateBundleAccessor(blob_store::BlobStore& bundle,
                                 BundledUpdateBackend& backend,
                                 bool disable_verification = false)
      : bundle_(bundle),
        backend_(backend),
        bundle_reader_(bundle_),
        disable_verification_(disable_verification) {}

  // Opens and verifies the software update bundle.
  //
  // Specifically, the opening process opens a blob reader to the given bundle
  // and initializes the bundle proto parser. No write will be allowed to the
  // bundle until Close() is called.
  //
  // If bundle verification is enabled (see the `option` argument in
  // the constructor), the verification process does the following:
  //
  // 1. Check whether the bundle contains an incoming new root metadata. If it
  // does, it verifies the root against the current on-device root. If
  // successful, the on-device root will be updated to the new root.
  //
  // 2. Verify the targets metadata against the current trusted root.
  //
  // 3. Either verify all target payloads (size and hash) or defer that
  // verification till when a target is accessed.
  //
  // 4. Invoke the backend to do downstream verification of the bundle.
  //
  // Returns:
  // OK - Bundle was successfully opened and verified.
  Status OpenAndVerify();

  // Closes the bundle by invalidating the verification and closing
  // the reader to release the read-only lock
  //
  // Returns:
  // OK - success.
  // DATA_LOSS - Error writing data or fail to verify written data.
  Status Close();

  // Writes the manifest of the staged bundle to the given writer.
  //
  // Returns:
  // FAILED_PRECONDITION - Bundle is not open and verified.
  // TODO(pwbug/456): Add other error codes if necessary.
  Status PersistManifest(stream::Writer& staged_manifest_writer);

  // Is the target payload present in the bundle (not personalized out).
  //
  // Returns:
  // OK - Whether or not the target_file was included in the UpdateBundle or
  //      whether it was personalized out.
  // FAILED_PRECONDITION - Bundle is not open and verified.
  // TODO(pwbug/456): Add other error codes if necessary.
  Result<bool> IsTargetPayloadIncluded(std::string_view target_file);

  // Returns a reader for the target file by `target_file` in the update
  // bundle.
  //
  // Returns:
  // A reader instance for the target file.
  // TODO(pwbug/456): Figure out a way to propagate error.
  stream::IntervalReader GetTargetPayload(std::string_view target_file);

  // Returns a protobuf::Message representation of the update bundle.
  //
  // Returns:
  // An instance of protobuf::Message of the udpate bundle.
  // FAILED_PRECONDITION - Bundle is not open and verified.
  protobuf::Message GetDecoder();

  ManifestAccessor GetManifestAccessor() { return ManifestAccessor(this); };

 private:
  blob_store::BlobStore& bundle_;
  BundledUpdateBackend& backend_;
  blob_store::BlobStore::BlobReader bundle_reader_;
  protobuf::Message decoder_;
  bool disable_verification_;
  bool bundle_verified_ = false;

  // Opens the bundle for read-only access and readies the parser.
  Status DoOpen();

  // Performs TUF and downstream custom verification.
  Status DoVerify();

  // The method checks whether the update bundle contains a root metadata
  // different from the on-device one. If it does, it performs the following
  // verification and upgrade flow:
  //
  // 1. Verify the signatures according to the on-device trusted
  // disable_verificationroot metadata
  //    obtained from the backend.
  // 2. Verify content of the new root metadata, including:
  //    1) Check role magic field.
  //    2) Check signature requirement. Specifically, check that no key is
  //       reused across different roles and keys are unique in the same
  //       requirement.
  //    3) Check key mapping. Specifically, check that all keys are unique,
  //       ECDSA keys, and the key ids are exactly the SHA256 of `key type +
  //       key scheme + key value`.
  // 3. Verify the signatures against the new root metadata.
  // 4. Check rollback.
  // 5. Update on-device root metadata.
  Status UpgradeRoot();

  // The method verifies the top-level targets metadata against the trusted
  // root. The verification includes the following:
  //
  // 1. Verify the signatures of the targets metadata.
  // 2. Check the content of the targets metadata.
  // 3. Check rollback against the version from on-device manifest, if one
  //    exists (the manifest may be reset in the case of key rotation).
  //
  // TODO(pwbug/456): Should manifest persisting be handled here? The current
  // API design of this class exposes a PersistManifest() method, which implies
  // that manifest persisting is handled by some higher level logic.
  Status VerifyTargetsMetadata();

  // A helper to get the on-device trusted root metadata. It returns an
  // instance of SignedRootMetadata proto message.
  protobuf::Message GetOnDeviceTrustedRoot();

  // The method performs verification of the target payloads. Specifically, it
  // 1. For target payloads found in the bundle, verify its size and hash.
  // 2. For target payloads not found in the bundle, call downstream to verify
  // it and report back.
  Status VerifyTargetsPayloads();
};

}  // namespace pw::software_update
