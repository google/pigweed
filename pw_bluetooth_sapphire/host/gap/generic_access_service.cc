// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "generic_access_service.h"

#include <lib/async/cpp/task.h>
#include <lib/async/default.h>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <limits>

#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/types.h"

namespace bt {
namespace gap {

namespace {
// Value of 0xFFFF indicates no specific minimum for some connection
// parameters.
constexpr uint16_t kUnspecifiedValue = 0xFFFF;
}  // namespace

constexpr UUID GenericAccessService::kServiceUUID;
constexpr uint64_t GenericAccessService::kDisplayNameId;
constexpr UUID GenericAccessService::kDisplayNameUUID;
constexpr uint64_t GenericAccessService::kAppearanceId;
constexpr UUID GenericAccessService::kAppearanceUUID;
constexpr uint64_t
    GenericAccessService::kPeripheralPreferredConnectionParametersId;
constexpr UUID
    GenericAccessService::kPeripheralPreferredConnectionParametersUUID;
constexpr size_t GenericAccessService::kMaxDeviceNameLength;

// Logs by the Generic Access Service will be tagged with
// |BT_LOG_GAS_TAG|.
const char *BT_LOG_GAS_TAG = "gas";

GenericAccessService::GenericAccessService(fbl::RefPtr<gatt::GATT> gatt)
    : weak_factory_(this),
      gatt_(gatt),
      service_id_(gatt::kInvalidId),
      device_name_("fuchsia"),
      appearance_(AppearanceCategory::kUnknown) {
  ZX_DEBUG_ASSERT(gatt_);
  Register();
}

void GenericAccessService::UpdateDeviceName(std::string device_name) {
  if (device_name.size() >= kMaxDeviceNameLength) {
    bt_log(TRACE, BT_LOG_GAS_TAG,
           "New device name too long, shortening from %lu bytes to %lu bytes",
           device_name.size(), kMaxDeviceNameLength);
    device_name.resize(kMaxDeviceNameLength);
  }

  device_name_ = std::move(device_name);
}

void GenericAccessService::UpdateAppearance(AppearanceCategory appearance) {
  appearance_ = appearance;
}

bool GenericAccessService::UpdatePreferredConnectionParameters(
    std::optional<hci::LEPreferredConnectionParameters> parameters) {
  // If parameters has a value, validate the connection parameters.
  if (parameters) {
    // Make sure |minimum_connection_interval| is the unspecified value
    // or within the valid range.
    uint16_t min_interval = parameters->min_interval();
    uint16_t max_interval = parameters->max_interval();
    uint16_t max_latency = parameters->max_latency();
    uint16_t supervision_timeout = parameters->supervision_timeout();

    // Make sure |min_interval| is the unspecified value, or within the valid
    // range.
    bool min_conn_int_specified = (min_interval != kUnspecifiedValue);
    if (min_conn_int_specified &&
        ((min_interval < hci::kLEConnectionIntervalMin) ||
         (min_interval > hci::kLEConnectionIntervalMax))) {
      bt_log(
          ERROR, BT_LOG_GAS_TAG,
          "New minimum interval value (%d) is not within the valid range; not "
          "updating preferred connection parameters...",
          min_interval);
      return false;
    }

    // Make sure |max_interval| is the unspecified value, or within the valid
    // range and not less than |minimum_connection_interval|.
    if (max_interval != kUnspecifiedValue) {
      if ((max_interval < hci::kLEConnectionIntervalMin) ||
          (max_interval > hci::kLEConnectionIntervalMax)) {
        bt_log(ERROR, BT_LOG_GAS_TAG,
               "New maximum interval value (%d) is not within the valid range; "
               "not "
               "updating preferred connection parameters...",
               max_interval);
        return false;
      } else if (min_conn_int_specified && (max_interval < min_interval)) {
        bt_log(ERROR, BT_LOG_GAS_TAG,
               "New maximum interval value (%d) is less than new minimum "
               "interval value (%d); not "
               "updating preferred connection parameters...",
               max_interval, min_interval);
        return false;
      }
    }

    // Make sure |max_latency| is within the valid range.
    if (max_latency > hci::kLEConnectionLatencyMax) {
      bt_log(
          ERROR, BT_LOG_GAS_TAG,
          "New maximum latency value (%d) is not within the valid range; not "
          "updating preferred connection parameters...",
          max_latency);
      return false;
    }

    // Make sure |supervision_timeout| is the unspecified value, or within the
    // valid range.
    if ((supervision_timeout != kUnspecifiedValue) &&
        ((supervision_timeout < hci::kLEConnectionSupervisionTimeoutMin) ||
         (supervision_timeout > hci::kLEConnectionSupervisionTimeoutMax))) {
      bt_log(
          ERROR, BT_LOG_GAS_TAG,
          "New supervision timeout value (%d) is not within the valid range; "
          "not updating preferred connection parameters...",
          supervision_timeout);
      return false;
    }
  }

  // Values are OK.
  preferred_connection_parameters_ = std::move(parameters);
  return true;
}

void GenericAccessService::OnReadValue(uint64_t id, int32_t offset,
                                       gatt::ReadResponder callback) {
  bt_log(TRACE, BT_LOG_GAS_TAG, "ReadValue on characteristic %lu at offset %d",
         id, offset);

  if (offset) {
    callback(att::ErrorCode::kInvalidOffset, BufferView());
    return;
  }

  switch (id) {
    case kDisplayNameId:
      OnReadDeviceName(std::move(callback));
      break;

    case kAppearanceId:
      OnReadAppearance(std::move(callback));
      break;

    case kPeripheralPreferredConnectionParametersId:
      // TODO(ghanan): Only return parameters if adapter is a peripheral.
      OnReadPreferredConnectionParameters(std::move(callback));
      break;

    default:
      callback(att::ErrorCode::kReadNotPermitted, BufferView());
      return;
  }
}

void GenericAccessService::Register() {
  ZX_DEBUG_ASSERT(service_id_ == gatt::kInvalidId);

  const att::AccessRequirements kDisallowed;
  const att::AccessRequirements kAllowedNoSecurity(false, false, false);

  auto service = std::make_unique<bt::gatt::Service>(true, kServiceUUID);

  service->AddCharacteristic(std::make_unique<bt::gatt::Characteristic>(
      kDisplayNameId,      // id
      kDisplayNameUUID,    // type
      gatt::kRead,         // properties
      0,                   // extended properties
      kAllowedNoSecurity,  // read
      kDisallowed,         // write
      kDisallowed          // update
      ));

  service->AddCharacteristic(std::make_unique<bt::gatt::Characteristic>(
      kAppearanceId,       // id
      kAppearanceUUID,     // type
      gatt::kRead,         // properties
      0,                   // extended properties
      kAllowedNoSecurity,  // read
      kDisallowed,         // write
      kDisallowed          // update
      ));

  service->AddCharacteristic(std::make_unique<bt::gatt::Characteristic>(
      kPeripheralPreferredConnectionParametersId,    // id
      kPeripheralPreferredConnectionParametersUUID,  // type
      gatt::kRead,                                   // properties
      0,                                             // extended properties
      kAllowedNoSecurity,                            // read
      kDisallowed,                                   // write
      kDisallowed                                    // update
      ));

  auto self = weak_factory_.GetWeakPtr();

  // Set up event handlers.
  auto read_handler = [self](auto svc_id, auto id, auto offset,
                             auto responder) mutable {
    if (self) {
      ZX_DEBUG_ASSERT(self->service_id_ == svc_id);
      self->OnReadValue(id, offset, std::move(responder));
    } else {
      responder(bt::att::ErrorCode::kUnlikelyError, bt::BufferView());
    }
  };
  auto write_handler = [](auto svc_id, auto id, auto offset, const auto &value,
                          auto responder) mutable {
    responder(bt::att::ErrorCode::kUnlikelyError);
  };
  auto ccc_callback = [](auto svc_id, auto id, bt::gatt::PeerId peer_id,
                         bool notify, bool indicate) {};

  auto id_cb = [self](bt::gatt::IdType id) mutable {
    if (!self) {
      return;
    }

    if (!id) {
      bt_log(TRACE, BT_LOG_GAS_TAG,
             "Failed to register generic access service");
    } else {
      bt_log(TRACE, BT_LOG_GAS_TAG,
             "Successfully registered generic access service with id: %lu", id);
      self->service_id_ = id;
    }
  };

  bt_log(TRACE, BT_LOG_GAS_TAG, "Registering generic access service...");

  gatt_->RegisterService(std::move(service), std::move(id_cb),
                         std::move(read_handler), std::move(write_handler),
                         std::move(ccc_callback));
}

void GenericAccessService::Unregister() {
  if (service_id_ == gatt::kInvalidId) {
    // Service is not registered so do nothing.
    bt_log(WARN, BT_LOG_GAS_TAG,
           "Attempted to unregister generic access service but it is already "
           "unregistered; doing nothing");
    return;
  }

  bt_log(TRACE, BT_LOG_GAS_TAG,
         "Unregistering generic access service with id: %lu...", service_id_);
  gatt_->UnregisterService(service_id_);
  service_id_ = gatt::kInvalidId;
}

void GenericAccessService::OnReadDeviceName(gatt::ReadResponder callback) {
  fidl::VectorPtr<uint8_t> value;
  for (auto c : device_name_) {
    value->push_back(c);
  }
  callback(att::ErrorCode::kNoError, BufferView(value->data(), value->size()));
}

void GenericAccessService::OnReadAppearance(gatt::ReadResponder callback) {
  fidl::VectorPtr<uint8_t> value;
  uint16_t appearance = static_cast<uint16_t>(appearance_);
  value->push_back(appearance & 0xFF);
  value->push_back((appearance >> 8) & 0xFF);
  callback(att::ErrorCode::kNoError, BufferView(value->data(), value->size()));
}

void GenericAccessService::OnReadPreferredConnectionParameters(
    gatt::ReadResponder callback) {
  if (!preferred_connection_parameters_) {
    callback(att::ErrorCode::kReadNotPermitted, BufferView());
    return;
  }

  fidl::VectorPtr<uint8_t> value;
  uint16_t min_interval = preferred_connection_parameters_->min_interval();
  uint16_t max_interval = preferred_connection_parameters_->max_interval();
  uint16_t max_latency = preferred_connection_parameters_->max_latency();
  uint16_t supervision_timeout =
      preferred_connection_parameters_->supervision_timeout();

  value->push_back(min_interval & 0xFF);
  value->push_back((min_interval >> 8) & 0xFF);
  value->push_back(max_interval & 0xFF);
  value->push_back((max_interval >> 8) & 0xFF);
  value->push_back(max_latency & 0xFF);
  value->push_back((max_latency >> 8) & 0xFF);
  value->push_back(supervision_timeout & 0xFF);
  value->push_back((supervision_timeout >> 8) & 0xFF);
  callback(att::ErrorCode::kNoError, BufferView(value->data(), value->size()));
}

}  // namespace gap
}  // namespace bt
