// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peer.h"

#include <lib/async/cpp/time.h>
#include <lib/async/default.h>

#include <iomanip>

#include "src/connectivity/bluetooth/core/bt-host/common/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/manufacturer_names.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/util.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_scanner.h"
#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"
#include "src/connectivity/bluetooth/lib/cpp-string/utf_codecs.h"

namespace bt::gap {
namespace {
// To prevent log spam, we only log every Nth failure to parse AdvertisingData from each peer.
// This value controls N.
const int64_t kAdvDataParseFailureWarnLogInterval = 25;
}  // namespace

std::string Peer::ConnectionStateToString(Peer::ConnectionState state) {
  switch (state) {
    case Peer::ConnectionState::kNotConnected:
      return "not connected";
    case Peer::ConnectionState::kInitializing:
      return "connecting";
    case Peer::ConnectionState::kConnected:
      return "connected";
  }

  BT_PANIC("invalid connection state %u", static_cast<unsigned int>(state));
  return "(unknown)";
}

std::string Peer::NameSourceToString(Peer::NameSource name_source) {
  switch (name_source) {
    case Peer::NameSource::kNameDiscoveryProcedure:
      return "Name Discovery Procedure";
    case Peer::NameSource::kAdvertisingDataComplete:
      return "Advertising data (complete)";
    case Peer::NameSource::kAdvertisingDataShortened:
      return "Advertising data (shortened)";
    case Peer::NameSource::kInquiryResultComplete:
      return "Inquiry result (complete)";
    case Peer::NameSource::kInquiryResultShortened:
      return "Inquiry result (shortened)";
    case Peer::NameSource::kGenericAccessService:
      return "Generic Access Service";
    case Peer::NameSource::kUnknown:
      return "Unknown source";
  }

  BT_PANIC("invalid peer name source %u", static_cast<unsigned int>(name_source));
  return "(unknown)";
}

Peer::LowEnergyData::LowEnergyData(Peer* owner)
    : peer_(owner),
      bond_data_(std::nullopt,
                 [](const std::optional<sm::PairingData>& p) { return p.has_value(); }),
      auto_conn_behavior_(AutoConnectBehavior::kAlways),
      features_(std::nullopt,
                [](const std::optional<hci_spec::LESupportedFeatures> f) {
                  return f ? bt_lib_cpp_string::StringPrintf("%#.16lx", f->le_features) : "";
                }),
      service_changed_gatt_data_({.notify = false, .indicate = false}) {
  BT_DEBUG_ASSERT(peer_);
}

void Peer::LowEnergyData::AttachInspect(inspect::Node& parent, std::string name) {
  node_ = parent.CreateChild(name);
  inspect_properties_.connection_state =
      node_.CreateString(LowEnergyData::kInspectConnectionStateName,
                         Peer::ConnectionStateToString(connection_state()));
  inspect_properties_.last_adv_data_parse_failure =
      node_.CreateString(LowEnergyData::kInspectLastAdvertisingDataParseFailureName, "");
  adv_data_parse_failure_count_.AttachInspect(
      node_, LowEnergyData::kInspectAdvertisingDataParseFailureCountName);
  bond_data_.AttachInspect(node_, LowEnergyData::kInspectBondDataName);
  features_.AttachInspect(node_, LowEnergyData::kInspectFeaturesName);
}

void Peer::LowEnergyData::SetAdvertisingData(int8_t rssi, const ByteBuffer& data,
                                             zx::time timestamp) {
  // Prolong this peer's expiration in case it is temporary.
  peer_->UpdateExpiry();

  peer_->SetRssiInternal(rssi);

  // Update the advertising data
  adv_data_buffer_ = DynamicByteBuffer(data.size());
  data.Copy(&adv_data_buffer_);
  AdvertisingData::ParseResult res = AdvertisingData::FromBytes(adv_data_buffer_);
  if (!res.is_ok()) {
    int64_t current_failure_count = *adv_data_parse_failure_count_;
    adv_data_parse_failure_count_.Set(current_failure_count + 1);
    inspect_properties_.last_adv_data_parse_failure.Set(
        AdvertisingData::ParseErrorToString(res.error_value()));
    std::string message = bt_lib_cpp_string::StringPrintf(
        "failed to parse advertising data: %s (peer: %s)",
        bt::AdvertisingData::ParseErrorToString(res.error_value()).c_str(),
        bt_str(peer_->identifier()));
    // To prevent log spam, we only log the first, and then every Nth failure to parse
    // AdvertisingData from each peer at WARN level. Other failures are logged at DEBUG level.
    if (*adv_data_parse_failure_count_ % kAdvDataParseFailureWarnLogInterval == 1) {
      bt_log(WARN, "gap-le", "%s", message.c_str());
    } else {
      bt_log(DEBUG, "gap-le", "%s", message.c_str());
    }
  } else {
    // Only update the adv_timestamp if the AdvertisingData parsed successfully
    adv_timestamp_ = timestamp;

    parsed_adv_data_ = std::move(*res);

    // Do not update the name of bonded peers because advertisements are unauthenticated.
    // TODO(fxbug.dev/85365): Populate more Peer fields with relevant fields from parsed_adv_data_.
    if (!peer_->bonded() && parsed_adv_data_->local_name().has_value()) {
      peer_->RegisterNameInternal(parsed_adv_data_->local_name()->name,
                                  parsed_adv_data_->local_name()->is_complete
                                      ? Peer::NameSource::kAdvertisingDataComplete
                                      : Peer::NameSource::kAdvertisingDataShortened);
    }
  }

  peer_->UpdatePeerAndNotifyListeners(NotifyListenersChange::kBondNotUpdated);
}

Peer::InitializingConnectionToken Peer::LowEnergyData::RegisterInitializingConnection() {
  ConnectionState prev_state = connection_state();
  initializing_tokens_count_++;
  OnConnectionStateMaybeChanged(prev_state);

  auto unregister_cb = [self = peer_->GetWeakPtr(), this] {
    if (!self.is_alive()) {
      return;
    }

    ConnectionState prev_state = connection_state();
    initializing_tokens_count_--;
    OnConnectionStateMaybeChanged(prev_state);
  };

  return InitializingConnectionToken(std::move(unregister_cb));
}

Peer::ConnectionToken Peer::LowEnergyData::RegisterConnection() {
  // The high-level connection state is the same whether one or many registered
  // connections exist, but we track each connection in metrics to support multiple
  // connections to the same peer.
  peer_->peer_metrics_->LogLeConnection();

  ConnectionState prev_state = connection_state();
  connection_tokens_count_++;
  OnConnectionStateMaybeChanged(prev_state);

  auto unregister_cb = [self = peer_->GetWeakPtr(), this] {
    if (!self.is_alive()) {
      return;
    }

    connection_tokens_count_--;
    peer_->peer_metrics_->LogLeDisconnection();
    OnConnectionStateMaybeChanged(/*previous=*/ConnectionState::kConnected);
  };

  return ConnectionToken(std::move(unregister_cb));
}

void Peer::LowEnergyData::SetConnectionParameters(const hci_spec::LEConnectionParameters& params) {
  BT_DEBUG_ASSERT(peer_->connectable());
  conn_params_ = params;
}

void Peer::LowEnergyData::SetPreferredConnectionParameters(
    const hci_spec::LEPreferredConnectionParameters& params) {
  BT_DEBUG_ASSERT(peer_->connectable());
  preferred_conn_params_ = params;
}

bool Peer::LowEnergyData::StoreBond(const sm::PairingData& bond_data) {
  return peer_->store_le_bond_callback_(bond_data);
}

void Peer::LowEnergyData::SetBondData(const sm::PairingData& bond_data) {
  BT_DEBUG_ASSERT(peer_->connectable());
  BT_DEBUG_ASSERT(peer_->address().type() != DeviceAddress::Type::kLEAnonymous);

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
  peer_->UpdatePeerAndNotifyListeners(NotifyListenersChange::kBondNotUpdated);
}

void Peer::LowEnergyData::ClearBondData() {
  BT_ASSERT(bond_data_->has_value());
  if (bond_data_->value().irk) {
    peer_->set_identity_known(false);
  }
  bond_data_.Set(std::nullopt);
}

void Peer::LowEnergyData::OnConnectionStateMaybeChanged(ConnectionState previous) {
  if (connection_state() == previous) {
    return;
  }

  bt_log(DEBUG, "gap-le",
         "peer (%s) LE connection state changed from %s to %s (initializing count: %hu, "
         "connection count: %hu)",
         bt_str(peer_->identifier()), ConnectionStateToString(previous).c_str(),
         ConnectionStateToString(connection_state()).c_str(), initializing_tokens_count_,
         connection_tokens_count_);

  inspect_properties_.connection_state.Set(ConnectionStateToString(connection_state()));

  if (previous == ConnectionState::kNotConnected) {
    peer_->TryMakeNonTemporary();
  } else if (connection_state() == ConnectionState::kNotConnected) {
    peer_->TryMakeTemporary();
  }

  peer_->UpdateExpiry();
  peer_->UpdatePeerAndNotifyListeners(NotifyListenersChange::kBondNotUpdated);
}

Peer::BrEdrData::BrEdrData(Peer* owner)
    : peer_(owner),
      eir_len_(0u),
      link_key_(std::nullopt, [](const std::optional<sm::LTK>& l) { return l.has_value(); }),
      services_({}, MakeContainerOfToStringConvertFunction()) {
  BT_DEBUG_ASSERT(peer_);
  BT_DEBUG_ASSERT(peer_->identity_known());

  // Devices that are capable of BR/EDR and use a LE random device address will
  // end up with separate entries for the BR/EDR and LE addresses.
  BT_DEBUG_ASSERT(peer_->address().type() != DeviceAddress::Type::kLERandom &&
                  peer_->address().type() != DeviceAddress::Type::kLEAnonymous);
  address_ = {DeviceAddress::Type::kBREDR, peer_->address().value()};
}

void Peer::BrEdrData::AttachInspect(inspect::Node& parent, std::string name) {
  node_ = parent.CreateChild(name);
  inspect_properties_.connection_state = node_.CreateString(
      BrEdrData::kInspectConnectionStateName, ConnectionStateToString(connection_state()));
  link_key_.AttachInspect(node_, BrEdrData::kInspectLinkKeyName);
  services_.AttachInspect(node_, BrEdrData::kInspectServicesName);
}

void Peer::BrEdrData::SetInquiryData(const pw::bluetooth::emboss::InquiryResultView& view) {
  BT_DEBUG_ASSERT(peer_->address().value() == DeviceAddressBytes{view.bd_addr()});
  SetInquiryData(DeviceClass(view.class_of_device().BackingStorage().ReadUInt()),
                 view.clock_offset().BackingStorage().ReadUInt(),
                 view.page_scan_repetition_mode().Read());
}

void Peer::BrEdrData::SetInquiryData(const hci_spec::InquiryResultRSSI& value) {
  BT_DEBUG_ASSERT(peer_->address().value() == value.bd_addr);
  SetInquiryData(value.class_of_device, value.clock_offset, value.page_scan_repetition_mode,
                 value.rssi);
}

void Peer::BrEdrData::SetInquiryData(const hci_spec::ExtendedInquiryResultEventParams& value) {
  BT_DEBUG_ASSERT(peer_->address().value() == value.bd_addr);
  SetInquiryData(
      value.class_of_device, value.clock_offset, value.page_scan_repetition_mode, value.rssi,
      BufferView(value.extended_inquiry_response, sizeof(value.extended_inquiry_response)));
}

Peer::InitializingConnectionToken Peer::BrEdrData::RegisterInitializingConnection() {
  BT_ASSERT(!connected());

  ConnectionState prev_state = connection_state();
  initializing_tokens_count_++;
  OnConnectionStateMaybeChanged(prev_state);

  return InitializingConnectionToken([self = peer_->GetWeakPtr(), this] {
    if (!self.is_alive()) {
      return;
    }

    ConnectionState prev_state = connection_state();
    initializing_tokens_count_--;
    OnConnectionStateMaybeChanged(prev_state);
  });
}

Peer::ConnectionToken Peer::BrEdrData::RegisterConnection() {
  BT_ASSERT_MSG(
      !connected(),
      "attempt to register BR/EDR connection when a connection is already registered (peer: %s)",
      bt_str(peer_->identifier()));

  ConnectionState prev_state = connection_state();
  connection_tokens_count_++;
  OnConnectionStateMaybeChanged(prev_state);

  return ConnectionToken([self = peer_->GetWeakPtr(), this] {
    if (!self.is_alive()) {
      return;
    }

    ConnectionState prev_state = connection_state();
    connection_tokens_count_--;
    OnConnectionStateMaybeChanged(prev_state);
  });
}

void Peer::BrEdrData::OnConnectionStateMaybeChanged(ConnectionState previous) {
  if (previous == connection_state()) {
    return;
  }

  bt_log(DEBUG, "gap-bredr", "peer (%s) BR/EDR connection state changed from \"%s\" to \"%s\"",
         bt_str(peer_->identifier()), ConnectionStateToString(previous).c_str(),
         ConnectionStateToString(connection_state()).c_str());
  inspect_properties_.connection_state.Set(ConnectionStateToString(connection_state()));

  if (connection_state() == ConnectionState::kConnected) {
    peer_->peer_metrics_->LogBrEdrConnection();
  } else if (previous == ConnectionState::kConnected) {
    peer_->peer_metrics_->LogBrEdrDisconnection();
  }

  peer_->UpdateExpiry();

  // Transition to or from kConnected state is a notifyable change.
  if (previous == ConnectionState::kConnected ||
      connection_state() == ConnectionState::kConnected) {
    peer_->UpdatePeerAndNotifyListeners(NotifyListenersChange::kBondNotUpdated);
  }

  // Become non-temporary if we successfully connect or are initializing. BR/EDR device remain
  // non-temporary afterwards if bonded, and temporary again if disconnect without bonding.
  if (connection_state() == ConnectionState::kNotConnected) {
    peer_->TryMakeTemporary();
  } else {
    peer_->TryMakeNonTemporary();
  }
}

void Peer::BrEdrData::SetInquiryData(
    DeviceClass device_class, uint16_t clock_offset,
    pw::bluetooth::emboss::PageScanRepetitionMode page_scan_rep_mode, int8_t rssi,
    const BufferView& eir_data) {
  peer_->UpdateExpiry();

  bool notify_listeners = false;

  // TODO(armansito): Consider sending notifications for RSSI updates perhaps
  // with throttling to avoid spamming.
  peer_->SetRssiInternal(rssi);

  page_scan_rep_mode_ = page_scan_rep_mode;
  clock_offset_ = le16toh(clock_offset) & hci_spec::kClockOffsetMask;

  if (!device_class_ || *device_class_ != device_class) {
    device_class_ = device_class;
    notify_listeners = true;
  }

  if (eir_data.size() && SetEirData(eir_data)) {
    notify_listeners = true;
  }

  peer_->OnPeerUpdate();

  if (notify_listeners) {
    peer_->NotifyListeners(NotifyListenersChange::kBondNotUpdated);
  }
}

bool Peer::BrEdrData::SetEirData(const ByteBuffer& eir) {
  BT_DEBUG_ASSERT(eir.size());

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
      // Do not update the name of bonded peers because inquiry results are unauthenticated.
      if (!peer_->bonded()) {
        changed =
            peer_->RegisterNameInternal(data.ToString(), Peer::NameSource::kInquiryResultComplete);
      }
    }
  }
  return changed;
}

void Peer::BrEdrData::SetBondData(const sm::LTK& link_key) {
  BT_DEBUG_ASSERT(peer_->connectable());

  // Make sure the peer is non-temporary.
  peer_->TryMakeNonTemporary();

  // Storing the key establishes the bond.
  link_key_.Set(link_key);

  // PeerCache notifies listeners of new bonds, so no need to request that here.
  peer_->UpdatePeerAndNotifyListeners(NotifyListenersChange::kBondNotUpdated);
}

void Peer::BrEdrData::ClearBondData() {
  BT_ASSERT(link_key_->has_value());
  link_key_.Set(std::nullopt);
}

void Peer::BrEdrData::AddService(UUID uuid) {
  auto [_, inserted] = services_.Mutable()->insert(uuid);
  if (inserted) {
    auto update_bond =
        bonded() ? NotifyListenersChange::kBondUpdated : NotifyListenersChange::kBondNotUpdated;
    peer_->UpdatePeerAndNotifyListeners(update_bond);
  }
}

Peer::Peer(NotifyListenersCallback notify_listeners_callback, PeerCallback update_expiry_callback,
           PeerCallback dual_mode_callback, StoreLowEnergyBondCallback store_le_bond_callback,
           PeerId identifier, const DeviceAddress& address, bool connectable,
           PeerMetrics* peer_metrics)
    : notify_listeners_callback_(std::move(notify_listeners_callback)),
      update_expiry_callback_(std::move(update_expiry_callback)),
      dual_mode_callback_(std::move(dual_mode_callback)),
      store_le_bond_callback_(std::move(store_le_bond_callback)),
      identifier_(identifier, MakeToStringInspectConvertFunction()),
      technology_((address.type() == DeviceAddress::Type::kBREDR) ? TechnologyType::kClassic
                                                                  : TechnologyType::kLowEnergy,
                  [](TechnologyType t) { return TechnologyTypeToString(t); }),
      address_(address, MakeToStringInspectConvertFunction()),
      identity_known_(false),
      name_(std::nullopt,
            [](const std::optional<PeerName>& v) {
              return v ? v->name + " [source: " + NameSourceToString(v->source) + "]" : "";
            }),
      lmp_version_(std::nullopt,
                   [](const std::optional<pw::bluetooth::emboss::CoreSpecificationVersion>& v) {
                     return v ? hci_spec::HCIVersionToString(*v) : "";
                   }),
      lmp_manufacturer_(
          std::nullopt,
          [](const std::optional<uint16_t>& m) { return m ? GetManufacturerName(*m) : ""; }),
      lmp_features_(hci_spec::LMPFeatureSet(), MakeToStringInspectConvertFunction()),
      connectable_(connectable),
      temporary_(true),
      rssi_(hci_spec::kRSSIInvalid),
      peer_metrics_(peer_metrics),
      last_updated_(async::Now(async_get_default_dispatcher())),
      weak_self_(this) {
  BT_DEBUG_ASSERT(notify_listeners_callback_);
  BT_DEBUG_ASSERT(update_expiry_callback_);
  BT_DEBUG_ASSERT(dual_mode_callback_);
  BT_DEBUG_ASSERT(identifier.IsValid());

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

void Peer::AttachInspect(inspect::Node& parent, std::string node_name) {
  node_ = parent.CreateChild(node_name);
  identifier_.AttachInspect(node_, kInspectPeerIdName);
  technology_.AttachInspect(node_, kInspectTechnologyName);
  address_.AttachInspect(node_, kInspectAddressName);
  name_.AttachInspect(node_, kInspectPeerNameName);
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
  return bt_lib_cpp_string::StringPrintf("{peer id: %s, address: %s}", bt_str(*identifier_),
                                         bt_str(*address_));
}

bool Peer::RegisterName(const std::string& name, Peer::NameSource source) {
  if (RegisterNameInternal(name, source)) {
    UpdateExpiry();
    // TODO(fxbug.dev/61739): Update the bond when this happens
    UpdatePeerAndNotifyListeners(NotifyListenersChange::kBondNotUpdated);
    return true;
  }
  return false;
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
  if (rssi != hci_spec::kRSSIInvalid && rssi_ != rssi) {
    rssi_ = rssi;
    return true;
  }
  return false;
}

bool Peer::RegisterNameInternal(const std::string& name, Peer::NameSource source) {
  if (!bt_lib_cpp_string::IsStringUTF8(name)) {
    bt_log(WARN, "gap", "%s: not setting name to string that is not valid UTF-8", bt_str(*this));
    return false;
  }
  if (!name_->has_value() || source < (*name_)->source ||
      (source == (*name_)->source && name != (*name_)->name)) {
    name_.Set(Peer::PeerName{name, source});
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
    UpdatePeerAndNotifyListeners(NotifyListenersChange::kBondNotUpdated);
  }

  return true;
}

bool Peer::TryMakeTemporary() {
  if (le() && le()->connection_state() == ConnectionState::kNotConnected && !identity_known()) {
    bt_log(DEBUG, "gap", "LE became temporary: %s:", bt_str(*this));
    temporary_.Set(true);
    return true;
  }
  if (bredr() && !bredr()->bonded()) {
    bt_log(DEBUG, "gap", "BR/EDR became temporary: %s:", bt_str(*this));
    temporary_.Set(true);
    return true;
  }
  return false;
}

void Peer::UpdateExpiry() {
  BT_DEBUG_ASSERT(update_expiry_callback_);
  update_expiry_callback_(*this);
}

void Peer::NotifyListeners(NotifyListenersChange change) {
  BT_DEBUG_ASSERT(notify_listeners_callback_);
  notify_listeners_callback_(*this, change);
}

void Peer::MakeDualMode() {
  technology_.Set(TechnologyType::kDualMode);
  if (bredr_cross_transport_key_) {
    BT_ASSERT(bredr_data_);  // Should only be hit after BR/EDR is already created.
    bredr_data_->SetBondData(*bredr_cross_transport_key_);
    bt_log(DEBUG, "gap-bredr", "restored cross-transport-generated br/edr link key");
    bredr_cross_transport_key_ = std::nullopt;
  }
  BT_DEBUG_ASSERT(dual_mode_callback_);
  dual_mode_callback_(*this);
}

void Peer::OnPeerUpdate() { last_updated_ = async::Now(async_get_default_dispatcher()); }

void Peer::UpdatePeerAndNotifyListeners(NotifyListenersChange change) {
  OnPeerUpdate();
  NotifyListeners(change);
}

}  // namespace bt::gap
