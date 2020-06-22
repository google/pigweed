// Copyright 2020 The Pigweed Authors
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
#include <span>

#include "pw_kvs/alignment.h"
#include "pw_status/status.h"

namespace pw::kvs {

class ChecksumAlgorithm {
 public:
  // Resets the checksum to its initial state.
  virtual void Reset() = 0;

  // Updates the checksum with the provided data.
  virtual void Update(std::span<const std::byte> data) = 0;

  // Updates the checksum from a pointer and size.
  void Update(const void* data, size_t size_bytes) {
    return Update(std::span(static_cast<const std::byte*>(data), size_bytes));
  }

  // Returns the final result of the checksum. Update() can no longer be called
  // after this. The returned std::span is valid until a call to Reset().
  //
  // Finish MUST be called before calling Verify.
  std::span<const std::byte> Finish() {
    Finalize();  // Implemented by derived classes, if required.
    return state();
  }

  // Returns the size of the checksum state.
  constexpr size_t size_bytes() const { return state_.size(); }

  // Compares a calculated checksum to this checksum's state. The checksum must
  // be at least as large as size_bytes(). If it is larger, bytes beyond
  // size_bytes() are ignored.
  //
  // Finish MUST be called before calling Verify.
  Status Verify(std::span<const std::byte> checksum) const;

 protected:
  // A derived class provides a std::span of its state buffer.
  constexpr ChecksumAlgorithm(std::span<const std::byte> state)
      : state_(state) {}

  // Protected destructor prevents deleting ChecksumAlgorithms from the base
  // class, so that it is safe to have a non-virtual destructor.
  ~ChecksumAlgorithm() = default;

  // Returns the current checksum state.
  constexpr std::span<const std::byte> state() const { return state_; }

 private:
  // Checksums that require finalizing operations may override this method.
  virtual void Finalize() {}

  std::span<const std::byte> state_;
};

// A checksum algorithm for which Verify always passes. This can be used to
// disable checksum verification for a particular entry format.
class IgnoreChecksum final : public ChecksumAlgorithm {
 public:
  constexpr IgnoreChecksum() : ChecksumAlgorithm({}) {}

  void Reset() override {}
  void Update(std::span<const std::byte>) override {}
};

// Calculates a checksum in kAlignmentBytes chunks. Checksum classes can inherit
// from this and implement UpdateAligned and FinalizeAligned instead of Update
// and Finalize.
template <size_t kAlignmentBytes, size_t kBufferSize = kAlignmentBytes>
class AlignedChecksum : public ChecksumAlgorithm {
 public:
  void Update(std::span<const std::byte> data) final { writer_.Write(data); }

 protected:
  constexpr AlignedChecksum(std::span<const std::byte> state)
      : ChecksumAlgorithm(state),
        output_(this),
        writer_(kAlignmentBytes, output_) {}

  ~AlignedChecksum() = default;

 private:
  static_assert(kBufferSize >= kAlignmentBytes);

  void Finalize() final {
    writer_.Flush();
    FinalizeAligned();
  }

  virtual void UpdateAligned(std::span<const std::byte> data) = 0;

  virtual void FinalizeAligned() = 0;

  OutputToMethod<&AlignedChecksum::UpdateAligned> output_;
  AlignedWriterBuffer<kBufferSize> writer_;
};

}  // namespace pw::kvs
