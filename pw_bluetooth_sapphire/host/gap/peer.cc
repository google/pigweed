// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peer.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/common/manufacturer_names.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_scanner.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"
#include "src/lib/fxl/strings/string_printf.h"

namespace bt {

namespace gap {

std::string Peer::ConnectionStateToString(Peer::ConnectionState state) {
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

Peer::LowEnergyData::LowEnergyData(Peer* owner, inspect::Node node)
    : peer_(owner),
      node_(std::move(node)),
      conn_state_(ConnectionState::kNotConnected,
                  node_.CreateString(LowEnergyData::kInspectConnectionStateName, ""),
                  &ConnectionStateToString),
      bond_data_(std::nullopt, node_.CreateBool(LowEnergyData::kInspectBondDataName, false),
                 [](const std::optional<sm::PairingData>& p) { return p.has_value(); }),
      auto_conn_behavior_(AutoConnectBehavior::kAlways),
      features_(std::nullopt, node_.CreateString(LowEnergyData::kInspectFeaturesName, ""),
                [](const std::optional<hci::LESupportedFeatures> f) {
                  return f ? fxl::StringPrintf("%#.16lx", f->le_features) : "";
                }) {
  ZX_DEBUG_ASSERT(peer_);
}

void Peer::LowEnergyData::SetAutoConnectBehaviorForIntentionalDisconnect(void) {
  auto_conn_behavior_ = AutoConnectBehavior::kSkipUntilNextConnection;
}

void Peer::LowEnergyData::SetAutoConnectBehaviorForSuccessfulConnection(void) {
  auto_conn_behavior_ = AutoConnectBehavior::kAlways;
}

void Peer::LowEnergyData::SetAdvertisingData(int8_t rssi, const ByteBuffer& adv) {
  adv_data_buffer_ = DynamicByteBuffer(adv.size());
  adv.Copy(&adv_data_buffer_);

  ProcessNewAdvertisingData(rssi, adv);
}

void Peer::LowEnergyData::AppendScanResponse(int8_t rssi, const ByteBuffer& scan_response) {
  if (scan_response.size() == 0u) {
    bt_log(DEBUG, "gap-le", "ignored empty scan response");
    return;
  }

  DynamicByteBuffer buffer(adv_data_buffer_.size() + scan_response.size());
  buffer.Write(adv_data_buffer_);
  buffer.Write(scan_response, adv_data_buffer_.size());
  adv_data_buffer_ = std::move(buffer);

  ProcessNewAdvertisingData(rssi, scan_response);
}

void Peer::LowEnergyData::ProcessNewAdvertisingData(int8_t rssi, const ByteBuffer& new_data) {
  // Prolong this peer's expiration in case it is temporary.
  peer_->UpdateExpiry();

  bool notify_listeners = peer_->SetRssiInternal(rssi);

  // Walk through the advertising data and update common fields.
  // TODO(armansito): Validate that the advertising data is not malformed?
  SupplementDataReader reader(new_data);
  DataType type;
  BufferView data;
  while (reader.GetNextField(&type, &data)) {
    if (type == DataType::kCompleteLocalName || type == DataType::kShortenedLocalName) {
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
  ZX_DEBUG_ASSERT(peer_->connectable() || state == ConnectionState::kNotConnected);

  if (state == connection_state()) {
    bt_log(TRACE, "gap-le", "LE connection state already \"%s\"!",
           ConnectionStateToString(state).c_str());
    return;
  }

  bt_log(TRACE, "gap-le", "peer (%s) LE connection state changed from \"%s\" to \"%s\"",
         bt_str(peer_->identifier()), ConnectionStateToString(connection_state()).c_str(),
         ConnectionStateToString(state).c_str());

  conn_state_.Set(state);

  // Become non-temporary if connected or a connection attempt is in progress.
  // Otherwise, become temporary again if the identity is unknown.
  if (state == ConnectionState::kInitializing || state == ConnectionState::kConnected) {
    peer_->TryMakeNonTemporary();
  } else if (state == ConnectionState::kNotConnected && !peer_->identity_known()) {
    bt_log(TRACE, "gap", "became temporary: %s:", bt_str(*peer_));
    peer_->temporary_.Set(true);
  }

  peer_->UpdateExpiry();
  peer_->NotifyListeners();
}

void Peer::LowEnergyData::SetConnectionParameters(const hci::LEConnectionParameters& params) {
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
  bond_data_.Set(bond_data);

  // Update to the new identity address if the current address is random.
  if (peer_->address().type() == DeviceAddress::Type::kLERandom && bond_data.identity_address) {
    peer_->set_identity_known(true);
    peer_->set_address(*bond_data.identity_address);
  }

  peer_->NotifyListeners();
}

void Peer::LowEnergyData::ClearBondData() {
  ZX_ASSERT(bond_data_->has_value());
  if (bond_data_->value().irk) {
    peer_->set_identity_known(false);
  }
  bond_data_.Set(std::nullopt);
}

Peer::BrEdrData::BrEdrData(Peer* owner, inspect::Node node)
    : peer_(owner),
      node_(std::move(node)),
      conn_state_(ConnectionState::kNotConnected,
                  node_.CreateString(BrEdrData::kInspectConnectionStateName, ""),
                  &ConnectionStateToString),
      eir_len_(0u),
      link_key_(std::nullopt, node_.CreateBool(BrEdrData::kInspectLinkKeyName, false),
                [](const std::optional<sm::LTK>& l) { return l.has_value(); }) {
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
  SetInquiryData(value.class_of_device, value.clock_offset, value.page_scan_repetition_mode);
}

void Peer::BrEdrData::SetInquiryData(const hci::InquiryResultRSSI& value) {
  ZX_DEBUG_ASSERT(peer_->address().value() == value.bd_addr);
  SetInquiryData(value.class_of_device, value.clock_offset, value.page_scan_repetition_mode,
                 value.rssi);
}

void Peer::BrEdrData::SetInquiryData(const hci::ExtendedInquiryResultEventParams& value) {
  ZX_DEBUG_ASSERT(peer_->address().value() == value.bd_addr);
  SetInquiryData(
      value.class_of_device, value.clock_offset, value.page_scan_repetition_mode, value.rssi,
      BufferView(value.extended_inquiry_response, sizeof(value.extended_inquiry_response)));
}

void Peer::BrEdrData::SetConnectionState(ConnectionState state) {
  ZX_DEBUG_ASSERT(peer_->connectable() || state == ConnectionState::kNotConnected);

  if (state == connection_state()) {
    bt_log(TRACE, "gap-bredr", "BR/EDR connection state already \"%s\"",
           ConnectionStateToString(state).c_str());
    return;
  }

  bt_log(TRACE, "gap-bredr", "peer (%s) BR/EDR connection state changed from \"%s\" to \"%s\"",
         bt_str(peer_->identifier()), ConnectionStateToString(connection_state()).c_str(),
         ConnectionStateToString(state).c_str());

  conn_state_.Set(state);
  peer_->UpdateExpiry();
  peer_->NotifyListeners();

  // Become non-temporary if we became connected. BR/EDR device remain
  // non-temporary afterwards.
  if (state == ConnectionState::kConnected) {
    peer_->TryMakeNonTemporary();
  }
}

void Peer::BrEdrData::SetInquiryData(DeviceClass device_class, uint16_t clock_offset,
                                     hci::PageScanRepetitionMode page_scan_rep_mode, int8_t rssi,
                                     const BufferView& eir_data) {
  peer_->UpdateExpiry();

  bool notify_listeners = false;

  // TODO(armansito): Consider sending notifications for RSSI updates perhaps
  // with throttling to avoid spamming.
  peer_->SetRssiInternal(rssi);

  page_scan_rep_mode_ = page_scan_rep_mode;
  clock_offset_ = static_cast<uint16_t>(hci::kClockOffsetValidFlagBit | le16toh(clock_offset));

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

bool Peer::BrEdrData::SetEirData(const ByteBuffer& eir) {
  ZX_DEBUG_ASSERT(eir.size());

  // TODO(armansito): Validate that the EIR data is not malformed?
  if (eir_buffer_.size() < eir.size()) {
    eir_buffer_ = DynamicByteBuffer(eir.size());
  }
  eir_len_ = eir.size();
  eir.Copy(&eir_buffer_);

  SupplementDataReader reader(eir);
  DataType type;
  BufferView data;
  bool changed = false;
  while (reader.GetNextField(&type, &data)) {
    if (type == DataType::kCompleteLocalName) {
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
  link_key_.Set(link_key);

  peer_->NotifyListeners();
}

void Peer::BrEdrData::ClearBondData() {
  ZX_ASSERT(link_key_->has_value());
  link_key_.Set(std::nullopt);
}

Peer::Peer(DeviceCallback notify_listeners_callback, DeviceCallback update_expiry_callback,
           DeviceCallback dual_mode_callback, PeerId identifier, const DeviceAddress& address,
           bool connectable, inspect::Node node)
    : node_(std::move(node)),
      notify_listeners_callback_(std::move(notify_listeners_callback)),
      update_expiry_callback_(std::move(update_expiry_callback)),
      dual_mode_callback_(std::move(dual_mode_callback)),
      identifier_(identifier, node_.CreateString(Peer::kInspectPeerIdName, "")),
      technology_((address.type() == DeviceAddress::Type::kBREDR) ? TechnologyType::kClassic
                                                                  : TechnologyType::kLowEnergy,
                  node_.CreateString(Peer::kInspectTechnologyName, ""),
                  [](TechnologyType t) { return TechnologyTypeToString(t); }),
      address_(address, node_.CreateString(Peer::kInspectAddressName, "")),
      identity_known_(false),
      lmp_version_(std::nullopt, node_.CreateString(Peer::kInspectVersionName, ""),
                   [](const std::optional<hci::HCIVersion>& v) {
                     return v ? hci::HCIVersionToString(*v) : "";
                   }),
      lmp_manufacturer_(
          std::nullopt, node_.CreateString(Peer::kInspectManufacturerName, ""),
          [](const std::optional<uint16_t>& m) { return m ? GetManufacturerName(*m) : ""; }),
      lmp_features_(hci::LMPFeatureSet(), node_.CreateString(Peer::kInspectFeaturesName, "")),
      connectable_(connectable, node_.CreateBool(Peer::kInspectConnectableName, "")),
      temporary_(true, node_.CreateBool(Peer::kInspectTemporaryName, true)),
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
  if (*technology_ == TechnologyType::kClassic) {
    bredr_data_ = BrEdrData(this, node_.CreateChild(Peer::BrEdrData::kInspectNodeName));
  } else {
    le_data_ = LowEnergyData(this, node_.CreateChild(Peer::LowEnergyData::kInspectNodeName));
  }
}

Peer::LowEnergyData& Peer::MutLe() {
  if (le_data_) {
    return *le_data_;
  }

  le_data_ = LowEnergyData(this, node_.CreateChild(Peer::LowEnergyData::kInspectNodeName));

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

  bredr_data_ = BrEdrData(this, node_.CreateChild(Peer::BrEdrData::kInspectNodeName));

  // Make dual-mode if both transport states have been initialized.
  if (le_data_) {
    MakeDualMode();
  }
  return *bredr_data_;
}

std::string Peer::ToString() const {
  return fxl::StringPrintf("{peer id: %s, address: %s}", bt_str(*identifier_), bt_str(*address_));
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
    bt_log(TRACE, "gap", "remains temporary: %s", bt_str(*this));
    return false;
  }

  bt_log(TRACE, "gap", "became non-temporary: %s:", bt_str(*this));

  if (*temporary_) {
    temporary_.Set(false);
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
  technology_.Set(TechnologyType::kDualMode);
  ZX_DEBUG_ASSERT(dual_mode_callback_);
  dual_mode_callback_(*this);
}

}  // namespace gap
}  // namespace bt
