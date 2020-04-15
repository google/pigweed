// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_ADVERTISING_DATA_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_ADVERTISING_DATA_H_

#include <lib/fit/function.h>

#include <cstddef>
#include <unordered_map>
#include <unordered_set>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/supplement_data.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"

namespace bt {
namespace gap {

// A helper to build Adversiting Data, Scan Response Data, or Extended Inquiry
// Response Data fields.
// TODO(jamuraa): Add functionality for ACAD and OOB
//
// This can be viewed as a complex container type which has a specified byte
// view that is valid for:
//  - Core Spec v5.0 Vol 3, Part C, Section 11 in the case of Advertising or
//    Scan Response Data
//  - Core Spec v5.0 Vol 3, Part C, Section 8 for Extended Inquiry Response data
//
// See those sections, and the Core Specification Supplement v7 for more
// information.
class AdvertisingData {
 public:
  // Creates an empty advertising data.
  AdvertisingData() = default;
  ~AdvertisingData() = default;

  // Move constructor and assignment operator
  AdvertisingData(AdvertisingData&& other) = default;
  AdvertisingData& operator=(AdvertisingData&& other) = default;

  // Fill the AdvertisingData |out_ad| from the raw Bluetooth field block
  // |data|. Returns false if |data| is not formatted correctly or on a parsing
  // error, and true otherwise. |out_ad| is not guaranteed to be in any state
  // unless we return true. Does not clear |out_ad| so this function can be used
  // to merge multiple field blocks.
  static bool FromBytes(const ByteBuffer& data, AdvertisingData* out_ad);

  // Copies all of the data in this object to |out|, including making a copy of
  // any data in manufacturing data or service data.
  // Overwrites any data which is already in |out|.
  void Copy(AdvertisingData* out) const;

  // Add a UUID to the set of services advertised.
  // These service UUIDs will automatically be compressed to be represented in
  // the smallest space possible.
  void AddServiceUuid(const UUID& uuid);

  // Get the service UUIDs represented in this advertisement.
  const std::unordered_set<UUID>& service_uuids() const;

  // Set some service data for the service specified by |uuid|.
  void SetServiceData(const UUID& uuid, const ByteBuffer& data);

  // Get a set of which UUIDs have service data in this advertisement.
  const std::unordered_set<UUID> service_data_uuids() const;

  // View the currently set service data for |uuid|.
  // This view is not stable; it should be used only ephemerally.
  // Returns an empty BufferView if no service data is set for |uuid|
  const BufferView service_data(const UUID& uuid) const;

  // Set some Manufacturer specific data for the company identified by
  // |company_id|
  void SetManufacturerData(const uint16_t company_id, const BufferView& data);

  // Get a set of which IDs have manufacturer data in this advertisement.
  const std::unordered_set<uint16_t> manufacturer_data_ids() const;

  // View the currently set manufacturer data for the company |company_id|.
  // Returns an empty BufferView if no manufacturer data is set for
  // |company_id|.
  // NOTE: it is valid to send a manufacturer data with no data. Check that one
  // exists using manufacturer_data_ids() first.
  // This view is not stable; it should be used only ephemerally.
  const BufferView manufacturer_data(const uint16_t company_id) const;

  // Sets the local TX Power
  // TODO(jamuraa): add documentation about where to get this number from
  void SetTxPower(int8_t dbm);

  // Gets the TX power
  std::optional<int8_t> tx_power() const;

  // Sets the local name
  void SetLocalName(const std::string& name);

  // Gets the local name
  std::optional<std::string> local_name() const;

  // Adds a URI to the set of URIs advertised.
  // Does nothing if |uri| is empty.
  void AddURI(const std::string& uri);

  // Get the URIs in this advertisement
  const std::unordered_set<std::string>& uris() const;

  // Sets the appearance
  void SetAppearance(uint16_t appearance);

  // Get the appearance
  std::optional<uint16_t> appearance() const;

  // Calculates the size of the current set of fields if they were to be written
  // to a buffer using WriteBlock()
  size_t CalculateBlockSize() const;

  // Writes the byte representation of this to |buffer|.
  // Returns false without modifying |buffer| if there is not enough space
  // (if the buffer size is less than block_size())
  bool WriteBlock(MutableByteBuffer* buffer) const;

  // Relation operators
  bool operator==(const AdvertisingData& other) const;
  bool operator!=(const AdvertisingData& other) const;

 private:
  // TODO(armansito): Consider storing the payload in its serialized form and
  // have these point into the structure (see NET-209).
  std::optional<std::string> local_name_;
  std::optional<int8_t> tx_power_;
  std::optional<uint16_t> appearance_;
  std::unordered_set<UUID> service_uuids_;
  std::unordered_map<uint16_t, DynamicByteBuffer> manufacturer_data_;
  std::unordered_map<UUID, DynamicByteBuffer> service_data_;

  std::unordered_set<std::string> uris_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AdvertisingData);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_ADVERTISING_DATA_H_
