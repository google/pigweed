// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_BUFFER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_BUFFER_H_

#include <fbl/slab_allocator.h>
#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/slab_allocator_traits.h"
#include "lib/fxl/macros.h"

namespace btlib {
namespace common {

template <size_t BackingBufferSize>
class SlabBuffer : public MutableByteBuffer {
 public:
  explicit SlabBuffer(size_t size) : size_(size) {
    ZX_DEBUG_ASSERT(size);
    ZX_DEBUG_ASSERT(size_ <= buffer_.size());
  }

  // ByteBuffer overrides:
  const uint8_t* data() const override { return buffer_.data(); }
  size_t size() const override { return size_; }
  const_iterator cbegin() const override { return buffer_.cbegin(); }
  const_iterator cend() const override { return cbegin() + size_; }

  // MutableByteBuffer overrides:
  uint8_t* mutable_data() override { return buffer_.mutable_data(); }
  void Fill(uint8_t value) override {
    buffer_.mutable_view(0, size_).Fill(value);
  }

 private:
  size_t size_;

  // The backing backing buffer can have a different size from what was
  // requested.
  common::StaticByteBuffer<BackingBufferSize> buffer_;

  FXL_DISALLOW_COPY_AND_ASSIGN(SlabBuffer);
};

namespace internal {

template <size_t BufferSize, size_t NumBuffers>
class SlabBufferImpl;

}  // namespace internal

template <size_t BufferSize, size_t NumBuffers>
using SlabBufferTraits =
    SlabAllocatorTraits<internal::SlabBufferImpl<BufferSize, NumBuffers>,
                        sizeof(SlabBuffer<BufferSize>),
                        NumBuffers>;

namespace internal {

template <size_t BufferSize, size_t NumBuffers>
class SlabBufferImpl
    : public SlabBuffer<BufferSize>,
      public fbl::SlabAllocated<SlabBufferTraits<BufferSize, NumBuffers>> {
 public:
  explicit SlabBufferImpl(size_t size) : SlabBuffer<BufferSize>(size) {}

 private:
  FXL_DISALLOW_COPY_AND_ASSIGN(SlabBufferImpl);
};

}  // namespace internal

}  // namespace common
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_BUFFER_H_
