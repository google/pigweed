// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_ADVERTISING_DATA_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_ADVERTISING_DATA_H_

#include <lib/fit/function.h>
#include <lib/fit/result.h>

#include <cstddef>
#include <limits>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/supplement_data.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"

namespace bt {

// Potential values that can be provided in the "Flags" advertising data
// bitfield.
// clang-format off
enum AdvFlag : uint8_t {
  // Octet 0
  kLELimitedDiscoverableMode        = (1 << 0),
  kLEGeneralDiscoverableMode        = (1 << 1),
  kBREDRNotSupported                = (1 << 2),
  kSimultaneousLEAndBREDRController = (1 << 3),
  kSimultaneousLEAndBREDRHost       = (1 << 4),
};
// clang-format on

// The Flags bitfield used in advertising data.
// Only the first octet (octet0) is represented in |AdvFlags|.
//
// See the Core Specification Supplement v9 for more information.
using AdvFlags = uint8_t;

constexpr uint8_t kDefaultNoAdvFlags = 0;

// The TLV size of the Flags datatype.
constexpr size_t kTLVFlagsSize = 3;

// The TLV size of the TX power data type
constexpr size_t kTLVTxPowerLevelSize = 3;

// The TLV size of the appearance data type
constexpr size_t kTLVAppearanceSize = 4;

// Constants for the expected size (in octets) of an
// advertising/EIR/scan-response data field.
//
//  * If a constant contains the word "Min", then it specifies a minimum
//    expected length rather than an exact length.
//
//  * If a constants contains the word "ElemSize", then the data field is
//    expected to contain a contiguous array of elements of the specified size.
constexpr size_t kAppearanceSize = 2;
constexpr size_t kManufacturerIdSize = 2;
constexpr size_t kTxPowerLevelSize = 1;

constexpr size_t kFlagsSizeMin = 1;
constexpr size_t kManufacturerSpecificDataSizeMin = kManufacturerIdSize;

constexpr uint8_t kMaxUint8 = std::numeric_limits<uint8_t>::max();
// The maximum length of a friendly name, derived from v5.2, Vol 4, Part E, 7.3.11 and Vol 3, Part
// C, 12.1
constexpr uint8_t kMaxNameLength = 248;

// The length of the entire manufacturer-specific data field must fit in a uint8_t, so the maximum
// data length is uint8_t::MAX - 1 byte for type - 2 bytes for manufacturer ID.
constexpr uint8_t kMaxManufacturerDataLength = kMaxUint8 - 3;

// The length of the service data field must fit in a uint8_t, so uint8_t::MAX - 1 byte for type.
constexpr uint8_t kMaxEncodedServiceDataLength = kMaxUint8 - 1;

// The length of an encoded URI together with its 1-byte type field must not exceed uint8_t limits
constexpr uint8_t kMaxEncodedUriLength = kMaxUint8 - 1;

// "A packet or data block shall not contain more than one instance for each Service UUID data
// size." (Core Specification Supplement v9 Part A 1.1.1). For each UUID size, 1 (type byte) + # of
// UUIDs * UUID size = length of that size's encoded UUIDs. This length must fit in a uint8_t, hence
// there is a per-UUID-size limit on the # of UUIDs.
constexpr uint8_t kMax16BitUuids = (kMaxUint8 - 1) / UUIDElemSize::k16Bit;
constexpr uint8_t kMax32BitUuids = (kMaxUint8 - 1) / UUIDElemSize::k32Bit;
constexpr uint8_t kMax128BitUuids = (kMaxUint8 - 1) / UUIDElemSize::k128Bit;

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
  // Possible failure modes for parsing an AdvertisingData from raw bytes.
  enum class ParseError {
    // The bytes provided are not a valid type-length-value container.
    kInvalidTlvFormat,
    // The length of a TxPowerLevel-type field does not match the TxPowerLevel value size.
    kTxPowerLevelMalformed,
    // The length of a LocalName-type field exceeds the length allowed by the spec (kMaxNameLength).
    kLocalNameTooLong,
    // A UUID-type field is malformed.
    kUuidsMalformed,
    // A ManufacturerSpecificData-type field is smaller than the minimum allowable length.
    kManufacturerSpecificDataTooSmall,
    // A ServiceData-type field is too small to fit its expected UUID size.
    kServiceDataTooSmall,
    // A UUID associated with a ServiceData-type field is malformed.
    kServiceDataUuidMalformed,
    // The length of an Appearance-type field does not match the Appearance value size.
    kAppearanceMalformed,
  };

  // Both complete and shortened forms of the local name can be advertised.
  struct LocalName {
    std::string name;
    bool is_complete;

    bool operator==(const LocalName& other) const {
      return (name == other.name) && (is_complete == other.is_complete);
    }
    bool operator!=(const LocalName& other) const { return !(*this == other); }
  };

  // Creates an empty advertising data.
  AdvertisingData() = default;
  ~AdvertisingData() = default;

  // When these move operations return, `other` is specified to be an empty AdvertisingData - i.e.
  // `other`'s state is as if `other = AdvertisingData()` was performed.
  AdvertisingData(AdvertisingData&& other) noexcept;
  AdvertisingData& operator=(AdvertisingData&& other) noexcept;

  // Construct from the raw Bluetooth field block |data|. Returns std::nullopt if |data| is not
  // formatted correctly or on a parsing error.
  using ParseResult = fit::result<ParseError, AdvertisingData>;
  static ParseResult FromBytes(const ByteBuffer& data);
  static std::string ParseErrorToString(ParseError e);

  // Copies all of the data in this object to |out|, including making a copy of
  // any data in manufacturing data or service data.
  // Overwrites any data which is already in |out|.
  void Copy(AdvertisingData* out) const;

  // Add a UUID to the set of services advertised.
  // These service UUIDs will automatically be compressed to be represented in the smallest space
  // possible. Returns true if the Service UUID was successfully added or already existed in the set
  // of advertised services, or false if the UUID set was full and `uuid` could not be added.
  [[nodiscard]] bool AddServiceUuid(const UUID& uuid);

  // Get the service UUIDs represented in this advertisement.
  std::unordered_set<UUID> service_uuids() const;

  // Set service data for the service specified by |uuid|. Returns true if the data was set, false
  // otherwise. Failure occurs if |uuid| + |data| exceed kMaxEncodedServiceDataLength when encoded.
  [[nodiscard]] bool SetServiceData(const UUID& uuid, const ByteBuffer& data);

  // Get a set of which UUIDs have service data in this advertisement.
  std::unordered_set<UUID> service_data_uuids() const;

  // View the currently set service data for |uuid|.
  // This view is not stable; it should be used only ephemerally.
  // Returns an empty BufferView if no service data is set for |uuid|
  BufferView service_data(const UUID& uuid) const;

  // Set Manufacturer specific data for the company identified by |company_id|. Returns false & does
  // not set the data if |data|.size() exceeds kMaxManufacturerDataLength, otherwise returns true.
  [[nodiscard]] bool SetManufacturerData(uint16_t company_id, const BufferView& data);

  // Get a set of which IDs have manufacturer data in this advertisement.
  std::unordered_set<uint16_t> manufacturer_data_ids() const;

  // View the currently set manufacturer data for the company |company_id|.
  // Returns an empty BufferView if no manufacturer data is set for
  // |company_id|.
  // NOTE: it is valid to send a manufacturer data with no data. Check that one
  // exists using manufacturer_data_ids() first.
  // This view is not stable; it should be used only ephemerally.
  BufferView manufacturer_data(uint16_t company_id) const;

  // Sets the local TX Power
  // TODO(jamuraa): add documentation about where to get this number from
  void SetTxPower(int8_t dbm);

  // Gets the TX power
  std::optional<int8_t> tx_power() const;

  // Returns false if `name` is not set, which happens if `name` is shortened and a complete name is
  // currently set, or if `name` exceeds kMaxLocalName bytes. Returns true if `name` is set.
  [[nodiscard]] bool SetLocalName(const LocalName& local_name);
  [[nodiscard]] bool SetLocalName(const std::string& name, bool is_complete = true) {
    return SetLocalName(LocalName{name, is_complete});
  }

  // Gets the local name
  std::optional<LocalName> local_name() const;

  // Adds a URI to the set of URIs advertised.
  // Does nothing if |uri| is empty or, when encoded, exceeds kMaxEncodedUriLength.
  [[nodiscard]] bool AddUri(const std::string& uri);

  // Get the URIs in this advertisement
  const std::unordered_set<std::string>& uris() const;

  // Sets the appearance
  void SetAppearance(uint16_t appearance);

  // Get the appearance
  std::optional<uint16_t> appearance() const;

  // Calculates the size of the current set of fields if they were to be written
  // to a buffer using WriteBlock().
  //
  // If |include_flags| is set, then the returned block size will include the
  // expected size of writing advertising data flags.
  size_t CalculateBlockSize(bool include_flags = false) const;

  // Writes the byte representation of this to |buffer| with the included |flags|.
  // Returns false without modifying |buffer| if there is not enough space (i.e If
  // the buffer size is less than CalculateBlockSize()).
  //
  // The responsibility is on the caller to provide a buffer that is large enough to
  // encode the |AdvertisingData| and the optional flags.
  bool WriteBlock(MutableByteBuffer* buffer, std::optional<AdvFlags> flags) const;

  // Relation operators
  bool operator==(const AdvertisingData& other) const;
  bool operator!=(const AdvertisingData& other) const;

 private:
  // This class enforces that a set of UUIDs does not grow beyond its provided upper bound.
  class BoundedUuids {
   public:
    // `bound` is the maximum number of UUIDs allowed in this set.
    explicit BoundedUuids(uint8_t bound) : bound_(bound) {}

    // Adds a UUID to the set. Returns false if the UUID couldn't be added to the set due to the
    // size bound, true otherwise.
    bool AddUuid(UUID uuid);

    const std::unordered_set<UUID>& set() const { return set_; }
    bool operator==(const BoundedUuids& other) const {
      return bound_ == other.bound_ && set_ == other.set_;
    }
    bool operator!=(const BoundedUuids& other) const { return !(*this == other); }

   private:
    std::unordered_set<UUID> set_ = std::unordered_set<UUID>{};
    uint8_t bound_;
  };

  // AD stores a map from service UUID size -> BoundedUuids. As the number of allowed UUID sizes is
  // static, we define this default variable to represent the "empty" state of the UUID set.
  const std::unordered_map<UUIDElemSize, BoundedUuids> kEmptyServiceUuidMap = {
      {UUIDElemSize::k16Bit, BoundedUuids(kMax16BitUuids)},
      {UUIDElemSize::k32Bit, BoundedUuids(kMax32BitUuids)},
      {UUIDElemSize::k128Bit, BoundedUuids(kMax128BitUuids)}};
  // TODO(armansito): Consider storing the payload in its serialized form and
  // have these point into the structure (see fxbug.dev/907).
  std::optional<LocalName> local_name_;
  std::optional<int8_t> tx_power_;
  std::optional<uint16_t> appearance_;

  // Each service UUID size is associated with a BoundedUuids. The BoundedUuids invariant that
  // |bound| >= |set|.size() field is maintained by the AD class, not the BoundedUuids struct.
  std::unordered_map<UUIDElemSize, BoundedUuids> service_uuids_ = kEmptyServiceUuidMap;

  // The length of each manufacturer data buffer is always <= kMaxManufacturerDataLength.
  std::unordered_map<uint16_t, DynamicByteBuffer> manufacturer_data_;

  // For each element in `service_data_`, the compact size of the UUID + the buffer length is always
  // <= kkMaxEncodedServiceDataLength
  std::unordered_map<UUID, DynamicByteBuffer> service_data_;

  std::unordered_set<std::string> uris_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AdvertisingData);
};

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_ADVERTISING_DATA_H_
