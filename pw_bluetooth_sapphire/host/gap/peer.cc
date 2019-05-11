// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peer.h"

#include <zircon/assert.h>

#include "advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_scanner.h"
#include "src/lib/fxl/strings/string_printf.h"

namespace bt {

using common::BufferView;
using common::ByteBuffer;
using common::DeviceAddress;
using common::DynamicByteBuffer;

namespace gap {
namespace {

std::string ConnectionStateToString(Peer::ConnectionState state) {
  switch (state) {
    case Peer::ConnectionState::kNotConnected:
      return "not connected";
    case Peer::ConnectionState::kInitializing:
      return "connecting";
    case Peer::ConnectionState::kConnected:
      return "connected";
  }

  ZX_PANIC("invalid connection state %u", static_cast<unsigned int>(state));
  return "(unknown)";
}

}  // namespace

Peer::LowEnergyData::LowEnergyData(Peer* owner)
    : peer_(owner),
      conn_state_(ConnectionState::kNotConnected),
      adv_data_len_(0u) {
  ZX_DEBUG_ASSERT(peer_);
}

void Peer::LowEnergyData::SetAdvertisingData(int8_t rssi,
                                             const common::ByteBuffer& adv) {
  // Prolong this peer's expiration in case it is temporary.
  peer_->UpdateExpiry();

  bool notify_listeners = peer_->SetRssiInternal(rssi);

  // Update the advertising data
  // TODO(armansito): Validate that the advertising data is not malformed?
  if (adv_data_buffer_.size() < adv.size()) {
    adv_data_buffer_ = DynamicByteBuffer(adv.size());
  }
  adv_data_len_ = adv.size();
  adv.Copy(&adv_data_buffer_);

  // Walk through the advertising data and update common fields.
  AdvertisingDataReader reader(adv);
  gap::DataType type;
  BufferView data;
  while (reader.GetNextField(&type, &data)) {
    if (type == gap::DataType::kCompleteLocalName ||
        type == gap::DataType::kShortenedLocalName) {
      // TODO(armansito): Parse more advertising data fields, such as preferred
      // connection parameters.
      // TODO(NET-607): SetName should be a no-op if a name was obtained via
      // the name discovery procedure.
      if (peer_->SetNameInternal(data.ToString())) {
        notify_listeners = true;
      }
    }
  }

  if (notify_listeners) {
    peer_->UpdateExpiry();
    peer_->NotifyListeners();
  }
}

void Peer::LowEnergyData::SetConnectionState(ConnectionState state) {
  ZX_DEBUG_ASSERT(peer_->connectable() ||
                  state == ConnectionState::kNotConnected);

  if (state == connection_state()) {
    bt_log(TRACE, "gap-le", "LE connection state already \"%s\"!",
           ConnectionStateToString(state).c_str());
    return;
  }

  bt_log(TRACE, "gap-le",
         "peer (%s) LE connection state changed from \"%s\" to \"%s\"",
         bt_str(peer_->identifier()),
         ConnectionStateToString(connection_state()).c_str(),
         ConnectionStateToString(state).c_str());

  conn_state_ = state;

  // Become non-temporary if connected or a connection attempt is in progress.
  // Otherwise, become temporary again if the identity is unknown.
  if (state == ConnectionState::kInitializing ||
      state == ConnectionState::kConnected) {
    peer_->TryMakeNonTemporary();
  } else if (state == ConnectionState::kNotConnected &&
             !peer_->identity_known()) {
    bt_log(TRACE, "gap", "became temporary: %s:", bt_str(*peer_));
    peer_->temporary_ = true;
  }

  peer_->UpdateExpiry();
  peer_->NotifyListeners();
}

void Peer::LowEnergyData::SetConnectionParameters(
    const hci::LEConnectionParameters& params) {
  ZX_DEBUG_ASSERT(peer_->connectable());
  conn_params_ = params;
}

void Peer::LowEnergyData::SetPreferredConnectionParameters(
    const hci::LEPreferredConnectionParameters& params) {
  ZX_DEBUG_ASSERT(peer_->connectable());
  preferred_conn_params_ = params;
}

void Peer::LowEnergyData::SetBondData(const sm::PairingData& bond_data) {
  ZX_DEBUG_ASSERT(peer_->connectable());
  ZX_DEBUG_ASSERT(peer_->address().type() != DeviceAddress::Type::kLEAnonymous);

  // Make sure the peer is non-temporary.
  peer_->TryMakeNonTemporary();

  // This will mark the peer as bonded
  bond_data_ = bond_data;

  // Update to the new identity address if the current address is random.
  if (peer_->address().type() == DeviceAddress::Type::kLERandom &&
      bond_data.identity_address) {
    peer_->set_identity_known(true);
    peer_->set_address(*bond_data.identity_address);
  }

  peer_->NotifyListeners();
}

void Peer::LowEnergyData::ClearBondData() {
  ZX_ASSERT(bond_data_);
  if (bond_data_->irk) {
    peer_->set_identity_known(false);
  }
  bond_data_ = std::nullopt;
}

Peer::BrEdrData::BrEdrData(Peer* owner)
    : peer_(owner), conn_state_(ConnectionState::kNotConnected), eir_len_(0u) {
  ZX_DEBUG_ASSERT(peer_);
  ZX_DEBUG_ASSERT(peer_->identity_known());

  // Devices that are capable of BR/EDR and use a LE random device address will
  // end up with separate entries for the BR/EDR and LE addresses.
  ZX_DEBUG_ASSERT(peer_->address().type() != DeviceAddress::Type::kLERandom &&
                  peer_->address().type() != DeviceAddress::Type::kLEAnonymous);
  address_ = {DeviceAddress::Type::kBREDR, peer_->address().value()};
}

void Peer::BrEdrData::SetInquiryData(const hci::InquiryResult& value) {
  ZX_DEBUG_ASSERT(peer_->address().value() == value.bd_addr);
  SetInquiryData(value.class_of_device, value.clock_offset,
                 value.page_scan_repetition_mode);
}

void Peer::BrEdrData::SetInquiryData(const hci::InquiryResultRSSI& value) {
  ZX_DEBUG_ASSERT(peer_->address().value() == value.bd_addr);
  SetInquiryData(value.class_of_device, value.clock_offset,
                 value.page_scan_repetition_mode, value.rssi);
}

void Peer::BrEdrData::SetInquiryData(
    const hci::ExtendedInquiryResultEventParams& value) {
  ZX_DEBUG_ASSERT(peer_->address().value() == value.bd_addr);
  SetInquiryData(value.class_of_device, value.clock_offset,
                 value.page_scan_repetition_mode, value.rssi,
                 BufferView(value.extended_inquiry_response,
                            sizeof(value.extended_inquiry_response)));
}

void Peer::BrEdrData::SetConnectionState(ConnectionState state) {
  ZX_DEBUG_ASSERT(peer_->connectable() ||
                  state == ConnectionState::kNotConnected);

  if (state == connection_state()) {
    bt_log(TRACE, "gap-bredr", "BR/EDR connection state already \"%s\"",
           ConnectionStateToString(state).c_str());
    return;
  }

  bt_log(TRACE, "gap-bredr",
         "peer (%s) BR/EDR connection state changed from \"%s\" to \"%s\"",
         bt_str(peer_->identifier()),
         ConnectionStateToString(connection_state()).c_str(),
         ConnectionStateToString(state).c_str());

  conn_state_ = state;
  peer_->UpdateExpiry();
  peer_->NotifyListeners();

  // Become non-temporary if we became connected. BR/EDR device remain
  // non-temporary afterwards.
  if (state == ConnectionState::kConnected) {
    peer_->TryMakeNonTemporary();
  }
}

void Peer::BrEdrData::SetInquiryData(
    common::DeviceClass device_class, uint16_t clock_offset,
    hci::PageScanRepetitionMode page_scan_rep_mode, int8_t rssi,
    const common::BufferView& eir_data) {
  peer_->UpdateExpiry();

  bool notify_listeners = false;

  // TODO(armansito): Consider sending notifications for RSSI updates perhaps
  // with throttling to avoid spamming.
  peer_->SetRssiInternal(rssi);

  page_scan_rep_mode_ = page_scan_rep_mode;
  clock_offset_ = static_cast<uint16_t>(hci::kClockOffsetValidFlagBit |
                                        le16toh(clock_offset));

  if (!device_class_ || *device_class_ != device_class) {
    device_class_ = device_class;
    notify_listeners = true;
  }

  if (eir_data.size() && SetEirData(eir_data)) {
    notify_listeners = true;
  }

  if (notify_listeners) {
    peer_->NotifyListeners();
  }
}

bool Peer::BrEdrData::SetEirData(const common::ByteBuffer& eir) {
  ZX_DEBUG_ASSERT(eir.size());

  // TODO(armansito): Validate that the EIR data is not malformed?
  if (eir_buffer_.size() < eir.size()) {
    eir_buffer_ = DynamicByteBuffer(eir.size());
  }
  eir_len_ = eir.size();
  eir.Copy(&eir_buffer_);

  // TODO(jamuraa): maybe rename this class?
  AdvertisingDataReader reader(eir);
  gap::DataType type;
  common::BufferView data;
  bool changed = false;
  while (reader.GetNextField(&type, &data)) {
    if (type == gap::DataType::kCompleteLocalName) {
      // TODO(armansito): Parse more fields.
      // TODO(armansito): SetName should be a no-op if a name was obtained via
      // the name discovery procedure.
      changed = peer_->SetNameInternal(data.ToString());
    }
  }
  return changed;
}

void Peer::BrEdrData::SetBondData(const sm::LTK& link_key) {
  ZX_DEBUG_ASSERT(peer_->connectable());

  // Make sure the peer is non-temporary.
  peer_->TryMakeNonTemporary();

  // Storing the key establishes the bond.
  link_key_ = link_key;

  peer_->NotifyListeners();
}

void Peer::BrEdrData::ClearBondData() {
  ZX_ASSERT(link_key_);
  link_key_ = std::nullopt;
}

Peer::Peer(DeviceCallback notify_listeners_callback,
           DeviceCallback update_expiry_callback,
           DeviceCallback dual_mode_callback, DeviceId identifier,
           const DeviceAddress& address, bool connectable)
    : notify_listeners_callback_(std::move(notify_listeners_callback)),
      update_expiry_callback_(std::move(update_expiry_callback)),
      dual_mode_callback_(std::move(dual_mode_callback)),
      identifier_(identifier),
      technology_((address.type() == DeviceAddress::Type::kBREDR)
                      ? TechnologyType::kClassic
                      : TechnologyType::kLowEnergy),
      address_(address),
      identity_known_(false),
      connectable_(connectable),
      temporary_(true),
      rssi_(hci::kRSSIInvalid) {
  ZX_DEBUG_ASSERT(notify_listeners_callback_);
  ZX_DEBUG_ASSERT(update_expiry_callback_);
  ZX_DEBUG_ASSERT(dual_mode_callback_);
  ZX_DEBUG_ASSERT(identifier.IsValid());

  if (address.type() == DeviceAddress::Type::kBREDR ||
      address.type() == DeviceAddress::Type::kLEPublic) {
    identity_known_ = true;
  }

  // Initialize transport-specific state.
  if (technology_ == TechnologyType::kClassic) {
    bredr_data_ = BrEdrData(this);
  } else {
    le_data_ = LowEnergyData(this);
  }
}

Peer::LowEnergyData& Peer::MutLe() {
  if (le_data_) {
    return *le_data_;
  }

  le_data_ = LowEnergyData(this);

  // Make dual-mode if both transport states have been initialized.
  if (bredr_data_) {
    MakeDualMode();
  }
  return *le_data_;
}

Peer::BrEdrData& Peer::MutBrEdr() {
  if (bredr_data_) {
    return *bredr_data_;
  }

  bredr_data_ = BrEdrData(this);

  // Make dual-mode if both transport states have been initialized.
  if (le_data_) {
    MakeDualMode();
  }
  return *bredr_data_;
}

std::string Peer::ToString() const {
  return fxl::StringPrintf("{peer id: %s, address: %s}", bt_str(identifier_),
                           bt_str(address_));
}

void Peer::SetName(const std::string& name) {
  if (SetNameInternal(name)) {
    UpdateExpiry();
    NotifyListeners();
  }
}

// Private methods below:

bool Peer::SetRssiInternal(int8_t rssi) {
  if (rssi != hci::kRSSIInvalid && rssi_ != rssi) {
    rssi_ = rssi;
    return true;
  }
  return false;
}

bool Peer::SetNameInternal(const std::string& name) {
  if (!name_ || *name_ != name) {
    name_ = name;
    return true;
  }
  return false;
}

bool Peer::TryMakeNonTemporary() {
  // TODO(armansito): Since we don't currently support address resolution,
  // random addresses should never be persisted.
  if (!connectable()) {
    bt_log(TRACE, "gap", "remains temporary: %s", ToString().c_str());
    return false;
  }

  bt_log(TRACE, "gap", "became non-temporary: %s:", ToString().c_str());

  if (temporary_) {
    temporary_ = false;
    UpdateExpiry();
    NotifyListeners();
  }

  return true;
}

void Peer::UpdateExpiry() {
  ZX_DEBUG_ASSERT(update_expiry_callback_);
  update_expiry_callback_(*this);
}

void Peer::NotifyListeners() {
  ZX_DEBUG_ASSERT(notify_listeners_callback_);
  notify_listeners_callback_(*this);
}

void Peer::MakeDualMode() {
  technology_ = TechnologyType::kDualMode;
  ZX_DEBUG_ASSERT(dual_mode_callback_);
  dual_mode_callback_(*this);
}

}  // namespace gap
}  // namespace bt
