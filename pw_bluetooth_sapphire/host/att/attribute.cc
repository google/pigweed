// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/att/attribute.h"

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
  BT_DEBUG_ASSERT(group_);
  BT_DEBUG_ASSERT(is_initialized());
}

Attribute::Attribute() : handle_(kInvalidHandle) {}

void Attribute::SetValue(const ByteBuffer& value) {
  BT_DEBUG_ASSERT(value.size());
  BT_DEBUG_ASSERT(value.size() <= kMaxAttributeValueLength);
  BT_DEBUG_ASSERT(!write_reqs_.allowed());
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
  BT_DEBUG_ASSERT(start_handle_ != kInvalidHandle);
  BT_DEBUG_ASSERT(decl_value.size());

  // It is a programmer error to provide an attr_count which overflows a handle
  // - this is why the below static cast is OK.
  BT_ASSERT(kHandleMax - start_handle >= attr_count);
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

  BT_DEBUG_ASSERT(attributes_[attributes_.size() - 1].handle() < end_handle_);

  // Groupings may not exceed kHandleMax attributes, so if we are incomplete per
  // the `complete()` check, we necessarily have < kHandleMax attributes. Thus
  // it is safe to cast attributes_.size() into a Handle.
  BT_ASSERT(attributes_.size() < kHandleMax - start_handle_);
  Handle handle = start_handle_ + static_cast<Handle>(attributes_.size());
  attributes_.push_back(Attribute(this, handle, type, read_reqs, write_reqs));

  return &attributes_[handle - start_handle_];
}

}  // namespace bt::att
