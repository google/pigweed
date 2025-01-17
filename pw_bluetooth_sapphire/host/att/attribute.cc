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

#include "pw_bluetooth_sapphire/internal/host/att/attribute.h"

#include <pw_assert/check.h>

namespace bt::att {

AccessRequirements::AccessRequirements() : value_(0u), min_enc_key_size_(0u) {}

AccessRequirements::AccessRequirements(bool encryption,
                                       bool authentication,
                                       bool authorization,
                                       uint8_t min_enc_key_size)
    : value_(kAttributePermissionBitAllowed),
      min_enc_key_size_(min_enc_key_size) {
  if (encryption) {
    value_ |= kAttributePermissionBitEncryptionRequired;
  }
  if (authentication) {
    value_ |= kAttributePermissionBitAuthenticationRequired;
  }
  if (authorization) {
    value_ |= kAttributePermissionBitAuthorizationRequired;
  }
}

Attribute::Attribute(AttributeGrouping* group,
                     Handle handle,
                     const UUID& type,
                     const AccessRequirements& read_reqs,
                     const AccessRequirements& write_reqs)
    : group_(group),
      handle_(handle),
      type_(type),
      read_reqs_(read_reqs),
      write_reqs_(write_reqs) {
  PW_DCHECK(group_);
  PW_DCHECK(is_initialized());
}

Attribute::Attribute() : handle_(kInvalidHandle) {}

void Attribute::SetValue(const ByteBuffer& value) {
  PW_DCHECK(value.size());
  PW_DCHECK(value.size() <= kMaxAttributeValueLength);
  PW_DCHECK(!write_reqs_.allowed());
  value_ = DynamicByteBuffer(value);
}

bool Attribute::ReadAsync(PeerId peer_id,
                          uint16_t offset,
                          ReadResultCallback result_callback) const {
  if (!is_initialized() || !read_handler_)
    return false;

  if (!read_reqs_.allowed())
    return false;

  read_handler_(peer_id, handle_, offset, std::move(result_callback));
  return true;
}

bool Attribute::WriteAsync(PeerId peer_id,
                           uint16_t offset,
                           const ByteBuffer& value,
                           WriteResultCallback result_callback) const {
  if (!is_initialized() || !write_handler_)
    return false;

  if (!write_reqs_.allowed())
    return false;

  write_handler_(peer_id, handle_, offset, value, std::move(result_callback));
  return true;
}

AttributeGrouping::AttributeGrouping(const UUID& group_type,
                                     Handle start_handle,
                                     size_t attr_count,
                                     const ByteBuffer& decl_value)
    : start_handle_(start_handle), active_(false) {
  PW_DCHECK(start_handle_ != kInvalidHandle);
  PW_DCHECK(decl_value.size());

  // It is a programmer error to provide an attr_count which overflows a handle
  // - this is why the below static cast is OK.
  PW_CHECK(kHandleMax - start_handle >= attr_count);
  auto handle_attr_count = static_cast<Handle>(attr_count);

  end_handle_ = start_handle + handle_attr_count;
  attributes_.reserve(handle_attr_count + 1);

  // TODO(armansito): Allow callers to require at most encryption.
  attributes_.push_back(Attribute(
      this,
      start_handle,
      group_type,
      AccessRequirements(/*encryption=*/false,
                         /*authentication=*/false,
                         /*authorization=*/false),  // read allowed, no security
      AccessRequirements()));                       // write disallowed

  attributes_[0].SetValue(decl_value);
}

Attribute* AttributeGrouping::AddAttribute(
    const UUID& type,
    const AccessRequirements& read_reqs,
    const AccessRequirements& write_reqs) {
  if (complete())
    return nullptr;

  PW_DCHECK(attributes_[attributes_.size() - 1].handle() < end_handle_);

  // Groupings may not exceed kHandleMax attributes, so if we are incomplete per
  // the `complete()` check, we necessarily have < kHandleMax attributes. Thus
  // it is safe to cast attributes_.size() into a Handle.
  PW_CHECK(attributes_.size() < kHandleMax - start_handle_);
  Handle handle = start_handle_ + static_cast<Handle>(attributes_.size());
  attributes_.push_back(Attribute(this, handle, type, read_reqs, write_reqs));

  return &attributes_[handle - start_handle_];
}

}  // namespace bt::att
