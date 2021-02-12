// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peer.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/common/manufacturer_names.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_scanner.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"
#include "src/lib/fxl/strings/string_printf.h"

namespace bt::gap {

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

Peer::LowEnergyData::LowEnergyData(Peer* owner)
    : peer_(owner),
      conn_state_(ConnectionState::kNotConnected, &ConnectionStateToString),
      bond_data_(std::nullopt,
                 [](const std::optional<sm::PairingData>& p) { return p.has_value(); }),
      auto_conn_behavior_(AutoConnectBehavior::kAlways),
      features_(std::nullopt,
                [](const std::optional<hci::LESupportedFeatures> f) {
                  return f ? fxl::StringPrintf("%#.16lx", f->le_features) : "";
                }),
      service_changed_gatt_data_({.notify = false, .indicate = false}) {
  ZX_DEBUG_ASSERT(peer_);
}

void Peer::LowEnergyData::AttachInspect(inspect::Node& parent, std::string name) {
  node_ = parent.CreateChild(name);
  conn_state_.AttachInspect(node_, LowEnergyData::kInspectConnectionStateName);
  bond_data_.AttachInspect(node_, LowEnergyData::kInspectBondDataName);
  features_.AttachInspect(node_, LowEnergyData::kInspectFeaturesName);
}

void Peer::LowEnergyData::SetAdvertisingData(int8_t rssi, const ByteBuffer& adv) {
  // Prolong this peer's expiration in case it is temporary.
  peer_->UpdateExpiry();

  bool notify_listeners = peer_->SetRssiInternal(rssi);

  // Update the advertising data
  // TODO(armansito): Validate that the advertising data is not malformed?
  adv_data_buffer_ = DynamicByteBuffer(adv.size());
  adv.Copy(&adv_data_buffer_);

  // Walk through the advertising data and update common fields.
  SupplementDataReader reader(adv);
  DataType type;
  BufferView data;
  while (reader.GetNextField(&type, &data)) {
    if (type == DataType::kCompleteLocalName || type == DataType::kShortenedLocalName) {
      // TODO(armansito): Parse more advertising data fields, such as preferred
      // connection parameters.
      // TODO(fxbug.dev/793): SetName should be a no-op if a name was obtained via
      // the name discovery procedure.
      if (peer_->SetNameInternal(data.ToString())) {
        notify_listeners = true;
      }
    }
  }

  if (notify_listeners) {
    peer_->UpdateExpiry();
    peer_->NotifyListeners(NotifyListenersChange::kBondNotUpdated);
  }
}

void Peer::LowEnergyData::SetConnectionState(ConnectionState state) {
  ZX_DEBUG_ASSERT(peer_->connectable() || state == ConnectionState::kNotConnected);

  if (state == connection_state()) {
    bt_log(DEBUG, "gap-le", "LE connection state already \"%s\"!",
           ConnectionStateToString(state).c_str());
    return;
  }

  bt_log(DEBUG, "gap-le", "peer (%s) LE connection state changed from \"%s\" to \"%s\"",
         bt_str(peer_->identifier()), ConnectionStateToString(connection_state()).c_str(),
         ConnectionStateToString(state).c_str());

  conn_state_.Set(state);

  // Become non-temporary if connected or a connection attempt is in progress.
  // Otherwise, become temporary again if the identity is unknown.
  if (state == ConnectionState::kInitializing || state == ConnectionState::kConnected) {
    peer_->TryMakeNonTemporary();
  } else if (state == ConnectionState::kNotConnected && !peer_->identity_known()) {
    bt_log(DEBUG, "gap", "became temporary: %s:", bt_str(*peer_));
    peer_->temporary_.Set(true);
  }

  peer_->UpdateExpiry();
  peer_->NotifyListeners(NotifyListenersChange::kBondNotUpdated);
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

  // PeerCache notifies listeners of new bonds, so no need to request that here.
  peer_->NotifyListeners(NotifyListenersChange::kBondNotUpdated);
}

void Peer::LowEnergyData::ClearBondData() {
  ZX_ASSERT(bond_data_->has_value());
  if (bond_data_->value().irk) {
    peer_->set_identity_known(false);
  }
  bond_data_.Set(std::nullopt);
}

Peer::BrEdrData::BrEdrData(Peer* owner)
    : peer_(owner),
      conn_state_(ConnectionState::kNotConnected, &ConnectionStateToString),
      eir_len_(0u),
      link_key_(std::nullopt, [](const std::optional<sm::LTK>& l) { return l.has_value(); }),
      services_({}, MakeContainerOfToStringConvertFunction()) {
  ZX_DEBUG_ASSERT(peer_);
  ZX_DEBUG_ASSERT(peer_->identity_known());

  // Devices that are capable of BR/EDR and use a LE random device address will
  // end up with separate entries for the BR/EDR and LE addresses.
  ZX_DEBUG_ASSERT(peer_->address().type() != DeviceAddress::Type::kLERandom &&
                  peer_->address().type() != DeviceAddress::Type::kLEAnonymous);
  address_ = {DeviceAddress::Type::kBREDR, peer_->address().value()};
}

void Peer::BrEdrData::AttachInspect(inspect::Node& parent, std::string name) {
  node_ = parent.CreateChild(name);
  conn_state_.AttachInspect(node_, BrEdrData::kInspectConnectionStateName);
  link_key_.AttachInspect(node_, BrEdrData::kInspectLinkKeyName);
  services_.AttachInspect(node_, BrEdrData::kInspectServicesName);
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
    bt_log(DEBUG, "gap-bredr", "BR/EDR connection state already \"%s\"",
           ConnectionStateToString(state).c_str());
    return;
  }

  bt_log(DEBUG, "gap-bredr", "peer (%s) BR/EDR connection state changed from \"%s\" to \"%s\"",
         bt_str(peer_->identifier()), ConnectionStateToString(connection_state()).c_str(),
         ConnectionStateToString(state).c_str());

  conn_state_.Set(state);
  peer_->UpdateExpiry();
  peer_->NotifyListeners(NotifyListenersChange::kBondNotUpdated);

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
    peer_->NotifyListeners(NotifyListenersChange::kBondNotUpdated);
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

  // PeerCache notifies listeners of new bonds, so no need to request that here.
  peer_->NotifyListeners(NotifyListenersChange::kBondNotUpdated);
}

void Peer::BrEdrData::ClearBondData() {
  ZX_ASSERT(link_key_->has_value());
  link_key_.Set(std::nullopt);
}

void Peer::BrEdrData::AddService(UUID uuid) {
  auto [_, inserted] = services_.Mutable()->insert(uuid);
  if (inserted) {
    auto update_bond =
        bonded() ? NotifyListenersChange::kBondUpdated : NotifyListenersChange::kBondNotUpdated;
    peer_->NotifyListeners(update_bond);
  }
}

Peer::Peer(NotifyListenersCallback notify_listeners_callback, PeerCallback update_expiry_callback,
           PeerCallback dual_mode_callback, PeerId identifier, const DeviceAddress& address,
           bool connectable)
    : notify_listeners_callback_(std::move(notify_listeners_callback)),
      update_expiry_callback_(std::move(update_expiry_callback)),
      dual_mode_callback_(std::move(dual_mode_callback)),
      identifier_(identifier, MakeToStringInspectConvertFunction()),
      technology_((address.type() == DeviceAddress::Type::kBREDR) ? TechnologyType::kClassic
                                                                  : TechnologyType::kLowEnergy,
                  [](TechnologyType t) { return TechnologyTypeToString(t); }),
      address_(address, MakeToStringInspectConvertFunction()),
      identity_known_(false),
      lmp_version_(std::nullopt,
                   [](const std::optional<hci::HCIVersion>& v) {
                     return v ? hci::HCIVersionToString(*v) : "";
                   }),
      lmp_manufacturer_(
          std::nullopt,
          [](const std::optional<uint16_t>& m) { return m ? GetManufacturerName(*m) : ""; }),
      lmp_features_(hci::LMPFeatureSet(), MakeToStringInspectConvertFunction()),
      connectable_(connectable),
      temporary_(true),
      rssi_(hci::kRSSIInvalid),
      weak_ptr_factory_(this) {
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
    bredr_data_ = BrEdrData(this);
  } else {
    le_data_ = LowEnergyData(this);
  }
}

void Peer::AttachInspect(inspect::Node& parent, std::string name) {
  node_ = parent.CreateChild(name);
  identifier_.AttachInspect(node_, kInspectPeerIdName);
  technology_.AttachInspect(node_, kInspectTechnologyName);
  address_.AttachInspect(node_, kInspectAddressName);
  lmp_version_.AttachInspect(node_, kInspectVersionName);
  lmp_manufacturer_.AttachInspect(node_, kInspectManufacturerName);
  lmp_features_.AttachInspect(node_, kInspectFeaturesName);
  connectable_.AttachInspect(node_, kInspectConnectableName);
  temporary_.AttachInspect(node_, kInspectTemporaryName);

  if (bredr_data_) {
    bredr_data_->AttachInspect(node_, Peer::BrEdrData::kInspectNodeName);
  }
  if (le_data_) {
    le_data_->AttachInspect(node_, Peer::LowEnergyData::kInspectNodeName);
  }
}
Peer::LowEnergyData& Peer::MutLe() {
  if (le_data_) {
    return *le_data_;
  }

  le_data_ = LowEnergyData(this);
  le_data_->AttachInspect(node_);

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
  bredr_data_->AttachInspect(node_);

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

    // TODO(fxbug.dev/61739): Update the bond when this happens
    NotifyListeners(NotifyListenersChange::kBondNotUpdated);
  }
}

void Peer::StoreBrEdrCrossTransportKey(sm::LTK ct_key) {
  if (!bredr_data_.has_value()) {
    // If the peer is LE-only, store the CT key separately until the peer is otherwise marked as
    // dual-mode.
    bredr_cross_transport_key_ = ct_key;
  } else if (!bredr_data_->link_key().has_value() ||
             ct_key.security().IsAsSecureAs(bredr_data_->link_key()->security())) {
    // "The devices shall not overwrite that existing key with a key that is weaker in either
    // strength or MITM protection." (v5.2 Vol. 3 Part C 14.1).
    bredr_data_->SetBondData(ct_key);
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
    bt_log(DEBUG, "gap", "remains temporary: %s", bt_str(*this));
    return false;
  }

  bt_log(DEBUG, "gap", "became non-temporary: %s:", bt_str(*this));

  if (*temporary_) {
    temporary_.Set(false);
    UpdateExpiry();
    NotifyListeners(NotifyListenersChange::kBondNotUpdated);
  }

  return true;
}

void Peer::UpdateExpiry() {
  ZX_DEBUG_ASSERT(update_expiry_callback_);
  update_expiry_callback_(*this);
}

void Peer::NotifyListeners(NotifyListenersChange change) {
  ZX_DEBUG_ASSERT(notify_listeners_callback_);
  notify_listeners_callback_(*this, change);
}

void Peer::MakeDualMode() {
  technology_.Set(TechnologyType::kDualMode);
  if (bredr_cross_transport_key_) {
    ZX_ASSERT(bredr_data_);  // Should only be hit after BR/EDR is already created.
    bredr_data_->SetBondData(*bredr_cross_transport_key_);
    bt_log(DEBUG, "gap-bredr", "restored cross-transport-generated br/edr link key");
    bredr_cross_transport_key_ = std::nullopt;
  }
  ZX_DEBUG_ASSERT(dual_mode_callback_);
  dual_mode_callback_(*this);
}

}  // namespace bt::gap
