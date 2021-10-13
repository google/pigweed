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
#include "pw_status/status.h"
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
      [[maybe_unused]] const ManifestAccessor& manifest,
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
  // match check), done after TUF bundle verification process if user_manifest
  // was provided as part of the bundle.
  virtual Status VerifyUserManifest(
      [[maybe_unused]] stream::Reader& user_manifest,
      [[maybe_unused]] size_t update_bundle_offset) {
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
  virtual Status GetCurrentManifestReader(
      [[maybe_unused]] stream::Reader* out) {
    return Status::Unimplemented();
  };

  // Use a reader that provides a new manifest for the device to save.
  virtual Status UpdateCurrentManifest(
      [[maybe_unused]] stream::Reader& manifest) {
    return OkStatus();
  };

  // Do any work needed to finalize the update including doing a required
  // reboot of the device! This is called after all software update state and
  // breadcrumbs have been cleaned up.
  //
  // After the reboot the update is fully complete.
  //
  // NOTE: If successful this method does not return and reboots the device, it
  // only returns on failure to finalize.
  virtual Status FinalizeApply() = 0;

  // Get reader of the device's root metadata.
  //
  // This method ALWAYS needs to be able to return a valid root metadata.
  // Failure to have a safe update can result in inability to do future
  // updates due to not having required metadata.
  virtual Status GetRootMetadataReader([[maybe_unused]] stream::Reader* out) {
    return Status::Unimplemented();
  };

  // Use a reader that provides a new root metadata for the device to save.
  //
  // This method needs to do updates in a reliable and failsafe way with no
  // window of vulnerability. It needs to ALWAYS be able to return a valid root
  // metadata. Failure to have a safe update can result in inability to do
  // future updates due to not having required metadata.
  virtual Status UpdateRootMetadata(
      [[maybe_unused]] stream::Reader& root_metadata) {
    return OkStatus();
  };
};

}  // namespace pw::software_update
