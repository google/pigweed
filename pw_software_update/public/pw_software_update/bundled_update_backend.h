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

#include <string_view>

#include "pw_result/result.h"
#include "pw_software_update/manifest_accessor.h"
#include "pw_software_update/update_bundle_accessor.h"
#include "pw_status/status.h"
#include "pw_stream/interval_reader.h"
#include "pw_stream/stream.h"

namespace pw::software_update {

// TODO(pwbug/478): update documentation for backend api contract
class BundledUpdateBackend {
 public:
  virtual ~BundledUpdateBackend() = default;

  // Optionally verify that the instance/content of the target file in use
  // on-device matches the metadata in the given manifest, called before apply.
  // (e.g. by checksum, if failed abort partial update and wipe/mark-invalid
  // running manifest)
  virtual Status VerifyTargetFile(
      [[maybe_unused]] ManifestAccessor manifest,
      [[maybe_unused]] std::string_view target_file_name) {
    return OkStatus();
  };

  // Perform any product-specific tasks needed before starting update sequence.
  virtual Status BeforeUpdateStart() { return OkStatus(); };

  // Attempts to enable the transfer service transfer handler, returning the
  // transfer_id if successful. This is invoked after BeforeUpdateStart();
  virtual Result<uint32_t> EnableBundleTransferHandler(
      std::string_view bundle_filename) = 0;

  // Disables the transfer service transfer handler. This is invoked after
  // either BeforeUpdateAbort() or BeforeBundleVerify().
  virtual void DisableBundleTransferHandler() = 0;

  // Perform any product-specific abort tasks before marking the update as
  // aborted in bundled updater.  This should set any downstream state to a
  // default no-update-pending state.
  // TODO: Revisit invariants; should this instead be "Abort()"? This is called
  // for all error paths in the service and needs to reset. Furthermore, should
  // this be async?
  virtual Status BeforeUpdateAbort() { return OkStatus(); };

  // Perform any product-specific tasks needed before starting verification.
  virtual Status BeforeBundleVerify() { return OkStatus(); };

  // Perform any product-specific bundle verification tasks (e.g. hw version
  // match check), done after TUF bundle verification process.
  virtual Status VerifyManifest(
      [[maybe_unused]] ManifestAccessor manifest_accessor) {
    return OkStatus();
  };

  // Perform product-specific tasks after all bundle verifications are complete.
  virtual Status AfterBundleVerified() { return OkStatus(); };

  // Perform any product-specific tasks before apply sequence started
  virtual Status BeforeApply() { return OkStatus(); };

  // Get status information from update backend. This will not be called when
  // BundledUpdater is in a step where it has entire control with no operation
  // handed over to update backend.
  virtual int64_t GetStatus() { return 0; }

  // Update the specific target file on the device.
  virtual Status ApplyTargetFile(std::string_view target_file_name,
                                 stream::Reader& target_payload,
                                 size_t update_bundle_offset) = 0;

  // Get reader of the device's current manifest.
  virtual Result<stream::SeekableReader*> GetCurrentManifestReader() {
    return Status::Unimplemented();
  }

  // Use a reader that provides a new manifest for the device to save.
  virtual Status UpdateCurrentManifest(
      [[maybe_unused]] stream::Reader& manifest) {
    return OkStatus();
  };

  // Do any work needed to finish the apply of the update and do a required
  // reboot of the device!
  //
  // NOTE: If successful this method does not return and reboots the device, it
  // only returns on failure to finalize.
  //
  // NOTE: ApplyReboot shall be configured such to allow pending RPC or logs to
  // send out the reply before the device reboots.
  virtual Status ApplyReboot() = 0;

  // Do any work needed to finalize the update including optionally doing a
  // reboot of the device! The software update state and breadcrumbs are not
  // cleaned up until this method returns OK.
  //
  // This method is called after the reboot done as part of ApplyReboot().
  //
  // If this method does an optional reboot, it will be called again after the
  // reboot.
  //
  // NOTE: PostRebootFinalize shall be configured such to allow pending RPC or
  // logs to send out the reply before the device reboots.
  virtual Status PostRebootFinalize() { return OkStatus(); }

  // Get reader of the device's root metadata.
  //
  // This method MUST return a valid root metadata once verified OTA is enabled.
  // An invalid or corrupted root metadata will result in permanent OTA
  // failures.
  virtual Result<stream::SeekableReader*> GetRootMetadataReader() {
    return Status::Unimplemented();
  };

  // Write a given root metadata to persistent storage in a failsafe manner.
  //
  // The updating must be atomic/fail-safe. An invalid or corrupted root
  // metadata will result in permanent OTA failures.
  //
  // TODO(pwbug/456): Investigate whether we should get a writer i.e.
  // GetRootMetadataWriter() instead of passing a reader.
  virtual Status SafelyPersistRootMetadata(
      [[maybe_unused]] stream::IntervalReader root_metadata) {
    return Status::Unimplemented();
  };
};

}  // namespace pw::software_update
