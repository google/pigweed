// Copyright 2022 The Pigweed Authors
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

#include "pw_rpc/raw/server_reader_writer.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_thread/config.h"
#include "pw_thread/thread_info.h"
#include "pw_thread_protos/thread.pwpb.h"
#include "pw_thread_protos/thread_snapshot_service.pwpb.h"
#include "pw_thread_protos/thread_snapshot_service.raw_rpc.pb.h"

namespace pw::thread {

Status ProtoEncodeThreadInfo(SnapshotThreadInfo::MemoryEncoder& encoder,
                             const ThreadInfo& thread_info);

constexpr size_t RequiredServiceBufferSize(
    const size_t num_threads = PW_THREAD_MAXIMUM_THREADS) {
  constexpr size_t kSizeOfResponse =
      SnapshotThreadInfo::kMaxEncodedSizeBytes + Thread::kMaxEncodedSizeBytes;
  return kSizeOfResponse * num_threads;
}

// The ThreadSnapshotService will return peak stack usage across running
// threads when requested by GetPeak().
class ThreadSnapshotService final
    : public pw_rpc::raw::ThreadSnapshotService::Service<
          ThreadSnapshotService> {
 public:
  ThreadSnapshotService(span<std::byte> encode_buffer)
      : encode_buffer_(encode_buffer) {}

  void GetPeakStackUsage(ConstByteSpan request, rpc::RawServerWriter& response);

 private:
  span<std::byte> encode_buffer_;
};

}  // namespace pw::thread
