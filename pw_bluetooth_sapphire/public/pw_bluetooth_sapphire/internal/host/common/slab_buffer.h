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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_BUFFER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_BUFFER_H_

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"

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
  void Fill(uint8_t value) override {
    buffer_.mutable_view(0, size_).Fill(value);
  }

 private:
  size_t size_;

  // The backing backing buffer can have a different size from what was
  // requested.
  StaticByteBuffer<BackingBufferSize> buffer_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SlabBuffer);
};

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_BUFFER_H_
