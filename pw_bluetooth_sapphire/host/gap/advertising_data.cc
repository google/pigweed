// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "advertising_data.h"

#include <endian.h>
#include <zircon/assert.h>

#include <type_traits>

#include "lib/fidl/cpp/type_converter.h"
#include "lib/fidl/cpp/vector.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/lib/fxl/strings/string_printf.h"
#include "src/lib/fxl/strings/utf_codecs.h"

// A partial fidl::TypeConverter template specialization for copying the
// contents of a type that derives from ByteBuffer into a
// fidl::VectorPtr<unsigned char>. If the input array is empty, the output array
// will be empty. Used by Vector<uint8_t>::From() in AsLEAdvertisingData()
template <typename T>
struct fidl::TypeConverter<fidl::VectorPtr<unsigned char>, T> {
  static fidl::VectorPtr<unsigned char> Convert(const T& input) {
    static_assert(std::is_base_of<bt::common::ByteBuffer, T>::value, "");

    fidl::VectorPtr<unsigned char> result =
        fidl::VectorPtr<unsigned char>::New(input.size());
    memcpy(result->data(), input.data(), input.size());
    return result;
  }
};

namespace ble = fuchsia::bluetooth::le;

namespace bt {
namespace gap {

using common::BufferView;
using common::ByteBuffer;
using common::DynamicByteBuffer;
using common::MutableByteBuffer;
using common::UUID;

namespace {

using UuidFunction = fit::function<void(const UUID&)>;

bool ParseUuids(const BufferView& data, size_t uuid_size, UuidFunction func) {
  ZX_DEBUG_ASSERT(func);
  ZX_DEBUG_ASSERT((uuid_size == k16BitUuidElemSize) ||
                  (uuid_size == k32BitUuidElemSize) ||
                  (uuid_size == k128BitUuidElemSize));

  if (data.size() % uuid_size) {
    bt_log(WARN, "gap-le", "malformed service UUIDs list");
    return false;
  }

  size_t uuid_count = data.size() / uuid_size;
  for (size_t i = 0; i < uuid_count; i++) {
    const BufferView uuid_bytes(data.data() + (i * uuid_size), uuid_size);
    UUID uuid;
    if (!UUID::FromBytes(uuid_bytes, &uuid))
      return false;

    func(uuid);
  }

  return true;
}

size_t SizeForType(DataType type) {
  switch (type) {
    case DataType::kIncomplete16BitServiceUuids:
    case DataType::kComplete16BitServiceUuids:
    case DataType::kServiceData16Bit:
      return k16BitUuidElemSize;
    case DataType::kIncomplete32BitServiceUuids:
    case DataType::kComplete32BitServiceUuids:
    case DataType::kServiceData32Bit:
      return k32BitUuidElemSize;
    case DataType::kIncomplete128BitServiceUuids:
    case DataType::kComplete128BitServiceUuids:
    case DataType::kServiceData128Bit:
      return k128BitUuidElemSize;
    default:
      break;
  };

  return 0;
}

// clang-format off
// https://www.bluetooth.com/specifications/assigned-numbers/uri-scheme-name-string-mapping
const char* kUriSchemes[] = {"aaa:", "aaas:", "about:", "acap:", "acct:", "cap:", "cid:",
        "coap:", "coaps:", "crid:", "data:", "dav:", "dict:", "dns:", "file:", "ftp:", "geo:",
        "go:", "gopher:", "h323:", "http:", "https:", "iax:", "icap:", "im:", "imap:", "info:",
        "ipp:", "ipps:", "iris:", "iris.beep:", "iris.xpc:", "iris.xpcs:", "iris.lwz:", "jabber:",
        "ldap:", "mailto:", "mid:", "msrp:", "msrps:", "mtqp:", "mupdate:", "news:", "nfs:", "ni:",
        "nih:", "nntp:", "opaquelocktoken:", "pop:", "pres:", "reload:", "rtsp:", "rtsps:", "rtspu:",
        "service:", "session:", "shttp:", "sieve:", "sip:", "sips:", "sms:", "snmp:", "soap.beep:",
        "soap.beeps:", "stun:", "stuns:", "tag:", "tel:", "telnet:", "tftp:", "thismessage:",
        "tn3270:", "tip:", "turn:", "turns:", "tv:", "urn:", "vemmi:", "ws:", "wss:", "xcon:",
        "xcon-userid:", "xmlrpc.beep:", "xmlrpc.beeps:", "xmpp:", "z39.50r:", "z39.50s:", "acr:",
        "adiumxtra:", "afp:", "afs:", "aim:", "apt:", "attachment:", "aw:", "barion:", "beshare:",
        "bitcoin:", "bolo:", "callto:", "chrome:", "chrome-extension:", "com-eventbrite-attendee:",
        "content:", "cvs:", "dlna-playsingle:", "dlna-playcontainer:", "dtn:", "dvb:", "ed2k:",
        "facetime:", "feed:", "feedready:", "finger:", "fish:", "gg:", "git:", "gizmoproject:",
        "gtalk:", "ham:", "hcp:", "icon:", "ipn:", "irc:", "irc6:", "ircs:", "itms:", "jar:",
        "jms:", "keyparc:", "lastfm:", "ldaps:", "magnet:", "maps:", "market:", "message:", "mms:",
        "ms-help:", "ms-settings-power:", "msnim:", "mumble:", "mvn:", "notes:", "oid:", "palm:",
        "paparazzi:", "pkcs11:", "platform:", "proxy:", "psyc:", "query:", "res:", "resource:",
        "rmi:", "rsync:", "rtmfp:", "rtmp:", "secondlife:", "sftp:", "sgn:", "skype:", "smb:",
        "smtp:", "soldat:", "spotify:", "ssh:", "steam:", "submit:", "svn:", "teamspeak:",
        "teliaeid:", "things:", "udp:", "unreal:", "ut2004:", "ventrilo:", "view-source:",
        "webcal:", "wtai:", "wyciwyg:", "xfire:", "xri:", "ymsgr:", "example:",
        "ms-settings-cloudstorage:"};
// clang-format on

const size_t kUriSchemesSize = std::extent<decltype(kUriSchemes)>::value;

std::string EncodeUri(const std::string& uri) {
  std::string encoded_scheme;
  for (size_t i = 0; i < kUriSchemesSize; i++) {
    const char* scheme = kUriSchemes[i];
    size_t scheme_len = strlen(scheme);
    if (std::equal(scheme, scheme + scheme_len, uri.begin())) {
      fxl::WriteUnicodeCharacter(i + 2, &encoded_scheme);
      return encoded_scheme + uri.substr(scheme_len);
    }
  }
  // First codepoint (U+0001) is for uncompressed schemes.
  fxl::WriteUnicodeCharacter(1, &encoded_scheme);
  return encoded_scheme + uri;
}

const char kUndefinedScheme = 0x01;

std::string DecodeUri(const std::string& uri) {
  if (uri[0] == kUndefinedScheme) {
    return uri.substr(1);
  }
  uint32_t cp = 0;
  size_t index = 0;
  if (!fxl::ReadUnicodeCharacter(uri.c_str(), uri.size(), &index, &cp)) {
    // Malformed UTF-8
    return "";
  }
  ZX_DEBUG_ASSERT(cp >= 2);
  return kUriSchemes[cp - 2] + uri.substr(index + 1);
}

template <typename T>
inline size_t BufferWrite(MutableByteBuffer* buffer, size_t pos, const T& var) {
  buffer->Write((uint8_t*)&var, sizeof(T), pos);
  return sizeof(T);
}

}  // namespace

bool AdvertisingData::FromBytes(const ByteBuffer& data,
                                AdvertisingData* out_ad) {
  ZX_DEBUG_ASSERT(out_ad);
  AdvertisingDataReader reader(data);
  if (!reader.is_valid())
    return false;

  DataType type;
  BufferView field;
  while (reader.GetNextField(&type, &field)) {
    switch (type) {
      case DataType::kTxPowerLevel: {
        if (field.size() != kTxPowerLevelSize) {
          bt_log(WARN, "gap-le", "received malformed Tx Power Level");
          return false;
        }

        out_ad->SetTxPower(static_cast<int8_t>(field[0]));
        break;
      }
      case DataType::kShortenedLocalName:
        // If a name has been previously set (e.g. because the Complete Local
        // Name was included in the scan response) then break. Otherwise we fall
        // through.
        if (out_ad->local_name())
          break;
      case DataType::kCompleteLocalName: {
        out_ad->SetLocalName(field.ToString());
        break;
      }
      case DataType::kIncomplete16BitServiceUuids:
      case DataType::kComplete16BitServiceUuids:
      case DataType::kIncomplete32BitServiceUuids:
      case DataType::kComplete32BitServiceUuids:
      case DataType::kIncomplete128BitServiceUuids:
      case DataType::kComplete128BitServiceUuids: {
        if (!ParseUuids(field, SizeForType(type), [&](const UUID& uuid) {
              out_ad->AddServiceUuid(uuid);
            }))
          return false;
        break;
      }
      case DataType::kManufacturerSpecificData: {
        if (field.size() < kManufacturerSpecificDataSizeMin) {
          bt_log(WARN, "gap-le", "manufacturer specific data too small");
          return false;
        }

        uint16_t id = le16toh(*reinterpret_cast<const uint16_t*>(field.data()));
        const BufferView manuf_data(field.data() + kManufacturerIdSize,
                                    field.size() - kManufacturerIdSize);

        out_ad->SetManufacturerData(id, manuf_data);
        break;
      }
      case DataType::kServiceData16Bit:
      case DataType::kServiceData32Bit:
      case DataType::kServiceData128Bit: {
        UUID uuid;
        size_t uuid_size = SizeForType(type);
        const BufferView uuid_bytes(field.data(), uuid_size);
        if (!UUID::FromBytes(uuid_bytes, &uuid))
          return false;
        const BufferView service_data(field.data() + uuid_size,
                                      field.size() - uuid_size);
        out_ad->SetServiceData(uuid, service_data);
        break;
      }
      case DataType::kAppearance: {
        // TODO(armansito): Peer should have a function to return the
        // device appearance, as it can be obtained either from advertising data
        // or via GATT.
        if (field.size() != kAppearanceSize) {
          bt_log(WARN, "gap-le", "received malformed Appearance");
          return false;
        }

        out_ad->SetAppearance(*reinterpret_cast<const uint16_t*>(field.data()));
        break;
      }
      case DataType::kURI: {
        out_ad->AddURI(DecodeUri(field.ToString()));
        break;
      }
      case DataType::kFlags: {
        // TODO(jamuraa): is there anything to do here?
        break;
      }
      default:
        bt_log(TRACE, "gap-le", "ignored advertising field (type %#.2x)",
               static_cast<unsigned int>(type));
        break;
    }
  }

  return true;
}

::ble::AdvertisingDataPtr AdvertisingData::AsLEAdvertisingData() const {
  auto fidl_data = ::ble::AdvertisingData::New();
  ZX_DEBUG_ASSERT(fidl_data);

  if (tx_power_) {
    fidl_data->tx_power_level = ::fuchsia::bluetooth::Int8::New();
    fidl_data->tx_power_level->value = *tx_power_;
  }

  if (appearance_) {
    fidl_data->appearance = ::fuchsia::bluetooth::UInt16::New();
    fidl_data->appearance->value = *appearance_;
  }

  for (const auto& pair : manufacturer_data_) {
    ::ble::ManufacturerSpecificDataEntry entry;
    entry.company_id = pair.first;
    entry.data = fidl::To<fidl::VectorPtr<unsigned char>>(pair.second);
    fidl_data->manufacturer_specific_data.push_back(std::move(entry));
  }

  for (const auto& pair : service_data_) {
    ::ble::ServiceDataEntry entry;
    entry.uuid = pair.first.ToString();
    entry.data = fidl::To<fidl::VectorPtr<unsigned char>>(pair.second);
    fidl_data->service_data.push_back(std::move(entry));
  }

  for (const auto& uuid : service_uuids_) {
    fidl_data->service_uuids.push_back(uuid.ToString());
  }

  for (const auto& uri : uris_) {
    fidl_data->uris.push_back(uri);
  }

  if (local_name_) {
    fidl_data->name = *local_name_;
  }

  return fidl_data;
}

bool AdvertisingData::FromFidl(const ::ble::AdvertisingData& fidl_ad,
                               AdvertisingData* out_ad) {
  ZX_DEBUG_ASSERT(out_ad);
  UUID uuid;
  for (const auto& uuid_str : *fidl_ad.service_uuids) {
    if (!StringToUuid(uuid_str, &uuid)) {
      bt_log(WARN, "gap-le", "FIDL advertising data contains malformd UUID: %s",
             uuid_str.c_str());
      return false;
    }
    out_ad->AddServiceUuid(uuid);
  }

  for (const auto& it : *fidl_ad.manufacturer_specific_data) {
    const std::vector<uint8_t>& data = it.data;
    BufferView manuf_view(data.data(), data.size());
    out_ad->SetManufacturerData(it.company_id, manuf_view);
  }

  for (const auto& it : *fidl_ad.service_data) {
    const std::vector<uint8_t>& data = it.data;
    BufferView service_data_bytes(data.data(), data.size());
    UUID service_data_uuid;
    if (!StringToUuid(it.uuid, &service_data_uuid)) {
      bt_log(WARN, "gap-le", "FIDL service data contains malformed UUID: %s",
             it.uuid.c_str());
      return false;
    }

    out_ad->SetServiceData(service_data_uuid, service_data_bytes);
  }

  if (fidl_ad.appearance) {
    out_ad->SetAppearance(fidl_ad.appearance->value);
  }

  if (fidl_ad.tx_power_level) {
    out_ad->SetTxPower(fidl_ad.tx_power_level->value);
  }

  if (fidl_ad.name) {
    out_ad->SetLocalName(fidl_ad.name);
  }

  return true;
}

void AdvertisingData::Copy(AdvertisingData* out) const {
  if (local_name_)
    out->SetLocalName(*local_name_);
  if (tx_power_)
    out->SetTxPower(*tx_power_);
  if (appearance_)
    out->SetAppearance(*appearance_);
  for (const auto& it : service_uuids_) {
    out->AddServiceUuid(it);
  }
  for (const auto& it : manufacturer_data_) {
    out->SetManufacturerData(it.first, it.second.view());
  }
  for (const auto& it : service_data_) {
    out->SetServiceData(it.first, it.second.view());
  }
  for (const auto& it : uris_) {
    out->AddURI(it);
  }
}

void AdvertisingData::AddServiceUuid(const UUID& uuid) {
  service_uuids_.insert(uuid);
}

const std::unordered_set<UUID>& AdvertisingData::service_uuids() const {
  return service_uuids_;
}

void AdvertisingData::SetServiceData(const UUID& uuid, const ByteBuffer& data) {
  DynamicByteBuffer srv_data(data.size());
  data.Copy(&srv_data);
  service_data_[uuid] = std::move(srv_data);
}

const std::unordered_set<UUID> AdvertisingData::service_data_uuids() const {
  std::unordered_set<UUID> uuids;
  for (const auto& it : service_data_) {
    uuids.emplace(it.first);
  }
  return uuids;
}

const BufferView AdvertisingData::service_data(const UUID& uuid) const {
  auto iter = service_data_.find(uuid);
  if (iter == service_data_.end())
    return BufferView();
  return BufferView(iter->second);
}

void AdvertisingData::SetManufacturerData(const uint16_t company_id,
                                          const BufferView& data) {
  DynamicByteBuffer manuf_data(data.size());
  data.Copy(&manuf_data);
  manufacturer_data_[company_id] = std::move(manuf_data);
}

const std::unordered_set<uint16_t> AdvertisingData::manufacturer_data_ids()
    const {
  std::unordered_set<uint16_t> manuf_ids;
  for (const auto& it : manufacturer_data_) {
    manuf_ids.emplace(it.first);
  }
  return manuf_ids;
}

const BufferView AdvertisingData::manufacturer_data(
    const uint16_t company_id) const {
  auto iter = manufacturer_data_.find(company_id);
  if (iter == manufacturer_data_.end())
    return BufferView();
  return BufferView(iter->second);
}

void AdvertisingData::SetTxPower(int8_t dbm) { tx_power_ = dbm; }

std::optional<int8_t> AdvertisingData::tx_power() const { return tx_power_; }

void AdvertisingData::SetLocalName(const std::string& name) {
  local_name_ = std::string(name);
}

std::optional<std::string> AdvertisingData::local_name() const {
  return local_name_;
}

void AdvertisingData::AddURI(const std::string& uri) {
  if (!uri.empty())
    uris_.insert(uri);
}

const std::unordered_set<std::string>& AdvertisingData::uris() const {
  return uris_;
}

void AdvertisingData::SetAppearance(uint16_t appearance) {
  appearance_ = appearance;
}

std::optional<uint16_t> AdvertisingData::appearance() const {
  return appearance_;
}

size_t AdvertisingData::CalculateBlockSize() const {
  size_t len = 0;
  if (tx_power_)
    len += 3;
  if (appearance_)
    len += 4;
  if (local_name_)
    len += 2 + local_name_->size();

  for (const auto& manuf_pair : manufacturer_data_) {
    len += 2 + 2 + manuf_pair.second.size();
  }

  for (const auto& service_data_pair : service_data_) {
    len += 2 + service_data_pair.first.CompactSize() +
           service_data_pair.second.size();
  }

  for (const auto& uri : uris_) {
    len += 2 + EncodeUri(uri).size();
  }

  size_t small_uuids = 0;
  size_t medium_uuids = 0;
  size_t big_uuids = 0;
  for (const auto& uuid : service_uuids_) {
    switch (uuid.CompactSize()) {
      case 2: {
        if (small_uuids == 0)
          len += 2;
        small_uuids++;
        break;
      }
      case 4: {
        if (medium_uuids == 0)
          len += 2;
        medium_uuids++;
        break;
      }
      case 16: {
        if (big_uuids == 0)
          len += 2;
        big_uuids++;
        break;
      }
      default: {
        bt_log(WARN, "gap-le", "unknown UUID size");
        break;
      }
    }
  }

  len += (small_uuids * 2) + (medium_uuids * 4) + (big_uuids * 16);
  return len;
}

bool AdvertisingData::WriteBlock(MutableByteBuffer* buffer) const {
  ZX_DEBUG_ASSERT(buffer);
  if (buffer->size() < CalculateBlockSize())
    return false;

  size_t pos = 0;
  if (tx_power_) {
    (*buffer)[pos++] = 2;
    (*buffer)[pos++] = static_cast<uint8_t>(DataType::kTxPowerLevel);
    (*buffer)[pos++] = *reinterpret_cast<uint8_t*>(tx_power_.value());
  }

  if (appearance_) {
    (*buffer)[pos++] = 3;
    (*buffer)[pos++] = static_cast<uint8_t>(DataType::kAppearance);
    pos += BufferWrite(buffer, pos, appearance_.value());
  }

  if (local_name_) {
    (*buffer)[pos++] = 1 + local_name_->size();
    (*buffer)[pos++] = static_cast<uint8_t>(DataType::kCompleteLocalName);
    buffer->Write(reinterpret_cast<const uint8_t*>(local_name_->c_str()),
                  local_name_->length(), pos);
    pos += local_name_->size();
  }

  for (const auto& manuf_pair : manufacturer_data_) {
    size_t data_size = manuf_pair.second.size();
    (*buffer)[pos++] = 1 + 2 + data_size;  // 1 for type, 2 for Manuf. Code
    (*buffer)[pos++] =
        static_cast<uint8_t>(DataType::kManufacturerSpecificData);
    pos += BufferWrite(buffer, pos, manuf_pair.first);
    buffer->Write(manuf_pair.second, pos);
    pos += data_size;
  }

  for (const auto& service_data_pair : service_data_) {
    size_t uuid_size = service_data_pair.first.CompactSize();
    (*buffer)[pos++] = 1 + uuid_size + service_data_pair.second.size();
    switch (uuid_size) {
      case 2:
        (*buffer)[pos++] = static_cast<uint8_t>(DataType::kServiceData16Bit);
        break;
      case 4:
        (*buffer)[pos++] = static_cast<uint8_t>(DataType::kServiceData32Bit);
        break;
      case 16:
        (*buffer)[pos++] = static_cast<uint8_t>(DataType::kServiceData128Bit);
        break;
    };
    auto target = buffer->mutable_view(pos);
    pos += service_data_pair.first.ToBytes(&target);
    buffer->Write(service_data_pair.second, pos);
    pos += service_data_pair.second.size();
  }

  for (const auto& uri : uris_) {
    std::string s = EncodeUri(uri);
    (*buffer)[pos++] = 1 + s.size();
    (*buffer)[pos++] = static_cast<uint8_t>(DataType::kURI);
    buffer->Write(reinterpret_cast<const uint8_t*>(s.c_str()), s.length(), pos);
    pos += s.size();
  }

  std::unordered_map<size_t, std::unordered_set<UUID>> uuid_sets;
  for (const auto& uuid : service_uuids_) {
    uuid_sets[uuid.CompactSize()].insert(uuid);
  }

  for (const auto& pair : uuid_sets) {
    (*buffer)[pos++] = 1 + pair.first * pair.second.size();
    switch (pair.first) {
      case 2:
        (*buffer)[pos++] =
            static_cast<uint8_t>(DataType::kIncomplete16BitServiceUuids);
        break;
      case 4:
        (*buffer)[pos++] =
            static_cast<uint8_t>(DataType::kIncomplete32BitServiceUuids);
        break;
      case 16:
        (*buffer)[pos++] =
            static_cast<uint8_t>(DataType::kIncomplete128BitServiceUuids);
        break;
    };
    for (const auto& uuid : pair.second) {
      auto target = buffer->mutable_view(pos);
      pos += uuid.ToBytes(&target);
    }
  }

  return true;
}

bool AdvertisingData::operator==(const AdvertisingData& other) const {
  if ((local_name_ != other.local_name_) || (tx_power_ != other.tx_power_) ||
      (appearance_ != other.appearance_) ||
      (service_uuids_ != other.service_uuids_) || (uris_ != other.uris_)) {
    return false;
  }

  if (manufacturer_data_.size() != other.manufacturer_data_.size()) {
    return false;
  }

  for (const auto& it : manufacturer_data_) {
    auto that = other.manufacturer_data_.find(it.first);
    if (that == other.manufacturer_data_.end()) {
      return false;
    }
    size_t bytes = it.second.size();
    if (bytes != that->second.size()) {
      return false;
    }
    if (std::memcmp(it.second.data(), that->second.data(), bytes) != 0) {
      return false;
    }
  }

  if (service_data_.size() != other.service_data_.size()) {
    return false;
  }

  for (const auto& it : service_data_) {
    auto that = other.service_data_.find(it.first);
    if (that == other.service_data_.end()) {
      return false;
    }
    size_t bytes = it.second.size();
    if (bytes != that->second.size()) {
      return false;
    }
    if (std::memcmp(it.second.data(), that->second.data(), bytes) != 0) {
      return false;
    }
  }

  return true;
}

bool AdvertisingData::operator!=(const AdvertisingData& other) const {
  return !(*this == other);
}

AdvertisingDataReader::AdvertisingDataReader(const ByteBuffer& data)
    : is_valid_(true), remaining_(data) {
  if (!remaining_.size()) {
    is_valid_ = false;
    return;
  }

  // Do a validity check.
  BufferView tmp(remaining_);
  while (tmp.size()) {
    size_t tlv_len = tmp[0];

    // A struct can have 0 as its length. In that case its valid to terminate.
    if (!tlv_len)
      break;

    // The full struct includes the length octet itself.
    size_t struct_size = tlv_len + 1;
    if (struct_size > tmp.size()) {
      is_valid_ = false;
      break;
    }

    tmp = tmp.view(struct_size);
  }
}

bool AdvertisingDataReader::GetNextField(DataType* out_type,
                                         BufferView* out_data) {
  ZX_DEBUG_ASSERT(out_type);
  ZX_DEBUG_ASSERT(out_data);

  if (!HasMoreData())
    return false;

  size_t tlv_len = remaining_[0];
  size_t cur_struct_size = tlv_len + 1;
  ZX_DEBUG_ASSERT(cur_struct_size <= remaining_.size());

  *out_type = static_cast<DataType>(remaining_[1]);
  *out_data = remaining_.view(2, tlv_len - 1);

  // Update |remaining_|.
  remaining_ = remaining_.view(cur_struct_size);
  return true;
}

bool AdvertisingDataReader::HasMoreData() const {
  if (!is_valid_ || !remaining_.size())
    return false;

  // If the buffer is valid and has remaining bytes but the length of the next
  // segment is zero, then we terminate.
  return !!remaining_[0];
}

AdvertisingDataWriter::AdvertisingDataWriter(MutableByteBuffer* buffer)
    : buffer_(buffer), bytes_written_(0u) {
  ZX_DEBUG_ASSERT(buffer_);
}

bool AdvertisingDataWriter::WriteField(DataType type, const ByteBuffer& data) {
  size_t next_size = data.size() + 2;  // 2 bytes for [length][type].
  if (bytes_written_ + next_size > buffer_->size() || next_size > 255)
    return false;

  (*buffer_)[bytes_written_++] = static_cast<uint8_t>(next_size) - 1;
  (*buffer_)[bytes_written_++] = static_cast<uint8_t>(type);

  // Get a view into the offset we want to write to.
  auto target = buffer_->mutable_view(bytes_written_);

  // Copy the data into the view.
  size_t written = data.Copy(&target);
  ZX_DEBUG_ASSERT(written == data.size());

  bytes_written_ += written;

  return true;
}

}  // namespace gap
}  // namespace bt
