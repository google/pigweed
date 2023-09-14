// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "helpers.h"

#include <endian.h>

#include <algorithm>
#include <charconv>
#include <functional>
#include <iterator>
#include <optional>
#include <unordered_set>

#include "fuchsia/bluetooth/sys/cpp/fidl.h"
#include "fuchsia/media/cpp/fidl.h"
#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/common/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/discovery_filter.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/vendor_protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/sco/sco.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"

using fuchsia::bluetooth::Error;
using fuchsia::bluetooth::ErrorCode;
using fuchsia::bluetooth::Int8;
using fuchsia::bluetooth::Status;

namespace fble = fuchsia::bluetooth::le;
namespace fbredr = fuchsia::bluetooth::bredr;
namespace fbt = fuchsia::bluetooth;
namespace fgatt = fuchsia::bluetooth::gatt;
namespace fgatt2 = fuchsia::bluetooth::gatt2;
namespace fsys = fuchsia::bluetooth::sys;
namespace faudio = fuchsia::hardware::audio;

const uint8_t BIT_SHIFT_4 = 4;
const uint8_t BIT_SHIFT_8 = 8;
const uint8_t BIT_SHIFT_16 = 16;

const uint8_t TOP_TWO_BITS = 0x0C;
const uint8_t BOTTOM_TWO_BITS = 0x03;

namespace bthost::fidl_helpers {
// TODO(fxbug.dev/125641): Add remaining codecs
std::optional<bt::hci_spec::vendor::android::A2dpCodecType> FidlToCodecType(
    const fbredr::AudioOffloadFeatures& codec) {
  switch (codec.Which()) {
    case fuchsia::bluetooth::bredr::AudioOffloadFeatures::kSbc:
      return bt::hci_spec::vendor::android::A2dpCodecType::kSbc;
    case fuchsia::bluetooth::bredr::AudioOffloadFeatures::kAac:
      return bt::hci_spec::vendor::android::A2dpCodecType::kAac;
    default:
      bt_log(WARN, "fidl", "Codec type not yet handled: %u",
             static_cast<unsigned int>(codec.Which()));
      return std::nullopt;
  }
}

bt::hci_spec::vendor::android::A2dpScmsTEnable FidlToScmsTEnable(bool scms_t_enable) {
  bt::hci_spec::vendor::android::A2dpScmsTEnable scms_t_enable_struct;
  if (scms_t_enable) {
    scms_t_enable_struct.enabled = pw::bluetooth::emboss::GenericEnableParam::ENABLE;
  } else {
    scms_t_enable_struct.enabled = pw::bluetooth::emboss::GenericEnableParam::DISABLE;
  }
  scms_t_enable_struct.header = 0x0;
  return scms_t_enable_struct;
}

bt::hci_spec::vendor::android::A2dpSamplingFrequency FidlToSamplingFrequency(
    fbredr::AudioSamplingFrequency sampling_frequency) {
  switch (sampling_frequency) {
    case fbredr::AudioSamplingFrequency::HZ_44100:
      return bt::hci_spec::vendor::android::A2dpSamplingFrequency::k44100Hz;
    case fbredr::AudioSamplingFrequency::HZ_48000:
      return bt::hci_spec::vendor::android::A2dpSamplingFrequency::k48000Hz;
    case fbredr::AudioSamplingFrequency::HZ_88200:
      return bt::hci_spec::vendor::android::A2dpSamplingFrequency::k88200Hz;
    case fbredr::AudioSamplingFrequency::HZ_96000:
      return bt::hci_spec::vendor::android::A2dpSamplingFrequency::k96000Hz;
  }
}

bt::hci_spec::vendor::android::A2dpBitsPerSample FidlToBitsPerSample(
    fbredr::AudioBitsPerSample bits_per_sample) {
  switch (bits_per_sample) {
    case fbredr::AudioBitsPerSample::BPS_16:
      return bt::hci_spec::vendor::android::A2dpBitsPerSample::k16BitsPerSample;
    case fbredr::AudioBitsPerSample::BPS_24:
      return bt::hci_spec::vendor::android::A2dpBitsPerSample::k24BitsPerSample;
    case fbredr::AudioBitsPerSample::BPS_32:
      return bt::hci_spec::vendor::android::A2dpBitsPerSample::k32BitsPerSample;
  }
}

bt::hci_spec::vendor::android::A2dpChannelMode FidlToChannelMode(
    fbredr::AudioChannelMode channel_mode) {
  switch (channel_mode) {
    case fbredr::AudioChannelMode::MONO:
      return bt::hci_spec::vendor::android::A2dpChannelMode::kMono;
    case fbredr::AudioChannelMode::STEREO:
      return bt::hci_spec::vendor::android::A2dpChannelMode::kStereo;
  }
}

bt::hci_spec::vendor::android::A2dpOffloadCodecInformation FidlToEncoderSettings(
    const fbredr::AudioEncoderSettings& encoder_settings,
    fbredr::AudioSamplingFrequency sampling_frequency, fbredr::AudioChannelMode channel_mode) {
  bt::hci_spec::vendor::android::A2dpOffloadCodecInformation codec_information;
  if (encoder_settings.is_sbc()) {
    bt::hci_spec::vendor::android::SbcCodecInformation sbc;

    // Bitmask: block length | subbands | allocation method
    // Block length: bits 7-4
    // Subbands: bits 3-2
    // Allocation method: bits 1-0
    if (encoder_settings.sbc().allocation == fuchsia::media::SbcAllocation::ALLOC_LOUDNESS) {
      sbc.blocklen_subbands_alloc_method =
          static_cast<uint8_t>(fuchsia::media::SbcAllocation::ALLOC_LOUDNESS) & BOTTOM_TWO_BITS;
    }
    if (encoder_settings.sbc().allocation == fuchsia::media::SbcAllocation::ALLOC_SNR) {
      sbc.blocklen_subbands_alloc_method =
          static_cast<uint8_t>(fuchsia::media::SbcAllocation::ALLOC_SNR) & BOTTOM_TWO_BITS;
    }

    if (encoder_settings.sbc().sub_bands == fuchsia::media::SbcSubBands::SUB_BANDS_4) {
      sbc.blocklen_subbands_alloc_method |=
          static_cast<uint8_t>(fuchsia::media::SbcSubBands::SUB_BANDS_4) & TOP_TWO_BITS;
    }
    if (encoder_settings.sbc().sub_bands == fuchsia::media::SbcSubBands::SUB_BANDS_8) {
      sbc.blocklen_subbands_alloc_method |=
          static_cast<uint8_t>(fuchsia::media::SbcSubBands::SUB_BANDS_8) & TOP_TWO_BITS;
    }

    if (encoder_settings.sbc().block_count == fuchsia::media::SbcBlockCount::BLOCK_COUNT_4) {
      sbc.blocklen_subbands_alloc_method |=
          static_cast<uint8_t>(fuchsia::media::SbcBlockCount::BLOCK_COUNT_4) << BIT_SHIFT_4;
    }
    if (encoder_settings.sbc().block_count == fuchsia::media::SbcBlockCount::BLOCK_COUNT_8) {
      sbc.blocklen_subbands_alloc_method |=
          static_cast<uint8_t>(fuchsia::media::SbcBlockCount::BLOCK_COUNT_8) << BIT_SHIFT_4;
    }
    if (encoder_settings.sbc().block_count == fuchsia::media::SbcBlockCount::BLOCK_COUNT_12) {
      sbc.blocklen_subbands_alloc_method |=
          static_cast<uint8_t>(fuchsia::media::SbcBlockCount::BLOCK_COUNT_12) << BIT_SHIFT_4;
    }
    if (encoder_settings.sbc().block_count == fuchsia::media::SbcBlockCount::BLOCK_COUNT_16) {
      sbc.blocklen_subbands_alloc_method |=
          static_cast<uint8_t>(fuchsia::media::SbcBlockCount::BLOCK_COUNT_16) << BIT_SHIFT_4;
    }

    sbc.min_bitpool_value = encoder_settings.sbc().bit_pool;
    sbc.max_bitpool_value = encoder_settings.sbc().bit_pool;

    // Bitmask: sampling frequency | channel mode
    // Sampling frequency: bits 7-4
    // Channel mode: bits 3-0
    sbc.sampling_freq_channel_mode = static_cast<uint8_t>(channel_mode);
    sbc.sampling_freq_channel_mode |= static_cast<uint8_t>(sampling_frequency) << BIT_SHIFT_4;

    memset(sbc.reserved, 0, sizeof(sbc.reserved));

    codec_information.sbc = sbc;
  }
  if (encoder_settings.is_aac()) {
    bt::hci_spec::vendor::android::AacCodecInformation aac;

    aac.object_type = static_cast<uint8_t>(encoder_settings.aac().aot);

    if (encoder_settings.aac().bit_rate.is_variable()) {
      aac.variable_bit_rate = bt::hci_spec::vendor::android::A2dpAacEnableVariableBitRate::kEnable;
    }
    if (encoder_settings.aac().bit_rate.is_constant()) {
      aac.variable_bit_rate = bt::hci_spec::vendor::android::A2dpAacEnableVariableBitRate::kDisable;
    }

    memset(aac.reserved, 0, sizeof(aac.reserved));

    codec_information.aac = aac;
  }
  return codec_information;
}

std::optional<bt::sdp::DataElement> FidlToDataElement(const fbredr::DataElement& fidl) {
  bt::sdp::DataElement out;
  switch (fidl.Which()) {
    case fbredr::DataElement::Tag::kInt8:
      return bt::sdp::DataElement(fidl.int8());
    case fbredr::DataElement::Tag::kInt16:
      return bt::sdp::DataElement(fidl.int16());
    case fbredr::DataElement::Tag::kInt32:
      return bt::sdp::DataElement(fidl.int32());
    case fbredr::DataElement::Tag::kInt64:
      return bt::sdp::DataElement(fidl.int64());
    case fbredr::DataElement::Tag::kUint8:
      return bt::sdp::DataElement(fidl.uint8());
    case fbredr::DataElement::Tag::kUint16:
      return bt::sdp::DataElement(fidl.uint16());
    case fbredr::DataElement::Tag::kUint32:
      return bt::sdp::DataElement(fidl.uint32());
    case fbredr::DataElement::Tag::kUint64:
      return bt::sdp::DataElement(fidl.uint64());
    case fbredr::DataElement::Tag::kStr: {
      const std::vector<uint8_t>& str = fidl.str();
      bt::DynamicByteBuffer bytes((bt::BufferView(str)));
      return bt::sdp::DataElement(bytes);
    }
    case fbredr::DataElement::Tag::kUrl:
      out.SetUrl(fidl.url());
      break;
    case fbredr::DataElement::Tag::kB:
      return bt::sdp::DataElement(fidl.b());
    case fbredr::DataElement::Tag::kUuid:
      out.Set(fidl_helpers::UuidFromFidl(fidl.uuid()));
      break;
    case fbredr::DataElement::Tag::kSequence: {
      std::vector<bt::sdp::DataElement> seq;
      for (const auto& fidl_elem : fidl.sequence()) {
        std::optional<bt::sdp::DataElement> elem = FidlToDataElement(*fidl_elem);
        if (!elem) {
          return std::nullopt;
        }
        seq.emplace_back(std::move(elem.value()));
      }
      out.Set(std::move(seq));
      break;
    }
    case fbredr::DataElement::Tag::kAlternatives: {
      std::vector<bt::sdp::DataElement> alts;
      for (const auto& fidl_elem : fidl.alternatives()) {
        auto elem = FidlToDataElement(*fidl_elem);
        if (!elem) {
          return std::nullopt;
        }
        alts.emplace_back(std::move(elem.value()));
      }
      out.SetAlternative(std::move(alts));
      break;
    }
    default:
      // Types not handled: Null datatype (never used)
      bt_log(WARN, "fidl", "Encountered FidlToDataElement type not handled.");
      return std::nullopt;
  }
  return out;
}

namespace {

fbt::AddressType AddressTypeToFidl(bt::DeviceAddress::Type type) {
  switch (type) {
    case bt::DeviceAddress::Type::kBREDR:
      [[fallthrough]];
    case bt::DeviceAddress::Type::kLEPublic:
      return fbt::AddressType::PUBLIC;
    case bt::DeviceAddress::Type::kLERandom:
      [[fallthrough]];
    case bt::DeviceAddress::Type::kLEAnonymous:
      return fbt::AddressType::RANDOM;
  }
  return fbt::AddressType::PUBLIC;
}

fbt::Address AddressToFidl(fbt::AddressType type, const bt::DeviceAddressBytes& value) {
  fbt::Address output;
  output.type = type;
  bt::MutableBufferView value_dst(output.bytes.data(), output.bytes.size());
  value_dst.Write(value.bytes());
  return output;
}

fbt::Address AddressToFidl(const bt::DeviceAddress& input) {
  return AddressToFidl(AddressTypeToFidl(input.type()), input.value());
}

bt::sm::SecurityProperties SecurityPropsFromFidl(const fsys::SecurityProperties& sec_prop) {
  auto level = bt::sm::SecurityLevel::kEncrypted;
  if (sec_prop.authenticated) {
    level = bt::sm::SecurityLevel::kAuthenticated;
  }
  return bt::sm::SecurityProperties(level, sec_prop.encryption_key_size,
                                    sec_prop.secure_connections);
}

fsys::SecurityProperties SecurityPropsToFidl(const bt::sm::SecurityProperties& sec_prop) {
  return fsys::SecurityProperties{
      .authenticated = sec_prop.authenticated(),
      .secure_connections = sec_prop.secure_connections(),

      // TODO(armansito): Declare the key size as uint8_t in bt::sm?
      .encryption_key_size = static_cast<uint8_t>(sec_prop.enc_key_size()),
  };
}

bt::sm::LTK LtkFromFidl(const fsys::Ltk& ltk) {
  return bt::sm::LTK(SecurityPropsFromFidl(ltk.key.security),
                     bt::hci_spec::LinkKey(ltk.key.data.value, ltk.rand, ltk.ediv));
}

fsys::PeerKey LtkToFidlPeerKey(const bt::sm::LTK& ltk) {
  return fsys::PeerKey{
      .security = SecurityPropsToFidl(ltk.security()),
      .data = fsys::Key{ltk.key().value()},
  };
}

fsys::Ltk LtkToFidl(const bt::sm::LTK& ltk) {
  return fsys::Ltk{
      .key = LtkToFidlPeerKey(ltk),
      .ediv = ltk.key().ediv(),
      .rand = ltk.key().rand(),
  };
}

bt::sm::Key PeerKeyFromFidl(const fsys::PeerKey& key) {
  return bt::sm::Key(SecurityPropsFromFidl(key.security), key.data.value);
}

fsys::PeerKey PeerKeyToFidl(const bt::sm::Key& key) {
  return fsys::PeerKey{
      .security = SecurityPropsToFidl(key.security()),
      .data = {key.value()},
  };
}

fbt::DeviceClass DeviceClassToFidl(bt::DeviceClass input) {
  auto bytes = input.bytes();
  fbt::DeviceClass output{
      static_cast<uint32_t>(bytes[0] | (bytes[1] << BIT_SHIFT_8) | (bytes[2] << BIT_SHIFT_16))};
  return output;
}

bool AddProtocolDescriptorList(bt::sdp::ServiceRecord* rec,
                               bt::sdp::ServiceRecord::ProtocolListId id,
                               const ::std::vector<fbredr::ProtocolDescriptor>& descriptor_list) {
  bt_log(TRACE, "fidl", "ProtocolDescriptorList %d", id);
  for (auto& descriptor : descriptor_list) {
    bt::sdp::DataElement protocol_params;
    if (descriptor.params.size() > 1) {
      std::vector<bt::sdp::DataElement> params;
      for (auto& fidl_param : descriptor.params) {
        auto bt_param = FidlToDataElement(fidl_param);
        if (bt_param) {
          params.emplace_back(std::move(bt_param.value()));
        } else {
          return false;
        }
      }
      protocol_params.Set(std::move(params));
    } else if (descriptor.params.size() == 1) {
      auto param = FidlToDataElement(descriptor.params.front());
      if (param) {
        protocol_params = std::move(param).value();
      } else {
        return false;
      }
      protocol_params = FidlToDataElement(descriptor.params.front()).value();
    }

    bt_log(TRACE, "fidl", "Adding protocol descriptor: {%d : %s}",
           fidl::ToUnderlying(descriptor.protocol), protocol_params.ToString().c_str());
    rec->AddProtocolDescriptor(id, bt::UUID(static_cast<uint16_t>(descriptor.protocol)),
                               std::move(protocol_params));
  }
  return true;
}

// Returns true if the appearance value (in host byte order) is included in
// fuchsia.bluetooth.Appearance, which is a subset of Bluetooth Assigned Numbers, "Appearance
// Values" (https://www.bluetooth.com/specifications/assigned-numbers/).
//
// TODO(fxbug.dev/66358): Remove this compatibility check with the strict Appearance enum.
[[nodiscard]] bool IsAppearanceValid(uint16_t appearance_raw) {
  switch (appearance_raw) {
    case 0u:  // UNKNOWN
      [[fallthrough]];
    case 64u:  // PHONE
      [[fallthrough]];
    case 128u:  // COMPUTER
      [[fallthrough]];
    case 192u:  // WATCH
      [[fallthrough]];
    case 193u:  // WATCH_SPORTS
      [[fallthrough]];
    case 256u:  // CLOCK
      [[fallthrough]];
    case 320u:  // DISPLAY
      [[fallthrough]];
    case 384u:  // REMOTE_CONTROL
      [[fallthrough]];
    case 448u:  // EYE_GLASSES
      [[fallthrough]];
    case 512u:  // TAG
      [[fallthrough]];
    case 576u:  // KEYRING
      [[fallthrough]];
    case 640u:  // MEDIA_PLAYER
      [[fallthrough]];
    case 704u:  // BARCODE_SCANNER
      [[fallthrough]];
    case 768u:  // THERMOMETER
      [[fallthrough]];
    case 769u:  // THERMOMETER_EAR
      [[fallthrough]];
    case 832u:  // HEART_RATE_SENSOR
      [[fallthrough]];
    case 833u:  // HEART_RATE_SENSOR_BELT
      [[fallthrough]];
    case 896u:  // BLOOD_PRESSURE
      [[fallthrough]];
    case 897u:  // BLOOD_PRESSURE_ARM
      [[fallthrough]];
    case 898u:  // BLOOD_PRESSURE_WRIST
      [[fallthrough]];
    case 960u:  // HID
      [[fallthrough]];
    case 961u:  // HID_KEYBOARD
      [[fallthrough]];
    case 962u:  // HID_MOUSE
      [[fallthrough]];
    case 963u:  // HID_JOYSTICK
      [[fallthrough]];
    case 964u:  // HID_GAMEPAD
      [[fallthrough]];
    case 965u:  // HID_DIGITIZER_TABLET
      [[fallthrough]];
    case 966u:  // HID_CARD_READER
      [[fallthrough]];
    case 967u:  // HID_DIGITAL_PEN
      [[fallthrough]];
    case 968u:  // HID_BARCODE_SCANNER
      [[fallthrough]];
    case 1024u:  // GLUCOSE_METER
      [[fallthrough]];
    case 1088u:  // RUNNING_WALKING_SENSOR
      [[fallthrough]];
    case 1089u:  // RUNNING_WALKING_SENSOR_IN_SHOE
      [[fallthrough]];
    case 1090u:  // RUNNING_WALKING_SENSOR_ON_SHOE
      [[fallthrough]];
    case 1091u:  // RUNNING_WALKING_SENSOR_ON_HIP
      [[fallthrough]];
    case 1152u:  // CYCLING
      [[fallthrough]];
    case 1153u:  // CYCLING_COMPUTER
      [[fallthrough]];
    case 1154u:  // CYCLING_SPEED_SENSOR
      [[fallthrough]];
    case 1155u:  // CYCLING_CADENCE_SENSOR
      [[fallthrough]];
    case 1156u:  // CYCLING_POWER_SENSOR
      [[fallthrough]];
    case 1157u:  // CYCLING_SPEED_AND_CADENCE_SENSOR
      [[fallthrough]];
    case 3136u:  // PULSE_OXIMETER
      [[fallthrough]];
    case 3137u:  // PULSE_OXIMETER_FINGERTIP
      [[fallthrough]];
    case 3138u:  // PULSE_OXIMETER_WRIST
      [[fallthrough]];
    case 3200u:  // WEIGHT_SCALE
      [[fallthrough]];
    case 3264u:  // PERSONAL_MOBILITY
      [[fallthrough]];
    case 3265u:  // PERSONAL_MOBILITY_WHEELCHAIR
      [[fallthrough]];
    case 3266u:  // PERSONAL_MOBILITY_SCOOTER
      [[fallthrough]];
    case 3328u:  // GLUCOSE_MONITOR
      [[fallthrough]];
    case 5184u:  // SPORTS_ACTIVITY
      [[fallthrough]];
    case 5185u:  // SPORTS_ACTIVITY_LOCATION_DISPLAY
      [[fallthrough]];
    case 5186u:  // SPORTS_ACTIVITY_LOCATION_AND_NAV_DISPLAY
      [[fallthrough]];
    case 5187u:  // SPORTS_ACTIVITY_LOCATION_POD
      [[fallthrough]];
    case 5188u:  // SPORTS_ACTIVITY_LOCATION_AND_NAV_POD
      return true;
    default:
      return false;
  }
}

[[nodiscard]] std::optional<fbt::Appearance> AppearanceToFidl(uint16_t appearance_raw) {
  if (IsAppearanceValid(appearance_raw)) {
    return static_cast<fbt::Appearance>(appearance_raw);
  }
  return std::nullopt;
}

}  // namespace

std::optional<bt::PeerId> PeerIdFromString(const std::string& id) {
  if (id.empty()) {
    return std::nullopt;
  }

  uint64_t value = 0;
  auto [_, error] = std::from_chars(id.data(), id.data() + id.size(), value, /*base=*/16);
  if (error != std::errc()) {
    return std::nullopt;
  }
  return bt::PeerId(value);
}

ErrorCode HostErrorToFidlDeprecated(bt::HostError host_error) {
  switch (host_error) {
    case bt::HostError::kFailed:
      return ErrorCode::FAILED;
    case bt::HostError::kTimedOut:
      return ErrorCode::TIMED_OUT;
    case bt::HostError::kInvalidParameters:
      return ErrorCode::INVALID_ARGUMENTS;
    case bt::HostError::kCanceled:
      return ErrorCode::CANCELED;
    case bt::HostError::kInProgress:
      return ErrorCode::IN_PROGRESS;
    case bt::HostError::kNotSupported:
      return ErrorCode::NOT_SUPPORTED;
    case bt::HostError::kNotFound:
      return ErrorCode::NOT_FOUND;
    default:
      break;
  }

  return ErrorCode::FAILED;
}

Status NewFidlError(ErrorCode error_code, const std::string& description) {
  Status status;
  status.error = std::make_unique<Error>();
  status.error->error_code = error_code;
  status.error->description = description;
  return status;
}

fsys::Error HostErrorToFidl(bt::HostError error) {
  switch (error) {
    case bt::HostError::kFailed:
      return fsys::Error::FAILED;
    case bt::HostError::kTimedOut:
      return fsys::Error::TIMED_OUT;
    case bt::HostError::kInvalidParameters:
      return fsys::Error::INVALID_ARGUMENTS;
    case bt::HostError::kCanceled:
      return fsys::Error::CANCELED;
    case bt::HostError::kInProgress:
      return fsys::Error::IN_PROGRESS;
    case bt::HostError::kNotSupported:
      return fsys::Error::NOT_SUPPORTED;
    case bt::HostError::kNotFound:
      return fsys::Error::PEER_NOT_FOUND;
    default:
      break;
  }
  return fsys::Error::FAILED;
}

fuchsia::bluetooth::gatt::Error GattErrorToFidl(const bt::att::Error& error) {
  return error.Visit(
      [](bt::HostError host_error) {
        return host_error == bt::HostError::kPacketMalformed
                   ? fuchsia::bluetooth::gatt::Error::INVALID_RESPONSE
                   : fuchsia::bluetooth::gatt::Error::FAILURE;
      },
      [](bt::att::ErrorCode att_error) {
        switch (att_error) {
          case bt::att::ErrorCode::kInsufficientAuthorization:
            return fuchsia::bluetooth::gatt::Error::INSUFFICIENT_AUTHORIZATION;
          case bt::att::ErrorCode::kInsufficientAuthentication:
            return fuchsia::bluetooth::gatt::Error::INSUFFICIENT_AUTHENTICATION;
          case bt::att::ErrorCode::kInsufficientEncryptionKeySize:
            return fuchsia::bluetooth::gatt::Error::INSUFFICIENT_ENCRYPTION_KEY_SIZE;
          case bt::att::ErrorCode::kInsufficientEncryption:
            return fuchsia::bluetooth::gatt::Error::INSUFFICIENT_ENCRYPTION;
          case bt::att::ErrorCode::kReadNotPermitted:
            return fuchsia::bluetooth::gatt::Error::READ_NOT_PERMITTED;
          default:
            break;
        }
        return fuchsia::bluetooth::gatt::Error::FAILURE;
      });
}

fuchsia::bluetooth::gatt2::Error AttErrorToGattFidlError(const bt::att::Error& error) {
  return error.Visit(
      [](bt::HostError host_error) {
        switch (host_error) {
          case bt::HostError::kPacketMalformed:
            return fuchsia::bluetooth::gatt2::Error::INVALID_PDU;
          case bt::HostError::kInvalidParameters:
            return fuchsia::bluetooth::gatt2::Error::INVALID_PARAMETERS;
          default:
            break;
        }
        return fuchsia::bluetooth::gatt2::Error::UNLIKELY_ERROR;
      },
      [](bt::att::ErrorCode att_error) {
        switch (att_error) {
          case bt::att::ErrorCode::kInsufficientAuthorization:
            return fuchsia::bluetooth::gatt2::Error::INSUFFICIENT_AUTHORIZATION;
          case bt::att::ErrorCode::kInsufficientAuthentication:
            return fuchsia::bluetooth::gatt2::Error::INSUFFICIENT_AUTHENTICATION;
          case bt::att::ErrorCode::kInsufficientEncryptionKeySize:
            return fuchsia::bluetooth::gatt2::Error::INSUFFICIENT_ENCRYPTION_KEY_SIZE;
          case bt::att::ErrorCode::kInsufficientEncryption:
            return fuchsia::bluetooth::gatt2::Error::INSUFFICIENT_ENCRYPTION;
          case bt::att::ErrorCode::kReadNotPermitted:
            return fuchsia::bluetooth::gatt2::Error::READ_NOT_PERMITTED;
          case bt::att::ErrorCode::kInvalidHandle:
            return fuchsia::bluetooth::gatt2::Error::INVALID_HANDLE;
          default:
            break;
        }
        return fuchsia::bluetooth::gatt2::Error::UNLIKELY_ERROR;
      });
}

bt::UUID UuidFromFidl(const fuchsia::bluetooth::Uuid& input) {
  // Conversion must always succeed given the defined size of |input|.
  static_assert(sizeof(input.value) == 16, "FIDL UUID definition malformed!");
  return bt::UUID(bt::BufferView(input.value.data(), input.value.size()));
}

fuchsia::bluetooth::Uuid UuidToFidl(const bt::UUID& uuid) {
  fuchsia::bluetooth::Uuid output;
  // Conversion must always succeed given the defined size of |input|.
  static_assert(sizeof(output.value) == 16, "FIDL UUID definition malformed!");
  output.value = uuid.value();
  return output;
}

bt::sm::IOCapability IoCapabilityFromFidl(fsys::InputCapability input,
                                          fsys::OutputCapability output) {
  if (input == fsys::InputCapability::NONE && output == fsys::OutputCapability::NONE) {
    return bt::sm::IOCapability::kNoInputNoOutput;
  }

  if (input == fsys::InputCapability::KEYBOARD && output == fsys::OutputCapability::DISPLAY) {
    return bt::sm::IOCapability::kKeyboardDisplay;
  }

  if (input == fsys::InputCapability::KEYBOARD && output == fsys::OutputCapability::NONE) {
    return bt::sm::IOCapability::kKeyboardOnly;
  }

  if (input == fsys::InputCapability::NONE && output == fsys::OutputCapability::DISPLAY) {
    return bt::sm::IOCapability::kDisplayOnly;
  }

  if (input == fsys::InputCapability::CONFIRMATION && output == fsys::OutputCapability::DISPLAY) {
    return bt::sm::IOCapability::kDisplayYesNo;
  }

  return bt::sm::IOCapability::kNoInputNoOutput;
}

std::optional<bt::gap::BrEdrSecurityMode> BrEdrSecurityModeFromFidl(
    const fsys::BrEdrSecurityMode mode) {
  switch (mode) {
    case fsys::BrEdrSecurityMode::MODE_4:
      return bt::gap::BrEdrSecurityMode::Mode4;
    case fsys::BrEdrSecurityMode::SECURE_CONNECTIONS_ONLY:
      return bt::gap::BrEdrSecurityMode::SecureConnectionsOnly;
    default:
      bt_log(WARN, "fidl", "BR/EDR security mode not recognized");
      return std::nullopt;
  }
}

bt::gap::LESecurityMode LeSecurityModeFromFidl(const fsys::LeSecurityMode mode) {
  switch (mode) {
    case fsys::LeSecurityMode::MODE_1:
      return bt::gap::LESecurityMode::Mode1;
    case fsys::LeSecurityMode::SECURE_CONNECTIONS_ONLY:
      return bt::gap::LESecurityMode::SecureConnectionsOnly;
  }
  bt_log(WARN, "fidl", "FIDL security mode not recognized, defaulting to SecureConnectionsOnly");
  return bt::gap::LESecurityMode::SecureConnectionsOnly;
}

std::optional<bt::sm::SecurityLevel> SecurityLevelFromFidl(const fsys::PairingSecurityLevel level) {
  switch (level) {
    case fsys::PairingSecurityLevel::ENCRYPTED:
      return bt::sm::SecurityLevel::kEncrypted;
    case fsys::PairingSecurityLevel::AUTHENTICATED:
      return bt::sm::SecurityLevel::kAuthenticated;
    default:
      return std::nullopt;
  };
}

fsys::TechnologyType TechnologyTypeToFidl(bt::gap::TechnologyType type) {
  switch (type) {
    case bt::gap::TechnologyType::kLowEnergy:
      return fsys::TechnologyType::LOW_ENERGY;
    case bt::gap::TechnologyType::kClassic:
      return fsys::TechnologyType::CLASSIC;
    case bt::gap::TechnologyType::kDualMode:
      return fsys::TechnologyType::DUAL_MODE;
    default:
      BT_PANIC("invalid technology type: %u", static_cast<unsigned int>(type));
      break;
  }

  // This should never execute.
  return fsys::TechnologyType::DUAL_MODE;
}

fsys::HostInfo HostInfoToFidl(const bt::gap::Adapter& adapter) {
  fsys::HostInfo info;
  info.set_id(fbt::HostId{adapter.identifier().value()});
  info.set_technology(TechnologyTypeToFidl(adapter.state().type()));
  info.set_local_name(adapter.local_name());
  info.set_discoverable(adapter.IsDiscoverable());
  info.set_discovering(adapter.IsDiscovering());
  std::vector<fbt::Address> addresses;
  addresses.emplace_back(
      AddressToFidl(fbt::AddressType::PUBLIC, adapter.state().controller_address));
  if (adapter.le() && adapter.le()->PrivacyEnabled() &&
      (!adapter.le()->CurrentAddress().IsPublic())) {
    addresses.emplace_back(AddressToFidl(adapter.le()->CurrentAddress()));
  }
  info.set_addresses(std::move(addresses));
  return info;
}

fsys::Peer PeerToFidl(const bt::gap::Peer& peer) {
  fsys::Peer output;
  output.set_id(fbt::PeerId{peer.identifier().value()});
  output.set_address(AddressToFidl(peer.address()));
  output.set_technology(TechnologyTypeToFidl(peer.technology()));
  output.set_connected(peer.connected());
  output.set_bonded(peer.bonded());

  if (peer.name()) {
    output.set_name(*peer.name());
  }

  if (peer.le()) {
    const std::optional<std::reference_wrapper<const bt::AdvertisingData>> adv_data =
        peer.le()->parsed_advertising_data();
    if (adv_data.has_value()) {
      if (adv_data->get().appearance().has_value()) {
        if (auto appearance = AppearanceToFidl(adv_data->get().appearance().value())) {
          output.set_appearance(appearance.value());
        } else {
          bt_log(DEBUG, "fidl", "omitting unencodeable appearance %#.4x of peer %s",
                 adv_data->get().appearance().value(), bt_str(peer.identifier()));
        }
      }
      if (adv_data->get().tx_power()) {
        output.set_tx_power(adv_data->get().tx_power().value());
      }
    }
  }
  if (peer.bredr() && peer.bredr()->device_class()) {
    output.set_device_class(DeviceClassToFidl(*peer.bredr()->device_class()));
  }
  if (peer.rssi() != bt::hci_spec::kRSSIInvalid) {
    output.set_rssi(peer.rssi());
  }

  if (peer.bredr()) {
    std::transform(peer.bredr()->services().begin(), peer.bredr()->services().end(),
                   std::back_inserter(*output.mutable_bredr_services()), UuidToFidl);
  }

  // TODO(fxbug.dev/57344): Populate le_service UUIDs based on GATT results as well as advertising
  // and inquiry data.

  return output;
}

std::optional<bt::DeviceAddress> AddressFromFidlBondingData(
    const fuchsia::bluetooth::sys::BondingData& bond) {
  bt::DeviceAddressBytes bytes(bond.address().bytes);
  bt::DeviceAddress::Type type;
  if (bond.has_bredr_bond()) {
    // A random identity address can only be present in a LE-only bond.
    if (bond.address().type == fbt::AddressType::RANDOM) {
      bt_log(WARN, "fidl", "BR/EDR or Dual-Mode bond cannot have a random identity address!");
      return std::nullopt;
    }
    // TODO(fxbug.dev/2761): We currently assign kBREDR as the address type for dual-mode bonds.
    // This makes address management for dual-mode devices a bit confusing as we have two "public"
    // address types (i.e. kBREDR and kLEPublic). We should align the stack address types with
    // the FIDL address types, such that both kBREDR and kLEPublic are represented as the same
    // kind of "PUBLIC".
    type = bt::DeviceAddress::Type::kBREDR;
  } else {
    type = bond.address().type == fbt::AddressType::RANDOM ? bt::DeviceAddress::Type::kLERandom
                                                           : bt::DeviceAddress::Type::kLEPublic;
  }

  bt::DeviceAddress address(type, bytes);

  if (!address.IsPublic() && !address.IsStaticRandom()) {
    bt_log(ERROR, "fidl", "%s: BondingData address is not public or static random (address: %s)",
           __FUNCTION__, bt_str(address));
    return std::nullopt;
  }

  return address;
}

bt::sm::PairingData LePairingDataFromFidl(bt::DeviceAddress peer_address,
                                          const fuchsia::bluetooth::sys::LeBondData& data) {
  bt::sm::PairingData result;

  if (data.has_peer_ltk()) {
    result.peer_ltk = LtkFromFidl(data.peer_ltk());
  }
  if (data.has_local_ltk()) {
    result.local_ltk = LtkFromFidl(data.local_ltk());
  }
  if (data.has_irk()) {
    result.irk = PeerKeyFromFidl(data.irk());
    // If there is an IRK, there must also be an identity address. Assume that the identity
    // address is the peer address, since the peer address is set to the identity address upon
    // bonding.
    result.identity_address = peer_address;
  }
  if (data.has_csrk()) {
    result.csrk = PeerKeyFromFidl(data.csrk());
  }
  return result;
}

std::optional<bt::sm::LTK> BredrKeyFromFidl(const fsys::BredrBondData& data) {
  if (!data.has_link_key()) {
    return std::nullopt;
  }
  auto key = PeerKeyFromFidl(data.link_key());
  return bt::sm::LTK(key.security(), bt::hci_spec::LinkKey(key.value(), 0, 0));
}

std::vector<bt::UUID> BredrServicesFromFidl(const fuchsia::bluetooth::sys::BredrBondData& data) {
  std::vector<bt::UUID> services_out;
  if (data.has_services()) {
    std::transform(data.services().begin(), data.services().end(), std::back_inserter(services_out),
                   UuidFromFidl);
  }
  return services_out;
}

fuchsia::bluetooth::sys::BondingData PeerToFidlBondingData(const bt::gap::Adapter& adapter,
                                                           const bt::gap::Peer& peer) {
  fsys::BondingData out;

  out.set_identifier(fbt::PeerId{peer.identifier().value()});
  out.set_local_address(
      AddressToFidl(fbt::AddressType::PUBLIC, adapter.state().controller_address));
  out.set_address(AddressToFidl(peer.address()));

  if (peer.name()) {
    out.set_name(*peer.name());
  }

  // LE
  if (peer.le() && peer.le()->bond_data()) {
    fsys::LeBondData out_le;
    const bt::sm::PairingData& bond = *peer.le()->bond_data();

    // TODO(armansito): Store the peer's preferred connection parameters.
    // TODO(fxbug.dev/59645): Store GATT and AD service UUIDs.

    if (bond.local_ltk) {
      out_le.set_local_ltk(LtkToFidl(*bond.local_ltk));
    }
    if (bond.peer_ltk) {
      out_le.set_peer_ltk(LtkToFidl(*bond.peer_ltk));
    }
    if (bond.irk) {
      out_le.set_irk(PeerKeyToFidl(*bond.irk));
    }
    if (bond.csrk) {
      out_le.set_csrk(PeerKeyToFidl(*bond.csrk));
    }

    out.set_le_bond(std::move(out_le));
  }

  // BR/EDR
  if (peer.bredr() && peer.bredr()->link_key()) {
    fsys::BredrBondData out_bredr;

    // TODO(fxbug.dev/1262): Populate with history of role switches.

    const auto& services = peer.bredr()->services();
    std::transform(services.begin(), services.end(),
                   std::back_inserter(*out_bredr.mutable_services()), UuidToFidl);
    out_bredr.set_link_key(LtkToFidlPeerKey(*peer.bredr()->link_key()));
    out.set_bredr_bond(std::move(out_bredr));
  }

  return out;
}

fble::RemoteDevicePtr NewLERemoteDevice(const bt::gap::Peer& peer) {
  bt::AdvertisingData ad;
  if (!peer.le()) {
    return nullptr;
  }

  auto fidl_device = std::make_unique<fble::RemoteDevice>();
  fidl_device->identifier = peer.identifier().ToString();
  fidl_device->connectable = peer.connectable();

  // Initialize advertising data only if its non-empty.
  const std::optional<std::reference_wrapper<const bt::AdvertisingData>> adv_data =
      peer.le()->parsed_advertising_data();
  if (adv_data.has_value()) {
    auto data = fidl_helpers::AdvertisingDataToFidlDeprecated(adv_data.value());
    fidl_device->advertising_data =
        std::make_unique<fble::AdvertisingDataDeprecated>(std::move(data));
  } else if (peer.le()->advertising_data_error().has_value()) {
    // If the peer advertising data has failed to parse, then this conversion failed.
    return nullptr;
  }

  if (peer.rssi() != bt::hci_spec::kRSSIInvalid) {
    fidl_device->rssi = std::make_unique<Int8>();
    fidl_device->rssi->value = peer.rssi();
  }

  return fidl_device;
}

bool IsScanFilterValid(const fble::ScanFilter& fidl_filter) {
  // |service_uuids| and |service_data_uuids| are the only fields that can potentially contain
  // invalid data, since they are represented as strings.
  if (fidl_filter.service_uuids) {
    for (const auto& uuid_str : *fidl_filter.service_uuids) {
      if (!bt::IsStringValidUuid(uuid_str)) {
        return false;
      }
    }
  }

  if (fidl_filter.service_data_uuids) {
    for (const auto& uuid_str : *fidl_filter.service_data_uuids) {
      if (!bt::IsStringValidUuid(uuid_str))
        return false;
    }
  }

  return true;
}

bool PopulateDiscoveryFilter(const fble::ScanFilter& fidl_filter,
                             bt::gap::DiscoveryFilter* out_filter) {
  BT_DEBUG_ASSERT(out_filter);

  if (fidl_filter.service_uuids) {
    std::vector<bt::UUID> uuids;
    for (const auto& uuid_str : *fidl_filter.service_uuids) {
      bt::UUID uuid;
      if (!bt::StringToUuid(uuid_str, &uuid)) {
        bt_log(WARN, "fidl", "invalid service UUID given to scan filter: %s", uuid_str.c_str());
        return false;
      }
      uuids.push_back(uuid);
    }

    if (!uuids.empty())
      out_filter->set_service_uuids(uuids);
  }

  if (fidl_filter.service_data_uuids) {
    std::vector<bt::UUID> uuids;
    for (const auto& uuid_str : *fidl_filter.service_data_uuids) {
      bt::UUID uuid;
      if (!bt::StringToUuid(uuid_str, &uuid)) {
        bt_log(WARN, "fidl", "invalid service data UUID given to scan filter: %s",
               uuid_str.c_str());
        return false;
      }
      uuids.push_back(uuid);
    }

    if (!uuids.empty())
      out_filter->set_service_data_uuids(uuids);
  }

  if (fidl_filter.connectable) {
    out_filter->set_connectable(fidl_filter.connectable->value);
  }

  if (fidl_filter.manufacturer_identifier) {
    out_filter->set_manufacturer_code(fidl_filter.manufacturer_identifier->value);
  }

  if (fidl_filter.name_substring && !fidl_filter.name_substring.value_or("").empty()) {
    out_filter->set_name_substring(fidl_filter.name_substring.value_or(""));
  }

  if (fidl_filter.max_path_loss) {
    out_filter->set_pathloss(fidl_filter.max_path_loss->value);
  }

  return true;
}

bt::gap::DiscoveryFilter DiscoveryFilterFromFidl(
    const fuchsia::bluetooth::le::Filter& fidl_filter) {
  bt::gap::DiscoveryFilter out;

  if (fidl_filter.has_service_uuid()) {
    out.set_service_uuids({bt::UUID(fidl_filter.service_uuid().value)});
  }

  if (fidl_filter.has_service_data_uuid()) {
    out.set_service_data_uuids({bt::UUID(fidl_filter.service_data_uuid().value)});
  }

  if (fidl_filter.has_manufacturer_id()) {
    out.set_manufacturer_code(fidl_filter.manufacturer_id());
  }

  if (fidl_filter.has_connectable()) {
    out.set_connectable(fidl_filter.connectable());
  }

  if (fidl_filter.has_name()) {
    out.set_name_substring(fidl_filter.name());
  }

  if (fidl_filter.has_max_path_loss()) {
    out.set_pathloss(fidl_filter.max_path_loss());
  }

  return out;
}

bt::gap::AdvertisingInterval AdvertisingIntervalFromFidl(fble::AdvertisingModeHint mode_hint) {
  switch (mode_hint) {
    case fble::AdvertisingModeHint::VERY_FAST:
      return bt::gap::AdvertisingInterval::FAST1;
    case fble::AdvertisingModeHint::FAST:
      return bt::gap::AdvertisingInterval::FAST2;
    case fble::AdvertisingModeHint::SLOW:
      return bt::gap::AdvertisingInterval::SLOW;
  }
  return bt::gap::AdvertisingInterval::SLOW;
}

std::optional<bt::AdvertisingData> AdvertisingDataFromFidl(const fble::AdvertisingData& input) {
  bt::AdvertisingData output;

  if (input.has_name()) {
    if (!output.SetLocalName(input.name())) {
      return std::nullopt;
    }
  }
  if (input.has_appearance()) {
    output.SetAppearance(static_cast<uint16_t>(input.appearance()));
  }
  if (input.has_tx_power_level()) {
    output.SetTxPower(input.tx_power_level());
  }
  if (input.has_service_uuids()) {
    for (const auto& uuid : input.service_uuids()) {
      bt::UUID bt_uuid = UuidFromFidl(uuid);
      if (!output.AddServiceUuid(bt_uuid)) {
        bt_log(WARN, "fidl",
               "Received more Service UUIDs than fit in a single AD - truncating UUID %s",
               bt_str(bt_uuid));
      }
    }
  }
  if (input.has_service_data()) {
    for (const auto& entry : input.service_data()) {
      if (!output.SetServiceData(UuidFromFidl(entry.uuid), bt::BufferView(entry.data))) {
        return std::nullopt;
      }
    }
  }
  if (input.has_manufacturer_data()) {
    for (const auto& entry : input.manufacturer_data()) {
      bt::BufferView data(entry.data);
      if (!output.SetManufacturerData(entry.company_id, data)) {
        return std::nullopt;
      }
    }
  }
  if (input.has_uris()) {
    for (const auto& uri : input.uris()) {
      if (!output.AddUri(uri)) {
        return std::nullopt;
      }
    }
  }

  return output;
}

fble::AdvertisingData AdvertisingDataToFidl(const bt::AdvertisingData& input) {
  fble::AdvertisingData output;

  if (input.local_name()) {
    output.set_name(input.local_name()->name);
  }
  if (input.appearance()) {
    // TODO(fxbug.dev/66358): Remove this to allow for passing arbitrary appearance values to
    // clients in a way that's forward-compatible with future BLE revisions.
    const uint16_t appearance_raw = input.appearance().value();
    if (auto appearance = AppearanceToFidl(appearance_raw)) {
      output.set_appearance(appearance.value());
    } else {
      bt_log(DEBUG, "fidl", "omitting unencodeable appearance %#.4x of peer %s", appearance_raw,
             input.local_name().has_value() ? input.local_name()->name.c_str() : "");
    }
  }
  if (input.tx_power()) {
    output.set_tx_power_level(*input.tx_power());
  }
  std::unordered_set<bt::UUID> service_uuids = input.service_uuids();
  if (!service_uuids.empty()) {
    std::vector<fbt::Uuid> uuids;
    uuids.reserve(service_uuids.size());
    for (const auto& uuid : service_uuids) {
      uuids.push_back(fbt::Uuid{uuid.value()});
    }
    output.set_service_uuids(std::move(uuids));
  }
  if (!input.service_data_uuids().empty()) {
    std::vector<fble::ServiceData> entries;
    for (const auto& uuid : input.service_data_uuids()) {
      auto data = input.service_data(uuid);
      fble::ServiceData entry{fbt::Uuid{uuid.value()}, data.ToVector()};
      entries.push_back(std::move(entry));
    }
    output.set_service_data(std::move(entries));
  }
  if (!input.manufacturer_data_ids().empty()) {
    std::vector<fble::ManufacturerData> entries;
    for (const auto& id : input.manufacturer_data_ids()) {
      auto data = input.manufacturer_data(id);
      fble::ManufacturerData entry{id, data.ToVector()};
      entries.push_back(std::move(entry));
    }
    output.set_manufacturer_data(std::move(entries));
  }
  if (!input.uris().empty()) {
    std::vector<std::string> uris;
    for (const auto& uri : input.uris()) {
      uris.push_back(uri);
    }
    output.set_uris(std::move(uris));
  }

  return output;
}

fble::AdvertisingDataDeprecated AdvertisingDataToFidlDeprecated(const bt::AdvertisingData& input) {
  fble::AdvertisingDataDeprecated output;

  if (input.local_name()) {
    output.name = input.local_name()->name;
  }
  if (input.appearance()) {
    output.appearance = std::make_unique<fbt::UInt16>();
    output.appearance->value = *input.appearance();
  }
  if (input.tx_power()) {
    output.tx_power_level = std::make_unique<fbt::Int8>();
    output.tx_power_level->value = *input.tx_power();
  }
  if (!input.service_uuids().empty()) {
    output.service_uuids.emplace();
    for (const auto& uuid : input.service_uuids()) {
      output.service_uuids->push_back(uuid.ToString());
    }
  }
  if (!input.service_data_uuids().empty()) {
    output.service_data.emplace();
    for (const auto& uuid : input.service_data_uuids()) {
      auto data = input.service_data(uuid);
      fble::ServiceDataEntry entry{uuid.ToString(), data.ToVector()};
      output.service_data->push_back(std::move(entry));
    }
  }
  if (!input.manufacturer_data_ids().empty()) {
    output.manufacturer_specific_data.emplace();
    for (const auto& id : input.manufacturer_data_ids()) {
      auto data = input.manufacturer_data(id);
      fble::ManufacturerSpecificDataEntry entry{id, data.ToVector()};
      output.manufacturer_specific_data->push_back(std::move(entry));
    }
  }
  if (!input.uris().empty()) {
    output.uris.emplace();
    for (const auto& uri : input.uris()) {
      output.uris->push_back(uri);
    }
  }

  return output;
}

fuchsia::bluetooth::le::ScanData AdvertisingDataToFidlScanData(
    const bt::AdvertisingData& input, pw::chrono::SystemClock::time_point timestamp) {
  // Reuse bt::AdvertisingData -> fble::AdvertisingData utility, since most fields are the same as
  // fble::ScanData.
  fble::AdvertisingData fidl_adv_data = AdvertisingDataToFidl(input);
  fble::ScanData out;
  if (fidl_adv_data.has_tx_power_level()) {
    out.set_tx_power(fidl_adv_data.tx_power_level());
  }
  if (fidl_adv_data.has_appearance()) {
    out.set_appearance(fidl_adv_data.appearance());
  }
  if (fidl_adv_data.has_service_uuids()) {
    out.set_service_uuids(std::move(*fidl_adv_data.mutable_service_uuids()));
  }
  if (fidl_adv_data.has_service_data()) {
    out.set_service_data(std::move(*fidl_adv_data.mutable_service_data()));
  }
  if (fidl_adv_data.has_manufacturer_data()) {
    out.set_manufacturer_data(std::move(*fidl_adv_data.mutable_manufacturer_data()));
  }
  if (fidl_adv_data.has_uris()) {
    out.set_uris(std::move(*fidl_adv_data.mutable_uris()));
  }
  zx_time_t timestamp_ns = timestamp.time_since_epoch().count();
  out.set_timestamp(timestamp_ns);
  return out;
}

fble::Peer PeerToFidlLe(const bt::gap::Peer& peer) {
  BT_ASSERT(peer.le());

  fble::Peer output;
  output.set_id(fbt::PeerId{peer.identifier().value()});
  output.set_connectable(peer.connectable());

  if (peer.rssi() != bt::hci_spec::kRSSIInvalid) {
    output.set_rssi(peer.rssi());
  }

  const std::optional<std::reference_wrapper<const bt::AdvertisingData>> advertising_data =
      peer.le()->parsed_advertising_data();
  if (advertising_data.has_value()) {
    std::optional<pw::chrono::SystemClock::time_point> timestamp =
        peer.le()->parsed_advertising_data_timestamp();
    output.set_advertising_data(AdvertisingDataToFidl(advertising_data.value()));
    output.set_data(AdvertisingDataToFidlScanData(advertising_data.value(), timestamp.value()));
  }

  if (peer.name()) {
    output.set_name(peer.name().value());
  }

  output.set_bonded(peer.bonded());
  zx_time_t last_updated_ns = peer.last_updated().time_since_epoch().count();
  output.set_last_updated(last_updated_ns);

  return output;
}

bt::gatt::ReliableMode ReliableModeFromFidl(const fgatt::WriteOptions& write_options) {
  return (write_options.has_reliable_mode() &&
          write_options.reliable_mode() == fgatt::ReliableMode::ENABLED)
             ? bt::gatt::ReliableMode::kEnabled
             : bt::gatt::ReliableMode::kDisabled;
}

// TODO(fxbug.dev/63438): The 64 bit `fidl_gatt_id` can overflow the 16 bits of a bt:att::Handle
// that underlies CharacteristicHandles when directly casted. Fix this.
bt::gatt::CharacteristicHandle CharacteristicHandleFromFidl(uint64_t fidl_gatt_id) {
  if (fidl_gatt_id > std::numeric_limits<bt::att::Handle>::max()) {
    bt_log(ERROR, "fidl",
           "Casting a 64-bit FIDL GATT ID with `bits[16, 63] != 0` (0x%lX) to 16-bit "
           "Characteristic Handle",
           fidl_gatt_id);
  }
  return bt::gatt::CharacteristicHandle(static_cast<bt::att::Handle>(fidl_gatt_id));
}

// TODO(fxbug.dev/63438): The 64 bit `fidl_gatt_id` can overflow the 16 bits of a bt:att::Handle
// that underlies DescriptorHandles when directly casted. Fix this.
bt::gatt::DescriptorHandle DescriptorHandleFromFidl(uint64_t fidl_gatt_id) {
  if (fidl_gatt_id > std::numeric_limits<bt::att::Handle>::max()) {
    bt_log(ERROR, "fidl",
           "Casting a 64-bit FIDL GATT ID with `bits[16, 63] != 0` (0x%lX) to 16-bit Descriptor "
           "Handle",
           fidl_gatt_id);
  }
  return bt::gatt::DescriptorHandle(static_cast<bt::att::Handle>(fidl_gatt_id));
}

fpromise::result<bt::sdp::ServiceRecord, fuchsia::bluetooth::ErrorCode>
ServiceDefinitionToServiceRecord(const fuchsia::bluetooth::bredr::ServiceDefinition& definition) {
  bt::sdp::ServiceRecord rec;
  std::vector<bt::UUID> classes;

  if (!definition.has_service_class_uuids()) {
    bt_log(WARN, "fidl", "Advertised service contains no Service UUIDs");
    return fpromise::error(fuchsia::bluetooth::ErrorCode::INVALID_ARGUMENTS);
  }

  for (auto& uuid : definition.service_class_uuids()) {
    bt::UUID btuuid = fidl_helpers::UuidFromFidl(uuid);
    bt_log(TRACE, "fidl", "Setting Service Class UUID %s", bt_str(btuuid));
    classes.emplace_back(btuuid);
  }

  rec.SetServiceClassUUIDs(classes);

  if (definition.has_protocol_descriptor_list()) {
    if (!AddProtocolDescriptorList(&rec, bt::sdp::ServiceRecord::kPrimaryProtocolList,
                                   definition.protocol_descriptor_list())) {
      bt_log(ERROR, "fidl", "Failed to add protocol descriptor list");
      return fpromise::error(fuchsia::bluetooth::ErrorCode::INVALID_ARGUMENTS);
    }
  }

  if (definition.has_additional_protocol_descriptor_lists()) {
    // It's safe to iterate through this list with a ProtocolListId as ProtocolListId = uint8_t,
    // and std::numeric_limits<uint8_t>::max() == 255 == the MAX_SEQUENCE_LENGTH vector limit from
    // fuchsia.bluetooth.bredr/ServiceDefinition.additional_protocol_descriptor_lists.
    BT_ASSERT(definition.additional_protocol_descriptor_lists().size() <=
              std::numeric_limits<bt::sdp::ServiceRecord::ProtocolListId>::max());
    bt::sdp::ServiceRecord::ProtocolListId protocol_list_id = 1;
    for (const auto& descriptor_list : definition.additional_protocol_descriptor_lists()) {
      if (!AddProtocolDescriptorList(&rec, protocol_list_id, descriptor_list)) {
        bt_log(ERROR, "fidl", "Failed to add additional protocol descriptor list");
        return fpromise::error(fuchsia::bluetooth::ErrorCode::INVALID_ARGUMENTS);
      }
      protocol_list_id++;
    }
  }

  if (definition.has_profile_descriptors()) {
    for (const auto& profile : definition.profile_descriptors()) {
      bt_log(TRACE, "fidl", "Adding Profile %#hx v%d.%d",
             static_cast<unsigned short>(profile.profile_id), profile.major_version,
             profile.minor_version);
      rec.AddProfile(bt::UUID(static_cast<uint16_t>(profile.profile_id)), profile.major_version,
                     profile.minor_version);
    }
  }

  if (definition.has_information()) {
    for (const auto& info : definition.information()) {
      if (!info.has_language()) {
        return fpromise::error(fuchsia::bluetooth::ErrorCode::INVALID_ARGUMENTS);
      }
      const std::string& language = info.language();
      std::string name, description, provider;
      if (info.has_name()) {
        name = info.name();
      }
      if (info.has_description()) {
        description = info.description();
      }
      if (info.has_provider()) {
        provider = info.provider();
      }
      bt_log(TRACE, "fidl", "Adding Info (%s): (%s, %s, %s)", language.c_str(), name.c_str(),
             description.c_str(), provider.c_str());
      rec.AddInfo(language, name, description, provider);
    }
  }

  if (definition.has_additional_attributes()) {
    for (const auto& attribute : definition.additional_attributes()) {
      auto elem = FidlToDataElement(attribute.element);
      if (elem) {
        bt_log(TRACE, "fidl", "Adding attribute %#x : %s", attribute.id,
               elem.value().ToString().c_str());
        rec.SetAttribute(attribute.id, std::move(elem.value()));
      }
    }
  }
  return fpromise::ok(std::move(rec));
}

bt::gap::BrEdrSecurityRequirements FidlToBrEdrSecurityRequirements(
    const fbredr::ChannelParameters& fidl) {
  bt::gap::BrEdrSecurityRequirements requirements{.authentication = false,
                                                  .secure_connections = false};
  if (fidl.has_security_requirements()) {
    if (fidl.security_requirements().has_authentication_required()) {
      requirements.authentication = fidl.security_requirements().authentication_required();
    }

    if (fidl.security_requirements().has_secure_connections_required()) {
      requirements.secure_connections = fidl.security_requirements().secure_connections_required();
    }
  }
  return requirements;
}

bt::sco::ParameterSet FidlToScoParameterSet(const fbredr::HfpParameterSet param_set) {
  switch (param_set) {
    case fbredr::HfpParameterSet::T1:
      return bt::sco::kParameterSetT1;
    case fbredr::HfpParameterSet::T2:
      return bt::sco::kParameterSetT2;
    case fbredr::HfpParameterSet::S1:
      return bt::sco::kParameterSetS1;
    case fbredr::HfpParameterSet::S2:
      return bt::sco::kParameterSetS2;
    case fbredr::HfpParameterSet::S3:
      return bt::sco::kParameterSetS3;
    case fbredr::HfpParameterSet::S4:
      return bt::sco::kParameterSetS4;
    case fbredr::HfpParameterSet::D0:
      return bt::sco::kParameterSetD0;
    case fbredr::HfpParameterSet::D1:
      return bt::sco::kParameterSetD1;
  }
}

bt::StaticPacket<pw::bluetooth::emboss::SynchronousConnectionParameters::VendorCodingFormatWriter>
FidlToScoCodingFormat(const fbredr::CodingFormat format) {
  bt::StaticPacket<pw::bluetooth::emboss::SynchronousConnectionParameters::VendorCodingFormatWriter>
      out;
  auto view = out.view();
  // Set to 0 since vendor specific coding formats are not supported.
  view.company_id().Write(0);
  view.vendor_codec_id().Write(0);
  switch (format) {
    case fbredr::CodingFormat::ALAW:
      view.coding_format().Write(pw::bluetooth::emboss::CodingFormat::A_LAW);
      break;
    case fbredr::CodingFormat::MULAW:
      view.coding_format().Write(pw::bluetooth::emboss::CodingFormat::U_LAW);
      break;
    case fbredr::CodingFormat::CVSD:
      view.coding_format().Write(pw::bluetooth::emboss::CodingFormat::CVSD);
      break;
    case fbredr::CodingFormat::TRANSPARENT:
      view.coding_format().Write(pw::bluetooth::emboss::CodingFormat::TRANSPARENT);
      break;
    case fbredr::CodingFormat::LINEAR_PCM:
      view.coding_format().Write(pw::bluetooth::emboss::CodingFormat::LINEAR_PCM);
      break;
    case fbredr::CodingFormat::MSBC:
      view.coding_format().Write(pw::bluetooth::emboss::CodingFormat::MSBC);
      break;
    case fbredr::CodingFormat::LC3:
      view.coding_format().Write(pw::bluetooth::emboss::CodingFormat::LC3);
      break;
    case fbredr::CodingFormat::G729A:
      view.coding_format().Write(pw::bluetooth::emboss::CodingFormat::G729A);
      break;
  }
  return out;
}

fpromise::result<pw::bluetooth::emboss::PcmDataFormat> FidlToPcmDataFormat(
    const faudio::SampleFormat& format) {
  switch (format) {
    case faudio::SampleFormat::PCM_SIGNED:
      return fpromise::ok(pw::bluetooth::emboss::PcmDataFormat::TWOS_COMPLEMENT);
    case faudio::SampleFormat::PCM_UNSIGNED:
      return fpromise::ok(pw::bluetooth::emboss::PcmDataFormat::UNSIGNED);
    default:
      // Other sample formats are not supported by SCO.
      return fpromise::error();
  }
}

pw::bluetooth::emboss::ScoDataPath FidlToScoDataPath(const fbredr::DataPath& path) {
  switch (path) {
    case fbredr::DataPath::HOST:
      return pw::bluetooth::emboss::ScoDataPath::HCI;
    case fbredr::DataPath::OFFLOAD: {
      // TODO(fxbug.dev/58458): Use path from stack configuration file instead of this hardcoded
      // value. "6" is the data path usually used in Broadcom controllers.
      return static_cast<pw::bluetooth::emboss::ScoDataPath>(6);
    }
    case fbredr::DataPath::TEST:
      return pw::bluetooth::emboss::ScoDataPath::AUDIO_TEST_MODE;
  }
}

fpromise::result<bt::StaticPacket<pw::bluetooth::emboss::SynchronousConnectionParametersWriter>>
FidlToScoParameters(const fbredr::ScoConnectionParameters& params) {
  bt::StaticPacket<pw::bluetooth::emboss::SynchronousConnectionParametersWriter> out;
  auto view = out.view();

  if (!params.has_parameter_set()) {
    bt_log(WARN, "fidl", "SCO parameters missing parameter_set");
    return fpromise::error();
  }
  auto param_set = FidlToScoParameterSet(params.parameter_set());
  view.transmit_bandwidth().Write(param_set.transmit_receive_bandwidth);
  view.receive_bandwidth().Write(param_set.transmit_receive_bandwidth);

  if (!params.has_air_coding_format()) {
    bt_log(WARN, "fidl", "SCO parameters missing air_coding_format");
    return fpromise::error();
  }
  auto air_coding_format = FidlToScoCodingFormat(params.air_coding_format());
  view.transmit_coding_format().CopyFrom(air_coding_format.view());
  view.receive_coding_format().CopyFrom(air_coding_format.view());

  if (!params.has_air_frame_size()) {
    bt_log(WARN, "fidl", "SCO parameters missing air_frame_size");
    return fpromise::error();
  }
  view.transmit_codec_frame_size_bytes().Write(params.air_frame_size());
  view.receive_codec_frame_size_bytes().Write(params.air_frame_size());

  if (!params.has_io_bandwidth()) {
    bt_log(WARN, "fidl", "SCO parameters missing io_bandwidth");
    return fpromise::error();
  }
  view.input_bandwidth().Write(params.io_bandwidth());
  view.output_bandwidth().Write(params.io_bandwidth());

  if (!params.has_io_coding_format()) {
    bt_log(WARN, "fidl", "SCO parameters missing io_coding_format");
    return fpromise::error();
  }
  auto io_coding_format = FidlToScoCodingFormat(params.io_coding_format());
  view.input_coding_format().CopyFrom(io_coding_format.view());
  view.output_coding_format().CopyFrom(io_coding_format.view());

  if (!params.has_io_frame_size()) {
    bt_log(WARN, "fidl", "SCO parameters missing io_frame_size");
    return fpromise::error();
  }
  view.input_coded_data_size_bits().Write(params.io_frame_size());
  view.output_coded_data_size_bits().Write(params.io_frame_size());

  if (params.has_io_pcm_data_format() && view.input_coding_format().coding_format().Read() ==
                                             pw::bluetooth::emboss::CodingFormat::LINEAR_PCM) {
    auto io_pcm_format = FidlToPcmDataFormat(params.io_pcm_data_format());
    if (io_pcm_format.is_error()) {
      bt_log(WARN, "fidl", "Unsupported IO PCM data format in SCO parameters");
      return fpromise::error();
    }
    view.input_pcm_data_format().Write(io_pcm_format.value());
    view.output_pcm_data_format().Write(io_pcm_format.value());

  } else if (view.input_coding_format().coding_format().Read() ==
             pw::bluetooth::emboss::CodingFormat::LINEAR_PCM) {
    bt_log(WARN, "fidl",
           "SCO parameters missing io_pcm_data_format (required for linear PCM IO coding format)");
    return fpromise::error();
  } else {
    view.input_pcm_data_format().Write(pw::bluetooth::emboss::PcmDataFormat::NOT_APPLICABLE);
    view.output_pcm_data_format().Write(pw::bluetooth::emboss::PcmDataFormat::NOT_APPLICABLE);
  }

  if (params.has_io_pcm_sample_payload_msb_position() &&
      view.input_coding_format().coding_format().Read() ==
          pw::bluetooth::emboss::CodingFormat::LINEAR_PCM) {
    view.input_pcm_sample_payload_msb_position().Write(params.io_pcm_sample_payload_msb_position());
    view.output_pcm_sample_payload_msb_position().Write(
        params.io_pcm_sample_payload_msb_position());
  } else {
    view.input_pcm_sample_payload_msb_position().Write(0u);
    view.output_pcm_sample_payload_msb_position().Write(0u);
  }

  if (!params.has_path()) {
    bt_log(WARN, "fidl", "SCO parameters missing data path");
    return fpromise::error();
  }
  auto path = FidlToScoDataPath(params.path());
  view.input_data_path().Write(path);
  view.output_data_path().Write(path);

  // For HCI Host transport the transport unit size should be "0". For PCM transport the unit size
  // is vendor specific. A unit size of "0" indicates "not applicable".
  // TODO(fxbug.dev/58458): Use unit size from stack configuration file instead of hardcoding "not
  // applicable".
  view.input_transport_unit_size_bits().Write(0u);
  view.output_transport_unit_size_bits().Write(0u);

  view.max_latency_ms().Write(param_set.max_latency_ms);
  view.packet_types().BackingStorage().WriteUInt(param_set.packet_types);
  view.retransmission_effort().Write(
      static_cast<pw::bluetooth::emboss::SynchronousConnectionParameters::ScoRetransmissionEffort>(
          param_set.retransmission_effort));

  return fpromise::ok(out);
}

fpromise::result<
    std::vector<bt::StaticPacket<pw::bluetooth::emboss::SynchronousConnectionParametersWriter>>>
FidlToScoParametersVector(const std::vector<fbredr::ScoConnectionParameters>& params) {
  std::vector<bt::StaticPacket<pw::bluetooth::emboss::SynchronousConnectionParametersWriter>> out;
  out.reserve(params.size());
  for (const fbredr::ScoConnectionParameters& param : params) {
    fpromise::result<bt::StaticPacket<pw::bluetooth::emboss::SynchronousConnectionParametersWriter>>
        result = FidlToScoParameters(param);
    if (result.is_error()) {
      return fpromise::error();
    }
    out.push_back(result.take_value());
  }
  return fpromise::ok(std::move(out));
}

bool IsFidlGattHandleValid(fuchsia::bluetooth::gatt2::Handle handle) {
  if (handle.value > std::numeric_limits<bt::att::Handle>::max()) {
    bt_log(ERROR, "fidl", "Invalid 64-bit FIDL GATT ID with `bits[16, 63] != 0` (0x%lX)",
           handle.value);
    return false;
  }
  return true;
}

bool IsFidlGattServiceHandleValid(fuchsia::bluetooth::gatt2::ServiceHandle handle) {
  if (handle.value > std::numeric_limits<bt::att::Handle>::max()) {
    bt_log(ERROR, "fidl", "Invalid 64-bit FIDL GATT ID with `bits[16, 63] != 0` (0x%lX)",
           handle.value);
    return false;
  }
  return true;
}

fuchsia::bluetooth::bredr::RxPacketStatus ScoPacketStatusToFidl(
    bt::hci_spec::SynchronousDataPacketStatusFlag status) {
  switch (status) {
    case bt::hci_spec::SynchronousDataPacketStatusFlag::kCorrectlyReceived:
      return fuchsia::bluetooth::bredr::RxPacketStatus::CORRECTLY_RECEIVED_DATA;
    case bt::hci_spec::SynchronousDataPacketStatusFlag::kPossiblyInvalid:
      return fuchsia::bluetooth::bredr::RxPacketStatus::POSSIBLY_INVALID_DATA;
    case bt::hci_spec::SynchronousDataPacketStatusFlag::kNoDataReceived:
      return fuchsia::bluetooth::bredr::RxPacketStatus::NO_DATA_RECEIVED;
    case bt::hci_spec::SynchronousDataPacketStatusFlag::kDataPartiallyLost:
      return fuchsia::bluetooth::bredr::RxPacketStatus::DATA_PARTIALLY_LOST;
  }
}

bt::att::ErrorCode Gatt2ErrorCodeFromFidl(fgatt2::Error error_code) {
  switch (error_code) {
    case fgatt2::Error::INVALID_HANDLE:
      return bt::att::ErrorCode::kInvalidHandle;
    case fgatt2::Error::READ_NOT_PERMITTED:
      return bt::att::ErrorCode::kReadNotPermitted;
    case fgatt2::Error::WRITE_NOT_PERMITTED:
      return bt::att::ErrorCode::kWriteNotPermitted;
    case fgatt2::Error::INVALID_OFFSET:
      return bt::att::ErrorCode::kInvalidOffset;
    case fgatt2::Error::INVALID_ATTRIBUTE_VALUE_LENGTH:
      return bt::att::ErrorCode::kInvalidAttributeValueLength;
    case fgatt2::Error::INSUFFICIENT_RESOURCES:
      return bt::att::ErrorCode::kInsufficientResources;
    case fgatt2::Error::VALUE_NOT_ALLOWED:
      return bt::att::ErrorCode::kValueNotAllowed;
    default:
      break;
  }
  return bt::att::ErrorCode::kUnlikelyError;
}

bt::att::AccessRequirements Gatt2AccessRequirementsFromFidl(
    const fuchsia::bluetooth::gatt2::SecurityRequirements& reqs) {
  return bt::att::AccessRequirements(
      reqs.has_encryption_required() ? reqs.encryption_required() : false,
      reqs.has_authentication_required() ? reqs.authentication_required() : false,
      reqs.has_authorization_required() ? reqs.authorization_required() : false);
}

std::unique_ptr<bt::gatt::Descriptor> Gatt2DescriptorFromFidl(const fgatt2::Descriptor& fidl_desc) {
  if (!fidl_desc.has_permissions()) {
    bt_log(WARN, "fidl", "FIDL descriptor missing required `permissions` field");
    return nullptr;
  }
  const fgatt2::AttributePermissions& perm = fidl_desc.permissions();
  bt::att::AccessRequirements read_reqs = perm.has_read()
                                              ? Gatt2AccessRequirementsFromFidl(perm.read())
                                              : bt::att::AccessRequirements();
  bt::att::AccessRequirements write_reqs = perm.has_write()
                                               ? Gatt2AccessRequirementsFromFidl(perm.write())
                                               : bt::att::AccessRequirements();

  if (!fidl_desc.has_type()) {
    bt_log(WARN, "fidl", "FIDL descriptor missing required `type` field");
    return nullptr;
  }
  bt::UUID type(fidl_desc.type().value);

  if (!fidl_desc.has_handle()) {
    bt_log(WARN, "fidl", "FIDL characteristic missing required `handle` field");
    return nullptr;
  }
  return std::make_unique<bt::gatt::Descriptor>(fidl_desc.handle().value, type, read_reqs,
                                                write_reqs);
}

std::unique_ptr<bt::gatt::Characteristic> Gatt2CharacteristicFromFidl(
    const fgatt2::Characteristic& fidl_chrc) {
  if (!fidl_chrc.has_properties()) {
    bt_log(WARN, "fidl", "FIDL characteristic missing required `properties` field");
    return nullptr;
  }
  if (!fidl_chrc.has_permissions()) {
    bt_log(WARN, "fidl", "FIDL characteristic missing required `permissions` field");
    return nullptr;
  }
  if (!fidl_chrc.has_type()) {
    bt_log(WARN, "fidl", "FIDL characteristic missing required `type` field");
    return nullptr;
  }
  if (!fidl_chrc.has_handle()) {
    bt_log(WARN, "fidl", "FIDL characteristic missing required `handle` field");
    return nullptr;
  }

  uint8_t props = static_cast<uint8_t>(fidl_chrc.properties());
  const uint16_t ext_props = static_cast<uint16_t>(fidl_chrc.properties()) >> CHAR_BIT;
  if (ext_props) {
    props |= bt::gatt::Property::kExtendedProperties;
  }

  const fgatt2::AttributePermissions& permissions = fidl_chrc.permissions();
  bool supports_update =
      (props & bt::gatt::Property::kNotify) || (props & bt::gatt::Property::kIndicate);
  if (supports_update != permissions.has_update()) {
    bt_log(WARN, "fidl", "Characteristic update permission %s",
           supports_update ? "required" : "must be null");
    return nullptr;
  }

  bt::att::AccessRequirements read_reqs = permissions.has_read()
                                              ? Gatt2AccessRequirementsFromFidl(permissions.read())
                                              : bt::att::AccessRequirements();
  bt::att::AccessRequirements write_reqs =
      permissions.has_write() ? Gatt2AccessRequirementsFromFidl(permissions.write())
                              : bt::att::AccessRequirements();
  bt::att::AccessRequirements update_reqs =
      permissions.has_update() ? Gatt2AccessRequirementsFromFidl(permissions.update())
                               : bt::att::AccessRequirements();

  bt::UUID type(fidl_chrc.type().value);

  auto chrc = std::make_unique<bt::gatt::Characteristic>(
      fidl_chrc.handle().value, type, props, ext_props, read_reqs, write_reqs, update_reqs);
  if (fidl_chrc.has_descriptors()) {
    for (const auto& fidl_desc : fidl_chrc.descriptors()) {
      std::unique_ptr<bt::gatt::Descriptor> maybe_desc =
          fidl_helpers::Gatt2DescriptorFromFidl(fidl_desc);
      if (!maybe_desc) {
        // Specific failures are logged in Gatt2DescriptorFromFidl
        return nullptr;
      }

      chrc->AddDescriptor(std::move(maybe_desc));
    }
  }

  return chrc;
}

}  // namespace bthost::fidl_helpers

// static
std::vector<uint8_t> fidl::TypeConverter<std::vector<uint8_t>, bt::ByteBuffer>::Convert(
    const bt::ByteBuffer& from) {
  std::vector<uint8_t> to(from.size());
  bt::MutableBufferView view(to.data(), to.size());
  view.Write(from);
  return to;
}
