// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_ATTRIBUTE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_ATTRIBUTE_H_

#include <memory>
#include <vector>

#include <lib/fit/function.h>
#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "lib/fxl/macros.h"

namespace btlib {
namespace att {

// Identifier type used to identify a peer device.
using DeviceId = common::DeviceId;

// Defines the read or write access permissions for an attribute.
class AccessRequirements final {
 public:
  // The default access permission prevents the attribute from being accessed.
  AccessRequirements();

  // Enables access permission with the given requirements.
  AccessRequirements(bool encryption, bool authentication, bool authorization);

  // Returns true if this attribute can be accessed at all.
  inline bool allowed() const {
    return value_ & kAttributePermissionBitAllowed;
  }

  // Returns true if no security is required.
  inline bool allowed_without_security() const {
    return value_ == kAttributePermissionBitAllowed;
  }

  // The following getters return the security requirements of this attribute:

  inline bool encryption_required() const {
    return value_ & kAttributePermissionBitEncryptionRequired;
  }

  inline bool authentication_required() const {
    return value_ & kAttributePermissionBitAuthenticationRequired;
  }

  inline bool authorization_required() const {
    return value_ & kAttributePermissionBitAuthorizationRequired;
  }

  inline bool operator==(const AccessRequirements& other) const {
    return value_ == other.value_;
  }

 private:
  uint8_t value_;
};

class AttributeGrouping;

// Represents an attribute. Each attribute is assigned a handle (unique within
// the scope of an Adapter) and a UUID that identifies its type. The type of an
// attribute dictates how to interpret the attribute value.
//
// Each attribute has a value of up to 512 octets. An Attribute can be
// configured to have a static value. In such a case the value can be directly
// obtained by calling |value()|. Such attributes cannot be written to.
//
// Otherwise, an Attribute is considered dynamic and its value must be accessed
// asynchronously by calling ReadAsync()/WriteAsync().
//
// Instances cannot be constructed directly and must be obtained from an
// AttributeGrouping.
//
// THREAD-SAFETY:
//
// This class is not thread-safe. The constructor/destructor and all public
// methods must be called on the same thread.
class Attribute final {
 public:
  // Allow move construction and assignment.
  Attribute(Attribute&& other) = default;
  Attribute& operator=(Attribute&& other) = default;

  bool is_initialized() const { return handle_ != kInvalidHandle; }

  Handle handle() const { return handle_; }
  const common::UUID& type() const { return type_; }

  // The grouping that this attribute belongs to.
  const AttributeGrouping& group() const { return *group_; }

  // Returns the current attribute value. Returns nullptr if the attribute has a
  // dynamic value.
  const common::ByteBuffer* value() const {
    return value_.size() ? &value_ : nullptr;
  }

  // The read/write permissions of this attribute.
  const AccessRequirements& read_reqs() const { return read_reqs_; }
  const AccessRequirements& write_reqs() const { return write_reqs_; }

  // Sets |value| as the static attribute value. Once a value is assigned it
  // cannot be overwritten. A static value cannot be assigned to an attribute
  // that permits writes as attribute writes need to propagate to the service
  // layer.
  void SetValue(const common::ByteBuffer& value);

  // Handlers for reading and writing and attribute value asynchronously. A
  // handler must call the provided the |result_callback| to signal the end of
  // the operation.
  using ReadResultCallback =
      fit::function<void(ErrorCode status, const common::ByteBuffer& value)>;
  using ReadHandler =
      fit::function<void(DeviceId peer_id, Handle handle, uint16_t offset,
                         ReadResultCallback result_callback)>;
  void set_read_handler(ReadHandler read_handler) {
    read_handler_ = std::move(read_handler);
  }

  // An "ATT Write Command" will trigger WriteHandler with
  // a null |result_callback|
  using WriteResultCallback = fit::function<void(ErrorCode status)>;
  using WriteHandler = fit::function<void(
      DeviceId peer_id, Handle handle, uint16_t offset,
      const common::ByteBuffer& value, WriteResultCallback result_callback)>;
  void set_write_handler(WriteHandler write_handler) {
    write_handler_ = std::move(write_handler);
  }

  // Initiates an asynchronous read of the attribute value. Returns false if
  // this attribute is not dynamic.
  bool ReadAsync(DeviceId peer_id, uint16_t offset,
                 ReadResultCallback result_callback) const;

  // Initiates an asynchronous write of the attribute value. Returns false if
  // this attribute is not dynamic.
  bool WriteAsync(DeviceId peer_id, uint16_t offset,
                  const common::ByteBuffer& value,
                  WriteResultCallback result_callback) const;

 private:
  // Only an AttributeGrouping can construct this.
  friend class AttributeGrouping;

  // The default constructor will construct this attribute as uninitialized.
  // This is intended for STL containers.
  Attribute();
  Attribute(AttributeGrouping* group, Handle handle, const common::UUID& type,
            const AccessRequirements& read_reqs,
            const AccessRequirements& write_reqs);

  AttributeGrouping* group_;  // The group that owns this Attribute.
  Handle handle_;
  common::UUID type_;
  AccessRequirements read_reqs_;
  AccessRequirements write_reqs_;

  ReadHandler read_handler_;
  WriteHandler write_handler_;

  common::DynamicByteBuffer value_;

  FXL_DISALLOW_COPY_AND_ASSIGN(Attribute);
};

// Represents a grouping of attributes (see Vol 3, Part F, 3.2.3). Each grouping
// contains at least one leading attribute that contains the group declaration.
// The type of this attribute dictates the type of the grouping.
//
// Each grouping covers a contiguous range of handle numbers. The size of the
// range is determined by the |attr_count| constructor argument which defines
// the number of attributes in the grouping following the declaration attribute.
// Once constructed, a grouping is not considered complete until available
// handles within the range have been populated.
class AttributeGrouping final {
 public:
  // Initializes this attribute grouping with a group declaration attribute and
  // enough storage for |attr_count| additional attributes. |decl_value| is
  // assigned as the read-only value of the declaration attribute.
  //
  // Note: |attr_count| should not cause the group end handle to exceed
  // att::kHandleMax.
  AttributeGrouping(const common::UUID& group_type, Handle start_handle,
                    size_t attr_count, const common::ByteBuffer& decl_value);

  // Inserts a new attribute into this grouping using the given parameters and
  // returns a pointer to it. Returns nullptr if the grouping is out of handles
  // to allocate.
  //
  // The caller should not hold on to the returned pointer as the Attribute
  // object is owned and managed by this AttributeGrouping.
  Attribute* AddAttribute(
      const common::UUID& type,
      const AccessRequirements& read_reqs = AccessRequirements(),
      const AccessRequirements& write_reqs = AccessRequirements());

  // Returns true if all attributes of this grouping have been populated.
  bool complete() const {
    return attributes_.size() == (end_handle_ - start_handle_ + 1);
  }

  const common::UUID& group_type() const {
    ZX_DEBUG_ASSERT(!attributes_.empty());
    return attributes_[0].type();
  }

  // Value of the group declaration attribute.
  const common::BufferView decl_value() const {
    ZX_DEBUG_ASSERT(!attributes_.empty());
    ZX_DEBUG_ASSERT(attributes_[0].value());
    return attributes_[0].value()->view();
  }

  // The start and end handles of this grouping (inclusive).
  Handle start_handle() const { return start_handle_; }
  Handle end_handle() const { return end_handle_; }

  bool active() const { return active_; }
  void set_active(bool active) {
    ZX_DEBUG_ASSERT_MSG(complete(),
                        "set_active() called on incomplete grouping!");
    active_ = active;
  }

  const std::vector<Attribute>& attributes() const { return attributes_; }

 private:
  Handle start_handle_;
  Handle end_handle_;

  // Only groupings that are active are considered when responding to ATT
  // requests.
  bool active_;

  // The Attributes in this grouping, including the declaration attribute. Space
  // is reserved for all attributes upon construction. The number of elements in
  // |attributes_| reflects how many of the attributes have been initialized.
  std::vector<Attribute> attributes_;

  FXL_DISALLOW_COPY_AND_ASSIGN(AttributeGrouping);
};

}  // namespace att
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_ATTRIBUTE_H_
