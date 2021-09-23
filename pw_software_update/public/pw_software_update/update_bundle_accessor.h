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
// auto status = bundle.OpenAndVerify(current_manifest);
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
// status = bundle.WriteManifest(staged_manifest_writer);
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
  // update_bundle - The software update bundle data on storage.
  // backend - project-specific BundledUpdateBackend
  constexpr UpdateBundleAccessor(blob_store::BlobStore& update_bundle,
                                 BundledUpdateBackend& backend)
      : bundle_(update_bundle), backend_(backend), bundle_reader_(bundle_) {}

  // Opens and verifies the software update bundle, using the TUF process.
  //
  // Returns:
  // OK - Bundle was successfully opened and verified.
  // TODO(pwbug/456): Add error codes.
  Status OpenAndVerify(const ManifestAccessor& current_manifest);

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
  Status WriteManifest(stream::Writer& staged_manifest_writer);

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

  protobuf::Message GetDecoder() { return decoder_; }

 private:
  blob_store::BlobStore& bundle_;
  BundledUpdateBackend& backend_;
  blob_store::BlobStore::BlobReader bundle_reader_;
  protobuf::Message decoder_;
};

}  // namespace pw::software_update
