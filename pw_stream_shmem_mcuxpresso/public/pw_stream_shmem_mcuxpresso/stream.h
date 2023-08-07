// Copyright 2023 The Pigweed Authors
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

#include <cstdint>

#include "fsl_mu.h"
#include "pw_bytes/span.h"
#include "pw_status/status.h"
#include "pw_stream/stream.h"
#include "pw_sync/binary_semaphore.h"

namespace pw::stream {

// Stream for reading/writing between processor cores using the Mcuxpresso SDK.
//
// It uses the MU module from the SDK for signaling data readiness. MU channels
// 0 and 1 are claimed for exclusive use. Each core should have an instance of
// this class with shared buffers pointing at the same physical memory locations
// that is uncached on both sides.
//
// Interrupt setup is different between cores, so that is left to the user. An
// example can be found in the docs. In the MU interrupt handler on each core,
// the `HandleInterrupt` function on the stream should be called.
class ShmemMcuxpressoStream : public NonSeekableReaderWriter {
 public:
  ShmemMcuxpressoStream(MU_Type* base,
                        ByteSpan shared_read_buffer,
                        ByteSpan shared_write_buffer)
      : base_(base),
        shared_read_buffer_(shared_read_buffer),
        shared_write_buffer_(shared_write_buffer) {}
  ~ShmemMcuxpressoStream();

  void Enable();
  void Disable();

  // To be called when MU interrupt fires.
  void HandleInterrupt();

 private:
  StatusWithSize DoRead(ByteSpan) override;
  Status DoWrite(ConstByteSpan) override;

  MU_Type* const base_;
  ByteSpan shared_read_buffer_;
  ByteSpan shared_write_buffer_;
  sync::BinarySemaphore read_semaphore_;
  sync::BinarySemaphore write_semaphore_;
  sync::BinarySemaphore write_done_semaphore_;
};

}  // namespace pw::stream
