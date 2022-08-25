// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_BUFFER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_BUFFER_H_

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"

namespace bt {

template <size_t BackingBufferSize>
class SlabBuffer : public MutableByteBuffer {
 public:
  explicit SlabBuffer(size_t size) : size_(size) {
    BT_ASSERT(size);
    BT_ASSERT(size_ <= buffer_.size());
  }

  // ByteBuffer overrides:
  const uint8_t* data() const override { return buffer_.data(); }
  size_t size() const override { return size_; }
  const_iterator cbegin() const override { return buffer_.cbegin(); }
  const_iterator cend() const override { return cbegin() + size_; }

  // MutableByteBuffer overrides:
  uint8_t* mutable_data() override { return buffer_.mutable_data(); }
  void Fill(uint8_t value) override { buffer_.mutable_view(0, size_).Fill(value); }

 private:
  size_t size_;

  // The backing backing buffer can have a different size from what was
  // requested.
  StaticByteBuffer<BackingBufferSize> buffer_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SlabBuffer);
};

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_BUFFER_H_
