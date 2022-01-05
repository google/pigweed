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

#include "pw_software_update/bundled_update.rpc.pb.h"
#include "pw_software_update/bundled_update_backend.h"
#include "pw_software_update/update_bundle_accessor.h"
#include "pw_status/status.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"
#include "pw_work_queue/work_queue.h"

namespace pw::software_update {

// Implementation class for pw.software_update.BundledUpdate.
// See bundled_update.proto for RPC method documentation.
class BundledUpdateService
    : public pw_rpc::nanopb::BundledUpdate::Service<BundledUpdateService> {
 public:
  BundledUpdateService(UpdateBundleAccessor& bundle,
                       BundledUpdateBackend& backend,
                       work_queue::WorkQueue& work_queue)
      : status_{},
        backend_(backend),
        bundle_(bundle),
        bundle_open_(false),
        work_queue_(work_queue),
        work_enqueued_(false) {
    status_.state = pw_software_update_BundledUpdateState_Enum_INACTIVE;
  }

  Status GetStatus(const pw_protobuf_Empty& request,
                   pw_software_update_BundledUpdateStatus& response);

  // Sync
  Status Start(const pw_software_update_StartRequest& request,
               pw_software_update_BundledUpdateStatus& response);

  // Sync
  Status SetTransferred(const pw_protobuf_Empty& request,
                        pw_software_update_BundledUpdateStatus& response);

  // Async
  Status Verify(const pw_protobuf_Empty& request,
                pw_software_update_BundledUpdateStatus& response);

  // Async
  Status Apply(const pw_protobuf_Empty& request,
               pw_software_update_BundledUpdateStatus& response);

  // Currently sync, should be async.
  // TODO: Make this async to support aborting verify/apply.
  Status Abort(const pw_protobuf_Empty& request,
               pw_software_update_BundledUpdateStatus& response);

  // Sync
  Status Reset(const pw_protobuf_Empty& request,
               pw_software_update_BundledUpdateStatus& response);

  // Notify the service that the bundle transfer has completed. The service has
  // no way to know when the bundle transfer completes, so users must invoke
  // this method in their transfer completion handler.
  //
  // After this call, the service will be in TRANSFERRED state if and only if
  // it was in the TRANSFERRING state.
  void NotifyTransferSucceeded();

  // TODO(davidrogers) Add a MaybeFinishApply() method that is called after
  // reboot to finish any need apply and verify work.

  // TODO:
  // VerifyProgress - to update % complete.
  // ApplyProgress - to update % complete.

 private:
  pw_software_update_BundledUpdateStatus status_ PW_GUARDED_BY(mutex_);
  BundledUpdateBackend& backend_ PW_GUARDED_BY(mutex_);
  UpdateBundleAccessor& bundle_ PW_GUARDED_BY(mutex_);
  bool bundle_open_ PW_GUARDED_BY(mutex_);
  work_queue::WorkQueue& work_queue_ PW_GUARDED_BY(mutex_);
  bool work_enqueued_ PW_GUARDED_BY(mutex_);
  sync::Mutex mutex_;

  void DoVerify();
  void DoApply();
  void Finish(_pw_software_update_BundledUpdateResult_Enum result)
      PW_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool IsFinished() const PW_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return status_.state == pw_software_update_BundledUpdateState_Enum_FINISHED;
  }
};

}  // namespace pw::software_update
