// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_DATABASE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_DATABASE_H_

#include <list>
#include <memory>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/att/attribute.h"
#include "src/connectivity/bluetooth/core/bt-host/att/write_queue.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/lib/fxl/memory/ref_counted.h"

namespace bt::att {

// This class provides a simple attribute database abstraction. Attributes can
// be populated directly and queried to fulfill ATT protocol requests.
//
// Many Database instances can be created as long as care is taken that the
// referenced handle ranges are distinct. While this class is primarily intended
// to be used as a local ATT server database, it could also be used to represent
// a remote attribute cache.
//
// THREAD-SAFETY:
//
// This class is not thread-safe. The constructor/destructor and all public
// methods must be called on the same thread.
class Database final : public fxl::RefCountedThreadSafe<Database> {
  using GroupingList = std::list<AttributeGrouping>;

 public:
  // This type allows iteration over the attributes in a database. An iterator
  // is always initialzed with a handle range and options to skip attributes or
  // groupings based on attribute type. An iterator always skips
  // inactive/incomplete groupings.
  //
  // Modifying a database invalidates its iterators.
  class Iterator final {
   public:
    // Returns the current attribute. Returns nullptr if the end of the handle
    // range has been reached.
    const Attribute* get() const;

    // Advances the iterator forward. Skips over non-matching attributes if a
    // type filter has been set. Has no effect if the end of the range was
    // reached.
    void Advance();

    // If set, |next()| will only return attributes with the given |type|. No
    // filter is set by default.
    void set_type_filter(const UUID& type) { type_filter_ = type; }

    // Returns true if the iterator cannot be advanced any further.
    inline bool AtEnd() const { return grp_iter_ == grp_end_; }

   private:
    inline void MarkEnd() { grp_iter_ = grp_end_; }

    friend class Database;
    Iterator(GroupingList* list, Handle start, Handle end, const UUID* type, bool groups_only);

    Handle start_;
    Handle end_;
    bool grp_only_;
    GroupingList::iterator grp_end_;
    GroupingList::iterator grp_iter_;
    uint8_t attr_offset_;
    std::optional<UUID> type_filter_;
  };

  // Initializes this database to span the attribute handle range given by
  // |range_start| and |range_end|. This allows the upper layer to segment the
  // handle range into multiple contiguous regions by instantiating multiple
  // Database objects.
  //
  // Note: This is to make it easy for the GATT layer to group service
  // declarations with 16-bit UUIDs and 128-bit UUIDs separately as recommended
  // by the GATT specification (see Vol 3, Part G, 3.1). By default
  inline static fxl::RefPtr<Database> Create(Handle range_start = kHandleMin,
                                             Handle range_end = kHandleMax) {
    return fxl::AdoptRef(new Database(range_start, range_end));
  }

  // Returns an iterator that covers the handle range defined by |start| and
  // |end| (inclusive). If |groups_only| is true, then the returned iterator
  // will only return group declaration attributes (this allows quicker
  // iteration over groupings while handling the ATT Read By Group Type
  // request).
  //
  // If |type| is not a nullptr, it will be assigned as the iterator's type
  // filter.
  Iterator GetIterator(Handle start, Handle end, const UUID* type = nullptr,
                       bool groups_only = false);

  // Creates a new attribute grouping with the given |type|. The grouping will
  // be initialized to contain |attr_count| attributes (excluding the
  // group declaration attribute) and |value| will be assigned as the group
  // declaration attribute value.
  //
  // Returns a pointer to the new grouping, which can be used to populate
  // attributes. Returns nullptr if the requested grouping could not be
  // created due to insufficient handles.
  //
  // The returned pointer is owned and managed by this Database and should not
  // be retained by the caller. Removing the grouping will invalidate the
  // returned pointer.
  AttributeGrouping* NewGrouping(const UUID& group_type, size_t attr_count,
                                 const ByteBuffer& decl_value);

  // Removes the attribute grouping that has the given starting handle. Returns
  // false if no such grouping was found.
  bool RemoveGrouping(Handle start_handle);

  const std::list<AttributeGrouping>& groupings() const { return groupings_; }

  // Finds and returns the attribute with the given handle. Returns nullptr if
  // the attribute cannot be found or is part of a grouping that is inactive
  // or incomplete.
  const Attribute* FindAttribute(Handle handle);

  // Applies all write requests in |write_queue| and reports the result in
  // |callback|. All requests will be delivered to the attribute write handlers
  // at once, in order, without waiting for a response on individual writes.
  //
  // If one or more requests result in an application protocol error,
  // |callback| will be invoked with the value of the first error received and
  // all subsequent results will be ignored. Otherwise, |callback| will be
  // called with a success status once a reply has been received for all
  // requests in the queue.
  //
  // This function performs any security checks even though these are expected
  // to be done during the "prepare" phase. This is because the attribute
  // database can change from the time writes are queued until they are
  // committed. |security| should match the security properties of the link
  // under which the execute write request was initiatied.
  //
  // Attribute value validation (such as offset and length checks) are expected
  // to be done by the application that has set up the attribute. This function
  // does check for the validity of a given attribute handle and whether the
  // attribute supports writes.
  //
  // The Handle argument of |callback| is undefined if ErrorCode is
  // ErrorCode::kNoError and should be ignored.
  using WriteCallback = fit::function<void(Handle, ErrorCode)>;
  void ExecuteWriteQueue(PeerId peer_id, PrepareWriteQueue write_queue,
                         const sm::SecurityProperties& security, WriteCallback callback);

 private:
  FRIEND_REF_COUNTED_THREAD_SAFE(Database);

  Database(Handle range_start, Handle range_end);
  ~Database() = default;

  Handle range_start_;
  Handle range_end_;

  // The list of groupings is sorted by handle where each grouping maps to a
  // non-overlapping handle range. Successive groupings don't necessarily
  // represent contiguous handle ranges as any grouping can be removed.
  //
  // Note: This uses a std::list because std::lower_bound doesn't work with a
  // LinkedList (aka fbl::DoublyLinkedList). This is only marginally
  // less space efficient.
  GroupingList groupings_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Database);
};

}  // namespace bt::att

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_DATABASE_H_
