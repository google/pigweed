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
#include "pw_software_update/manifest_accessor.h"

#include "pw_assert/check.h"
#include "pw_software_update/update_bundle_accessor.h"

namespace pw::software_update {

ManifestAccessor::ManifestAccessor(UpdateBundleAccessor* update_bundle_accessor)
    : update_bundle_accessor_(update_bundle_accessor) {
  PW_CHECK_NOTNULL(update_bundle_accessor_);
}

Status ManifestAccessor::WriteManifest(stream::Writer& writer) {
  return update_bundle_accessor_->PersistManifest(writer);
};

pw::stream::IntervalReader ManifestAccessor::GetUserManifest() {
  stream::IntervalReader user_manifest =
      update_bundle_accessor_->GetTargetPayload("user_manifest");
  return user_manifest;
};

}  // namespace pw::software_update
