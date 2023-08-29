// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "adapter.h"

#include <endian.h>

#include "bredr_connection_manager.h"
#include "bredr_discovery_manager.h"
#include "event_masks.h"
#include "low_energy_address_manager.h"
#include "low_energy_advertising_manager.h"
#include "low_energy_connection_manager.h"
#include "low_energy_discovery_manager.h"
#include "peer.h"
#include "src/connectivity/bluetooth/core/bt-host//hci-spec/vendor_protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/metrics.h"
#include "src/connectivity/bluetooth/core/bt-host/common/random.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/util.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/android_extended_low_energy_advertiser.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/extended_low_energy_advertiser.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/legacy_low_energy_advertiser.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/legacy_low_energy_scanner.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_connector.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/sequential_command_runner.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/emboss_control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

#include <pw_bluetooth/hci_commands.emb.h>

namespace bt::gap {

namespace hci_android = hci_spec::vendor::android;

static constexpr const char* kInspectLowEnergyDiscoveryManagerNodeName =
    "low_energy_discovery_manager";
static constexpr const char* kInspectLowEnergyConnectionManagerNodeName =
    "low_energy_connection_manager";
static constexpr const char* kInspectBrEdrConnectionManagerNodeName = "bredr_connection_manager";
static constexpr const char* kInspectBrEdrDiscoveryManagerNodeName = "bredr_discovery_manager";

// All asynchronous callbacks are posted on the Loop on which this Adapter
// instance is created.
class AdapterImpl final : public Adapter {
 public:
  // There must be an async_t dispatcher registered as a default when an AdapterImpl
  // instance is created. The Adapter instance will use it for all of its
  // asynchronous tasks.
  explicit AdapterImpl(hci::Transport::WeakPtr hci, gatt::GATT::WeakPtr gatt,
                       std::unique_ptr<l2cap::ChannelManager> l2cap);
  ~AdapterImpl() override;

  AdapterId identifier() const override { return identifier_; }

  bool Initialize(InitializeCallback callback, fit::closure transport_error_cb) override;

  void ShutDown() override;

  bool IsInitializing() const override { return init_state_ == State::kInitializing; }

  bool IsInitialized() const override { return init_state_ == State::kInitialized; }

  const AdapterState& state() const override { return state_; }

  class LowEnergyImpl final : public LowEnergy {
   public:
    explicit LowEnergyImpl(AdapterImpl* adapter) : adapter_(adapter) {}

    void Connect(PeerId peer_id, ConnectionResultCallback callback,
                 LowEnergyConnectionOptions connection_options) override {
      adapter_->le_connection_manager_->Connect(peer_id, std::move(callback), connection_options);
      adapter_->metrics_.le.outgoing_connection_requests.Add();
    }

    bool Disconnect(PeerId peer_id) override {
      return adapter_->le_connection_manager_->Disconnect(peer_id);
    }

    void Pair(PeerId peer_id, sm::SecurityLevel pairing_level, sm::BondableMode bondable_mode,
              sm::ResultFunction<> cb) override {
      adapter_->le_connection_manager_->Pair(peer_id, pairing_level, bondable_mode, std::move(cb));
      adapter_->metrics_.le.pair_requests.Add();
    }

    void SetLESecurityMode(LESecurityMode mode) override {
      adapter_->le_connection_manager_->SetSecurityMode(mode);
    }

    LESecurityMode security_mode() const override {
      return adapter_->le_connection_manager_->security_mode();
    }

    void StartAdvertising(AdvertisingData data, AdvertisingData scan_rsp,
                          AdvertisingInterval interval, bool anonymous, bool include_tx_power_level,
                          std::optional<ConnectableAdvertisingParameters> connectable,
                          AdvertisingStatusCallback status_callback) override {
      LowEnergyAdvertisingManager::ConnectionCallback advertisement_connect_cb = nullptr;
      if (connectable) {
        BT_ASSERT(connectable->connection_cb);

        // All advertisement connections are first registered with LowEnergyConnectionManager before
        // being reported to higher layers.
        advertisement_connect_cb = [this, connectable = std::move(connectable)](
                                       AdvertisementId advertisement_id,
                                       std::unique_ptr<hci::LowEnergyConnection> link) mutable {
          auto register_link_cb = [advertisement_id,
                                   connection_callback = std::move(connectable->connection_cb)](
                                      ConnectionResult result) {
            connection_callback(advertisement_id, std::move(result));
          };

          adapter_->le_connection_manager_->RegisterRemoteInitiatedLink(
              std::move(link), connectable->bondable_mode, std::move(register_link_cb));
        };
      }

      adapter_->le_advertising_manager_->StartAdvertising(
          std::move(data), std::move(scan_rsp), std::move(advertisement_connect_cb), interval,
          anonymous, include_tx_power_level, std::move(status_callback));
      adapter_->metrics_.le.start_advertising_events.Add();
    }

    void StopAdvertising(AdvertisementId advertisement_id) override {
      adapter_->le_advertising_manager_->StopAdvertising(advertisement_id);
      adapter_->metrics_.le.stop_advertising_events.Add();
    }

    void StartDiscovery(bool active, SessionCallback callback) override {
      adapter_->le_discovery_manager_->StartDiscovery(active, std::move(callback));
      adapter_->metrics_.le.start_discovery_events.Add();
    }

    void EnablePrivacy(bool enabled) override {
      adapter_->le_address_manager_->EnablePrivacy(enabled);
    }

    bool PrivacyEnabled() const override { return adapter_->le_address_manager_->PrivacyEnabled(); }

    const DeviceAddress& CurrentAddress() const override {
      return adapter_->le_address_manager_->current_address();
    }

    void register_address_changed_callback(fit::closure callback) override {
      auto cb = [cb = std::move(callback)](auto) { cb(); };
      adapter_->le_address_manager_->register_address_changed_callback(std::move(cb));
    }

    void set_irk(const std::optional<UInt128>& irk) override {
      adapter_->le_address_manager_->set_irk(irk);
    }

    std::optional<UInt128> irk() const override { return adapter_->le_address_manager_->irk(); }

    void set_request_timeout_for_testing(zx::duration value) override {
      adapter_->le_connection_manager_->set_request_timeout_for_testing(value);
    }

    void set_scan_period_for_testing(zx::duration period) override {
      adapter_->le_discovery_manager_->set_scan_period(period);
    }

   private:
    AdapterImpl* adapter_;
  };

  LowEnergy* le() const override { return low_energy_.get(); }

  class BrEdrImpl final : public BrEdr {
   public:
    explicit BrEdrImpl(AdapterImpl* adapter) : adapter_(adapter) {}

    bool Connect(PeerId peer_id, ConnectResultCallback callback) override {
      return adapter_->bredr_connection_manager_->Connect(peer_id, std::move(callback));
      adapter_->metrics_.bredr.outgoing_connection_requests.Add();
    }

    bool Disconnect(PeerId peer_id, DisconnectReason reason) override {
      return adapter_->bredr_connection_manager_->Disconnect(peer_id, reason);
    }

    void OpenL2capChannel(PeerId peer_id, l2cap::PSM psm,
                          BrEdrSecurityRequirements security_requirements,
                          l2cap::ChannelParameters params, l2cap::ChannelCallback cb) override {
      adapter_->metrics_.bredr.open_l2cap_channel_requests.Add();
      adapter_->bredr_connection_manager_->OpenL2capChannel(peer_id, psm, security_requirements,
                                                            params, std::move(cb));
    }

    PeerId GetPeerId(hci_spec::ConnectionHandle handle) const override {
      return adapter_->bredr_connection_manager_->GetPeerId(handle);
    }

    SearchId AddServiceSearch(const UUID& uuid, std::unordered_set<sdp::AttributeId> attributes,
                              SearchCallback callback) override {
      return adapter_->bredr_connection_manager_->AddServiceSearch(uuid, std::move(attributes),
                                                                   std::move(callback));
    }

    bool RemoveServiceSearch(SearchId id) override {
      return adapter_->bredr_connection_manager_->RemoveServiceSearch(id);
    }

    void Pair(PeerId peer_id, BrEdrSecurityRequirements security,
              hci::ResultFunction<> callback) override {
      adapter_->bredr_connection_manager_->Pair(peer_id, security, std::move(callback));
      adapter_->metrics_.bredr.pair_requests.Add();
    }

    void SetBrEdrSecurityMode(BrEdrSecurityMode mode) override {
      adapter_->bredr_connection_manager_->SetSecurityMode(mode);
    }

    BrEdrSecurityMode security_mode() const override {
      return adapter_->bredr_connection_manager_->security_mode();
    }

    void SetConnectable(bool connectable, hci::ResultFunction<> status_cb) override {
      adapter_->bredr_connection_manager_->SetConnectable(connectable, std::move(status_cb));
      if (connectable) {
        adapter_->metrics_.bredr.set_connectable_true_events.Add();
      } else {
        adapter_->metrics_.bredr.set_connectable_false_events.Add();
      }
    }

    void RequestDiscovery(DiscoveryCallback callback) override {
      adapter_->bredr_discovery_manager_->RequestDiscovery(std::move(callback));
      adapter_->metrics_.bredr.request_discovery_events.Add();
    }

    void RequestDiscoverable(DiscoverableCallback callback) override {
      adapter_->bredr_discovery_manager_->RequestDiscoverable(std::move(callback));
      adapter_->metrics_.bredr.request_discoverable_events.Add();
    }

    RegistrationHandle RegisterService(std::vector<sdp::ServiceRecord> records,
                                       l2cap::ChannelParameters chan_params,
                                       ServiceConnectCallback conn_cb) override {
      return adapter_->sdp_server_->RegisterService(std::move(records), chan_params,
                                                    std::move(conn_cb));
    }

    bool UnregisterService(RegistrationHandle handle) override {
      return adapter_->sdp_server_->UnregisterService(handle);
    }

    std::optional<ScoRequestHandle> OpenScoConnection(
        PeerId peer_id,
        const bt::StaticPacket<pw::bluetooth::emboss::SynchronousConnectionParametersWriter>&
            parameters,
        sco::ScoConnectionManager::OpenConnectionCallback callback) override {
      return adapter_->bredr_connection_manager_->OpenScoConnection(peer_id, parameters,
                                                                    std::move(callback));
    }
    std::optional<ScoRequestHandle> AcceptScoConnection(
        PeerId peer_id,
        const std::vector<
            bt::StaticPacket<pw::bluetooth::emboss::SynchronousConnectionParametersWriter>>
            parameters,
        sco::ScoConnectionManager::AcceptConnectionCallback callback) override {
      return adapter_->bredr_connection_manager_->AcceptScoConnection(
          peer_id, std::move(parameters), std::move(callback));
    }

   private:
    AdapterImpl* adapter_;
  };

  BrEdr* bredr() const override { return bredr_.get(); }

  PeerCache* peer_cache() override { return &peer_cache_; }

  bool AddBondedPeer(BondingData bonding_data) override;

  void SetPairingDelegate(PairingDelegate::WeakPtr delegate) override;

  bool IsDiscoverable() const override;

  bool IsDiscovering() const override;

  void SetLocalName(std::string name, hci::ResultFunction<> callback) override;

  std::string local_name() const override { return bredr_discovery_manager_->local_name(); }

  void SetDeviceClass(DeviceClass dev_class, hci::ResultFunction<> callback) override;

  void set_auto_connect_callback(AutoConnectCallback callback) override {
    auto_conn_cb_ = std::move(callback);
  }

  void AttachInspect(inspect::Node& parent, std::string name) override;

  WeakSelf<Adapter>::WeakPtr AsWeakPtr() override { return weak_self_adapter_.GetWeakPtr(); }

 private:
  // Called by Initialize() after Transport is initialized.
  void InitializeStep1();

  // Second step of the initialization sequence. Called by InitializeStep1() when the
  // first batch of HCI commands have been sent.
  void InitializeStep2();

  // Third step of the initialization sequence. Called by InitializeStep2() when
  // the second batch of HCI commands have been sent.
  void InitializeStep3();

  // Fourth step of the initialization sequence. Called by InitializeStep3()
  // when the third batch of HCI commands have been sent.
  void InitializeStep4();

  // Returns true if initialization was completed, or false if initialization is not
  // in progress.
  bool CompleteInitialization(bool success);

  // Reads LMP feature mask's bits from |page|
  void InitQueueReadLMPFeatureMaskPage(uint8_t page);

  // Assigns properties to |adapter_node_| using values discovered during other initialization
  // steps.
  void UpdateInspectProperties();

  // Called by ShutDown() and during Initialize() in case of failure. This
  // synchronously cleans up the transports and resets initialization state.
  void CleanUp();

  // Called by Transport after it experiences a fatal error.
  void OnTransportError();

  // Called when a directed connectable advertisement is received from a bonded
  // LE device. This amounts to a connection request from a bonded peripheral
  // which is handled by routing the request to |le_connection_manager_| to
  // initiate a Direct Connection Establishment procedure (Vol 3, Part C,
  // 9.3.8).
  void OnLeAutoConnectRequest(Peer* peer);

  // Called by |le_address_manager_| to query whether it is currently allowed to
  // reconfigure the LE random address.
  bool IsLeRandomAddressChangeAllowed();

  std::unique_ptr<hci::LowEnergyAdvertiser> CreateAdvertiser() {
    constexpr hci_spec::LESupportedFeature feature =
        hci_spec::LESupportedFeature::kLEExtendedAdvertising;
    if (state().low_energy_state.IsFeatureSupported(feature)) {
      bt_log(INFO, "gap", "controller supports extended advertising, using extended LE commands");
      return std::make_unique<hci::ExtendedLowEnergyAdvertiser>(hci_);
    }

    if (state().IsControllerFeatureSupported(
            pw::bluetooth::Controller::FeaturesBits::kAndroidVendorExtensions)) {
      uint8_t max_advt = state().android_vendor_capabilities.max_simultaneous_advertisements();
      bt_log(INFO, "gap",
             "controller supports android vendor extensions, max simultaneous advertisements: %d",
             max_advt);
      return std::make_unique<hci::AndroidExtendedLowEnergyAdvertiser>(hci_, max_advt);
    }

    bt_log(INFO, "gap", "controller supports only legacy advertising, using legacy LE commands");
    return std::make_unique<hci::LegacyLowEnergyAdvertiser>(hci_);
  }

  // Must be initialized first so that child nodes can be passed to other constructors.
  inspect::Node adapter_node_;
  struct InspectProperties {
    inspect::StringProperty adapter_id;
    inspect::StringProperty hci_version;
    inspect::UintProperty bredr_max_num_packets;
    inspect::UintProperty bredr_max_data_length;
    inspect::UintProperty le_max_num_packets;
    inspect::UintProperty le_max_data_length;
    inspect::UintProperty sco_max_num_packets;
    inspect::UintProperty sco_max_data_length;
    inspect::StringProperty lmp_features;
    inspect::StringProperty le_features;
  };
  InspectProperties inspect_properties_;

  // Metrics properties
  inspect::Node metrics_node_;
  inspect::Node metrics_bredr_node_;
  inspect::Node metrics_le_node_;
  struct AdapterMetrics {
    struct LeMetrics {
      UintMetricCounter outgoing_connection_requests;
      UintMetricCounter pair_requests;
      UintMetricCounter start_advertising_events;
      UintMetricCounter stop_advertising_events;
      UintMetricCounter start_discovery_events;
    } le;
    struct BrEdrMetrics {
      UintMetricCounter outgoing_connection_requests;
      UintMetricCounter pair_requests;
      UintMetricCounter set_connectable_true_events;
      UintMetricCounter set_connectable_false_events;
      UintMetricCounter request_discovery_events;
      UintMetricCounter request_discoverable_events;
      UintMetricCounter open_l2cap_channel_requests;
    } bredr;
  };
  AdapterMetrics metrics_;

  // Uniquely identifies this adapter on the current system.
  AdapterId identifier_;

  async_dispatcher_t* dispatcher_;
  hci::Transport::WeakPtr hci_;

  // Callback invoked to notify clients when the underlying transport is closed.
  fit::closure transport_error_cb_;

  // Parameters relevant to the initialization sequence.
  // TODO(armansito): The Initialize()/ShutDown() pattern has become common
  // enough in this project that it might be worth considering moving the
  // init-state-keeping into an abstract base.
  enum State {
    kNotInitialized = 0,
    kInitializing,
    kInitialized,
  };
  std::atomic<State> init_state_;
  std::unique_ptr<hci::SequentialCommandRunner> init_seq_runner_;

  // The callback passed to Initialize(). Null after initialization completes.
  InitializeCallback init_cb_;

  // Contains the global adapter state.
  AdapterState state_;

  // The maximum LMP feature page that we will read.
  std::optional<size_t> max_lmp_feature_page_index_;

  // Provides access to discovered, connected, and/or bonded remote Bluetooth
  // devices.
  PeerCache peer_cache_;

  // L2CAP layer used by GAP. This must be destroyed after the following members because they raw
  // pointers to this member.
  std::unique_ptr<l2cap::ChannelManager> l2cap_;

  // The GATT profile. We use this reference to add and remove data bearers and
  // for service discovery.
  gatt::GATT::WeakPtr gatt_;

  // Objects that abstract the controller for connection and advertising
  // procedures.
  std::unique_ptr<hci::LowEnergyAdvertiser> hci_le_advertiser_;
  std::unique_ptr<hci::LowEnergyConnector> hci_le_connector_;
  std::unique_ptr<hci::LowEnergyScanner> hci_le_scanner_;

  // Objects that perform LE procedures.
  std::unique_ptr<LowEnergyAddressManager> le_address_manager_;
  std::unique_ptr<LowEnergyDiscoveryManager> le_discovery_manager_;
  std::unique_ptr<LowEnergyConnectionManager> le_connection_manager_;
  std::unique_ptr<LowEnergyAdvertisingManager> le_advertising_manager_;
  std::unique_ptr<LowEnergyImpl> low_energy_;

  // Objects that perform BR/EDR procedures.
  std::unique_ptr<BrEdrConnectionManager> bredr_connection_manager_;
  std::unique_ptr<BrEdrDiscoveryManager> bredr_discovery_manager_;
  std::unique_ptr<sdp::Server> sdp_server_;
  std::unique_ptr<BrEdrImpl> bredr_;

  // Callback to propagate ownership of an auto-connected LE link.
  AutoConnectCallback auto_conn_cb_;

  // This must remain the last member to make sure that all weak pointers are
  // invalidating before other members are destroyed.
  WeakSelf<AdapterImpl> weak_self_;
  WeakSelf<Adapter> weak_self_adapter_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AdapterImpl);
};

AdapterImpl::AdapterImpl(hci::Transport::WeakPtr hci, gatt::GATT::WeakPtr gatt,
                         std::unique_ptr<l2cap::ChannelManager> l2cap)
    : identifier_(Random<AdapterId>()),
      dispatcher_(async_get_default_dispatcher()),
      hci_(std::move(hci)),
      init_state_(State::kNotInitialized),
      l2cap_(std::move(l2cap)),
      gatt_(std::move(gatt)),
      weak_self_(this),
      weak_self_adapter_(this) {
  BT_DEBUG_ASSERT(hci_.is_alive());
  BT_DEBUG_ASSERT(gatt_.is_alive());
  BT_DEBUG_ASSERT_MSG(dispatcher_, "must create on a thread with a dispatcher");

  auto self = weak_self_.GetWeakPtr();
  hci_->SetTransportErrorCallback([self] {
    if (self.is_alive()) {
      self->OnTransportError();
    }
  });

  gatt_->SetPersistServiceChangedCCCCallback(
      [this](PeerId peer_id, gatt::ServiceChangedCCCPersistedData gatt_data) {
        Peer* peer = peer_cache_.FindById(peer_id);
        if (!peer) {
          bt_log(WARN, "gap", "Unable to find peer %s when storing persisted GATT data.",
                 bt_str(peer_id));
        } else if (!peer->le()) {
          bt_log(WARN, "gap", "Tried to store persisted GATT data for non-LE peer %s.",
                 bt_str(peer_id));
        } else {
          peer->MutLe().set_service_changed_gatt_data(gatt_data);
        }
      });

  gatt_->SetRetrieveServiceChangedCCCCallback([this](PeerId peer_id) {
    Peer* peer = peer_cache_.FindById(peer_id);
    if (!peer) {
      bt_log(WARN, "gap", "Unable to find peer %s when retrieving persisted GATT data.",
             peer_id.ToString().c_str());
      return std::optional<gatt::ServiceChangedCCCPersistedData>();
    }

    if (!peer->le()) {
      bt_log(WARN, "gap", "Tried to retrieve persisted GATT data for non-LE peer %s.",
             peer_id.ToString().c_str());
      return std::optional<gatt::ServiceChangedCCCPersistedData>();
    }

    return std::optional(peer->le()->get_service_changed_gatt_data());
  });
}

AdapterImpl::~AdapterImpl() {
  if (IsInitialized()) {
    ShutDown();
  }
}

bool AdapterImpl::Initialize(InitializeCallback callback, fit::closure transport_error_cb) {
  BT_DEBUG_ASSERT(callback);
  BT_DEBUG_ASSERT(transport_error_cb);

  if (IsInitialized()) {
    bt_log(WARN, "gap", "Adapter already initialized");
    return false;
  }

  BT_DEBUG_ASSERT(!IsInitializing());
  BT_DEBUG_ASSERT(!init_seq_runner_);

  init_state_ = State::kInitializing;
  init_cb_ = std::move(callback);
  transport_error_cb_ = std::move(transport_error_cb);

  hci_->Initialize([this](bool success) {
    if (!success) {
      bt_log(ERROR, "gap", "Failed to initialize Transport");
      CompleteInitialization(/*success=*/false);
      return;
    }
    init_seq_runner_ =
        std::make_unique<hci::SequentialCommandRunner>(hci_->command_channel()->AsWeakPtr());

    InitializeStep1();
  });

  return true;
}

void AdapterImpl::ShutDown() {
  bt_log(DEBUG, "gap", "adapter shutting down");

  if (IsInitializing()) {
    BT_DEBUG_ASSERT(!init_seq_runner_->IsReady());
    init_seq_runner_->Cancel();
  }

  CleanUp();
}

bool AdapterImpl::AddBondedPeer(BondingData bonding_data) {
  return peer_cache()->AddBondedPeer(bonding_data);
}

void AdapterImpl::SetPairingDelegate(PairingDelegate::WeakPtr delegate) {
  le_connection_manager_->SetPairingDelegate(delegate);
  bredr_connection_manager_->SetPairingDelegate(delegate);
}

bool AdapterImpl::IsDiscoverable() const {
  if (bredr_discovery_manager_ && bredr_discovery_manager_->discoverable()) {
    return true;
  }

  // If LE Privacy is enabled, then we are not discoverable.
  // TODO(fxbug.dev/109132): Make this dependent on whether the LE Public advertisement is active or
  // not.
  if (le_address_manager_ && le_address_manager_->PrivacyEnabled()) {
    return false;
  }

  return (le_advertising_manager_ && le_advertising_manager_->advertising());
}

bool AdapterImpl::IsDiscovering() const {
  return (le_discovery_manager_ && le_discovery_manager_->discovering()) ||
         (bredr_discovery_manager_ && bredr_discovery_manager_->discovering());
}

void AdapterImpl::SetLocalName(std::string name, hci::ResultFunction<> callback) {
  // TODO(fxbug.dev/40836): set the public LE advertisement name from |name|
  // If BrEdr is not supported, skip the name update.
  if (!bredr_discovery_manager_) {
    callback(ToResult(bt::HostError::kNotSupported));
    return;
  }

  // Make a copy of |name| to move separately into the lambda.
  std::string name_copy(name);
  bredr_discovery_manager_->UpdateLocalName(
      std::move(name),
      [this, cb = std::move(callback), local_name = std::move(name_copy)](auto status) {
        if (!bt_is_error(status, WARN, "gap", "set local name failed")) {
          state_.local_name = local_name;
        }
        cb(status);
      });
}

void AdapterImpl::SetDeviceClass(DeviceClass dev_class, hci::ResultFunction<> callback) {
  auto write_dev_class =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::WriteClassOfDeviceCommandWriter>(
          hci_spec::kWriteClassOfDevice);
  write_dev_class.view_t().class_of_device().BackingStorage().WriteUInt(dev_class.to_int());
  hci_->command_channel()->SendCommand(
      std::move(write_dev_class), [cb = std::move(callback)](auto, const hci::EventPacket& event) {
        hci_is_error(event, WARN, "gap", "set device class failed");
        cb(event.ToResult());
      });
}

void AdapterImpl::AttachInspect(inspect::Node& parent, std::string name) {
  adapter_node_ = parent.CreateChild(name);
  UpdateInspectProperties();

  peer_cache_.AttachInspect(adapter_node_);

  metrics_node_ = adapter_node_.CreateChild(kMetricsInspectNodeName);

  metrics_le_node_ = metrics_node_.CreateChild("le");
  metrics_.le.outgoing_connection_requests.AttachInspect(metrics_le_node_,
                                                         "outgoing_connection_requests");
  metrics_.le.pair_requests.AttachInspect(metrics_le_node_, "pair_requests");
  metrics_.le.start_advertising_events.AttachInspect(metrics_le_node_, "start_advertising_events");
  metrics_.le.stop_advertising_events.AttachInspect(metrics_le_node_, "stop_advertising_events");
  metrics_.le.start_discovery_events.AttachInspect(metrics_le_node_, "start_discovery_events");

  metrics_bredr_node_ = metrics_node_.CreateChild("bredr");
  metrics_.bredr.outgoing_connection_requests.AttachInspect(metrics_bredr_node_,
                                                            "outgoing_connection_requests");
  metrics_.bredr.pair_requests.AttachInspect(metrics_bredr_node_, "pair_requests");
  metrics_.bredr.set_connectable_true_events.AttachInspect(metrics_bredr_node_,
                                                           "set_connectable_true_events");
  metrics_.bredr.set_connectable_false_events.AttachInspect(metrics_bredr_node_,
                                                            "set_connectable_false_events");
  metrics_.bredr.request_discovery_events.AttachInspect(metrics_bredr_node_,
                                                        "request_discovery_events");
  metrics_.bredr.request_discoverable_events.AttachInspect(metrics_bredr_node_,
                                                           "request_discoverable_events");
  metrics_.bredr.open_l2cap_channel_requests.AttachInspect(metrics_bredr_node_,
                                                           "open_l2cap_channel_requests");
}

void AdapterImpl::InitializeStep1() {
  state_.controller_features = hci_->GetFeatures();

  // Start by resetting the controller to a clean state and then send
  // informational parameter commands that are not specific to LE or BR/EDR. The
  // commands sent here are mandatory for all LE controllers.
  //
  // NOTE: It's safe to pass capture |this| directly in the callbacks as
  // |init_seq_runner_| will internally invalidate the callbacks if it ever gets
  // deleted.

  // HCI_Reset
  auto reset_command =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(hci_spec::kReset);
  init_seq_runner_->QueueCommand(std::move(reset_command));

  // HCI_Read_Local_Version_Information
  init_seq_runner_->QueueCommand(
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ReadLocalVersionInformationCommandView>(
          hci_spec::kReadLocalVersionInfo),
      [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "read local version info failed")) {
          return;
        }
        auto params = cmd_complete.return_params<hci_spec::ReadLocalVersionInfoReturnParams>();
        state_.hci_version = params->hci_version;
      });

  // HCI_Read_Local_Supported_Commands
  init_seq_runner_->QueueCommand(
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ReadLocalSupportedCommandsCommandView>(
          hci_spec::kReadLocalSupportedCommands),
      [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "read local supported commands failed")) {
          return;
        }
        auto params =
            cmd_complete.return_params<hci_spec::ReadLocalSupportedCommandsReturnParams>();
        std::memcpy(state_.supported_commands, params->supported_commands,
                    sizeof(params->supported_commands));
      });

  // HCI_Read_Local_Supported_Features
  InitQueueReadLMPFeatureMaskPage(0);

  // HCI_Read_BD_ADDR
  init_seq_runner_->QueueCommand(
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ReadBdAddrCommandView>(
          hci_spec::kReadBDADDR),
      [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "read BR_ADDR failed")) {
          return;
        }
        auto params = cmd_complete.return_params<hci_spec::ReadBDADDRReturnParams>();
        state_.controller_address = params->bd_addr;
      });

  if (state().IsControllerFeatureSupported(
          pw::bluetooth::Controller::FeaturesBits::kAndroidVendorExtensions)) {
    bt_log(INFO, "gap", "controller supports android hci extensions, querying exact feature set");
    init_seq_runner_->QueueCommand(
        hci::EmbossCommandPacket::New<
            pw::bluetooth::vendor::android_hci::LEGetVendorCapabilitiesCommandView>(
            hci_android::kLEGetVendorCapabilities),
        [this](const hci::EmbossEventPacket& event) {
          if (hci_is_error(event, WARN, "gap",
                           "Failed to query android hci extension capabilities")) {
            return;
          }

          auto params = event.view<pw::bluetooth::vendor::android_hci::
                                       LEGetVendorCapabilitiesCommandCompleteEventView>();
          state_.android_vendor_capabilities.Initialize(params);
        });
  }

  init_seq_runner_->RunCommands([this](hci::Result<> status) mutable {
    if (bt_is_error(status, ERROR, "gap", "Failed to obtain initial controller information: %s",
                    bt_str(status))) {
      CompleteInitialization(/*success=*/false);
      return;
    }

    InitializeStep2();
  });
}

void AdapterImpl::InitializeStep2() {
  BT_DEBUG_ASSERT(IsInitializing());

  // Low Energy MUST be supported. We don't support BR/EDR-only controllers.
  if (!state_.IsLowEnergySupported()) {
    bt_log(ERROR, "gap", "Bluetooth LE not supported by controller");
    CompleteInitialization(/*success=*/false);
    return;
  }

  // Check the HCI version. We officially only support 4.2+ only but for now we
  // just log a warning message if the version is legacy.
  if (state_.hci_version < pw::bluetooth::emboss::CoreSpecificationVersion::V4_2) {
    bt_log(WARN, "gap", "controller is using legacy HCI version %s",
           hci_spec::HCIVersionToString(state_.hci_version).c_str());
  }

  BT_DEBUG_ASSERT(init_seq_runner_->IsReady());

  // If the controller supports the Read Buffer Size command then send it.
  // Otherwise we'll default to 0 when initializing the ACLDataChannel.
  if (state_.IsCommandSupported(/*octet=*/14, hci_spec::SupportedCommand::kReadBufferSize)) {
    // HCI_Read_Buffer_Size
    init_seq_runner_->QueueCommand(
        hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ReadBufferSizeCommandView>(
            hci_spec::kReadBufferSize),
        [this](const hci::EventPacket& cmd_complete) {
          if (hci_is_error(cmd_complete, WARN, "gap", "read buffer size failed")) {
            return;
          }
          auto params = cmd_complete.return_params<hci_spec::ReadBufferSizeReturnParams>();
          uint16_t acl_mtu = le16toh(params->hc_acl_data_packet_length);
          uint16_t acl_max_count = le16toh(params->hc_total_num_acl_data_packets);
          if (acl_mtu && acl_max_count) {
            state_.bredr_data_buffer_info = hci::DataBufferInfo(acl_mtu, acl_max_count);
          }
          uint16_t sco_mtu = le16toh(params->hc_synchronous_data_packet_length);
          uint16_t sco_max_count = le16toh(params->hc_total_num_synchronous_data_packets);
          if (sco_mtu && sco_max_count) {
            state_.sco_buffer_info = hci::DataBufferInfo(sco_mtu, sco_max_count);
          }
        });
  }

  // HCI_LE_Read_Local_Supported_Features
  init_seq_runner_->QueueCommand(
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::LEReadLocalSupportedFeaturesCommandView>(
          hci_spec::kLEReadLocalSupportedFeatures),
      [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "LE read local supported features failed")) {
          return;
        }
        auto params =
            cmd_complete.return_params<hci_spec::LEReadLocalSupportedFeaturesReturnParams>();
        state_.low_energy_state.supported_features_ = le64toh(params->le_features);
      });

  // HCI_LE_Read_Supported_States
  init_seq_runner_->QueueCommand(
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::LEReadSupportedStatesCommandView>(
          hci_spec::kLEReadSupportedStates),
      [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "LE read local supported states failed")) {
          return;
        }
        auto params = cmd_complete.return_params<hci_spec::LEReadSupportedStatesReturnParams>();
        state_.low_energy_state.supported_states_ = le64toh(params->le_states);
      });

  // HCI_LE_Read_Buffer_Size
  init_seq_runner_->QueueCommand(
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::LEReadBufferSizeCommandV1View>(
          hci_spec::kLEReadBufferSizeV1),
      [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "LE read buffer size failed")) {
          return;
        }
        auto params = cmd_complete.return_params<hci_spec::LEReadBufferSizeReturnParams>();
        uint16_t mtu = le16toh(params->hc_le_acl_data_packet_length);
        uint8_t max_count = params->hc_total_num_le_acl_data_packets;
        if (mtu && max_count) {
          state_.low_energy_state.data_buffer_info_ = hci::DataBufferInfo(mtu, max_count);
        }
      });

  if (state_.features.HasBit(/*page=*/0u,
                             hci_spec::LMPFeature::kSecureSimplePairingControllerSupport)) {
    // HCI_Write_Simple_Pairing_Mode
    auto write_spm =
        hci::EmbossCommandPacket::New<pw::bluetooth::emboss::WriteSimplePairingModeCommandWriter>(
            hci_spec::kWriteSimplePairingMode);
    auto write_ssp_params = write_spm.view_t();
    write_ssp_params.simple_pairing_mode().Write(pw::bluetooth::emboss::GenericEnableParam::ENABLE);
    init_seq_runner_->QueueCommand(std::move(write_spm), [](const hci::EventPacket& event) {
      // Warn if the command failed
      hci_is_error(event, WARN, "gap", "write simple pairing mode failed");
    });
  }

  // If there are extended features then try to read the first page of the extended features.
  if (state_.features.HasBit(/*page=*/0u, hci_spec::LMPFeature::kExtendedFeatures)) {
    // HCI_Write_LE_Host_Support
    if (!state_.IsCommandSupported(/*octet=*/24, hci_spec::SupportedCommand::kWriteLEHostSupport)) {
      bt_log(INFO, "gap", "LE Host is not supported");
    } else {
      bt_log(INFO, "gap", "LE Host is supported. Enabling LE Host mode");
      auto cmd_packet =
          hci::EmbossCommandPacket::New<pw::bluetooth::emboss::WriteLEHostSupportCommandWriter>(
              hci_spec::kWriteLEHostSupport);
      auto params = cmd_packet.view_t();
      params.le_supported_host().Write(pw::bluetooth::emboss::GenericEnableParam::ENABLE);
      init_seq_runner_->QueueCommand(std::move(cmd_packet), [](const hci::EventPacket& event) {
        hci_is_error(event, WARN, "gap", "Write LE Host support failed");
      });
    }

    // HCI_Write_Secure_Connections_Host_Support
    if (!state_.IsCommandSupported(
            /*octet=*/32, hci_spec::SupportedCommand::kWriteSecureConnectionsHostSupport)) {
      bt_log(INFO, "gap", "Secure Connections (Host Support) is not supported");
    } else {
      bt_log(INFO, "gap",
             "Secure Connections (Host Support) is supported. "
             "Enabling Secure Connections (Host Support) mode");
      auto cmd_packet = hci::EmbossCommandPacket::New<
          pw::bluetooth::emboss::WriteSecureConnectionsHostSupportCommandWriter>(
          hci_spec::kWriteSecureConnectionsHostSupport);
      auto params = cmd_packet.view_t();
      params.secure_connections_host_support().Write(
          pw::bluetooth::emboss::GenericEnableParam::ENABLE);
      init_seq_runner_->QueueCommand(std::move(cmd_packet), [](const hci::EventPacket& event) {
        hci_is_error(event, WARN, "gap", "Write Secure Connections (Host Support) failed");
      });
    }

    // Read updated page 1 after host support bits enabled.
    InitQueueReadLMPFeatureMaskPage(1);
  }

  init_seq_runner_->RunCommands([this](hci::Result<> status) mutable {
    if (bt_is_error(status, ERROR, "gap",
                    "failed to obtain initial controller information (step 2)")) {
      CompleteInitialization(/*success=*/false);
      return;
    }
    InitializeStep3();
  });
}

void AdapterImpl::InitializeStep3() {
  BT_ASSERT(IsInitializing());
  BT_ASSERT(init_seq_runner_->IsReady());
  BT_ASSERT(!init_seq_runner_->HasQueuedCommands());

  if (!state_.bredr_data_buffer_info.IsAvailable() &&
      !state_.low_energy_state.data_buffer_info().IsAvailable()) {
    bt_log(ERROR, "gap", "Both BR/EDR and LE buffers are unavailable");
    CompleteInitialization(/*success=*/false);
    return;
  }

  // Now that we have all the ACL data buffer information it's time to
  // initialize the ACLDataChannel.
  if (!hci_->InitializeACLDataChannel(state_.bredr_data_buffer_info,
                                      state_.low_energy_state.data_buffer_info())) {
    bt_log(ERROR, "gap", "Failed to initialize ACLDataChannel (step 3)");
    CompleteInitialization(/*success=*/false);
    return;
  }

  // The controller may not support SCO flow control (as implied by not supporting
  // HCI_Write_Synchronous_Flow_Control_Enable), in which case we don't support HCI SCO on this
  // controller yet.
  // TODO(fxbug.dev/89689): Support controllers that don't support SCO flow control.
  bool sco_flow_control_supported = state_.IsCommandSupported(
      /*octet=*/10, hci_spec::SupportedCommand::kWriteSynchronousFlowControlEnable);
  if (state_.sco_buffer_info.IsAvailable() && sco_flow_control_supported) {
    // Enable SCO flow control.
    auto sync_flow_control = hci::EmbossCommandPacket::New<
        pw::bluetooth::emboss::WriteSynchronousFlowControlEnableCommandWriter>(
        hci_spec::kWriteSynchronousFlowControlEnable);
    auto flow_control_params = sync_flow_control.view_t();
    flow_control_params.synchronous_flow_control_enable().Write(
        pw::bluetooth::emboss::GenericEnableParam::ENABLE);
    init_seq_runner_->QueueCommand(
        std::move(sync_flow_control), [this](const hci::EventPacket& event) {
          if (hci_is_error(event, ERROR, "gap",
                           "Write synchronous flow control enable failed, proceeding without HCI "
                           "SCO support")) {
            return;
          }

          if (!hci_->InitializeScoDataChannel(state_.sco_buffer_info)) {
            bt_log(WARN, "gap",
                   "Failed to initialize ScoDataChannel, proceeding without HCI SCO support");
            return;
          }
          bt_log(DEBUG, "gap", "ScoDataChannel initialized successfully");
        });
  } else {
    bt_log(INFO, "gap",
           "HCI SCO not supported (SCO buffer available: %d, SCO flow control supported: %d)",
           state_.sco_buffer_info.IsAvailable(), sco_flow_control_supported);
  }

  hci_->AttachInspect(adapter_node_);

  // Create ChannelManager, if we haven't been provided one for testing. Doing so here lets us
  // guarantee that AclDataChannel's lifetime is a superset of ChannelManager's lifetime.
  if (!l2cap_) {
    // Initialize ChannelManager to make it available for the next initialization step. The
    // AclDataChannel must be initialized before creating ChannelManager.
    l2cap_ = l2cap::ChannelManager::Create(hci_->acl_data_channel(), hci_->command_channel(),
                                           /*random_channel_ids=*/true);
    l2cap_->AttachInspect(adapter_node_, l2cap::ChannelManager::kInspectNodeName);
  }

  // HCI_Set_Event_Mask
  {
    uint64_t event_mask = BuildEventMask();
    auto set_event =
        hci::EmbossCommandPacket::New<pw::bluetooth::emboss::SetEventMaskCommandWriter>(
            hci_spec::kSetEventMask);
    auto set_event_params = set_event.view_t();
    set_event_params.event_mask().Write(event_mask);
    init_seq_runner_->QueueCommand(std::move(set_event), [](const hci::EventPacket& event) {
      hci_is_error(event, WARN, "gap", "set event mask failed");
    });
  }

  // HCI_LE_Set_Event_Mask
  {
    uint64_t event_mask = BuildLEEventMask();
    auto cmd_packet =
        hci::EmbossCommandPacket::New<pw::bluetooth::emboss::LESetEventMaskCommandWriter>(
            hci_spec::kLESetEventMask);
    cmd_packet.view_t().le_event_mask().BackingStorage().WriteUInt(event_mask);
    init_seq_runner_->QueueCommand(std::move(cmd_packet), [](const hci::EventPacket& event) {
      hci_is_error(event, WARN, "gap", "LE set event mask failed");
    });
  }

  // If page 2 of the extended features bitfield is available, read it
  if (max_lmp_feature_page_index_.has_value() && max_lmp_feature_page_index_.value() > 1) {
    InitQueueReadLMPFeatureMaskPage(2);
  }

  init_seq_runner_->RunCommands([this](hci::Result<> status) mutable {
    if (bt_is_error(status, ERROR, "gap",
                    "failed to obtain initial controller information (step 3)")) {
      CompleteInitialization(/*success=*/false);
      return;
    }
    InitializeStep4();
  });
}

void AdapterImpl::InitializeStep4() {
  // Initialize the scan manager and low energy adapters based on current feature support
  BT_DEBUG_ASSERT(IsInitializing());

  // We use the public controller address as the local LE identity address.
  DeviceAddress adapter_identity(DeviceAddress::Type::kLEPublic, state_.controller_address);

  // Initialize the LE local address manager.
  le_address_manager_ = std::make_unique<LowEnergyAddressManager>(
      adapter_identity, fit::bind_member<&AdapterImpl::IsLeRandomAddressChangeAllowed>(this),
      hci_->command_channel()->AsWeakPtr());

  // Initialize the HCI adapters.
  hci_le_advertiser_ = CreateAdvertiser();
  hci_le_connector_ = std::make_unique<hci::LowEnergyConnector>(
      hci_, le_address_manager_.get(), dispatcher_,
      fit::bind_member<&hci::LowEnergyAdvertiser::OnIncomingConnection>(hci_le_advertiser_.get()));
  hci_le_scanner_ =
      std::make_unique<hci::LegacyLowEnergyScanner>(le_address_manager_.get(), hci_, dispatcher_);

  // Initialize the LE manager objects
  le_discovery_manager_ =
      std::make_unique<LowEnergyDiscoveryManager>(hci_le_scanner_.get(), &peer_cache_);
  le_discovery_manager_->AttachInspect(adapter_node_, kInspectLowEnergyDiscoveryManagerNodeName);
  le_discovery_manager_->set_peer_connectable_callback(
      fit::bind_member<&AdapterImpl::OnLeAutoConnectRequest>(this));

  le_connection_manager_ = std::make_unique<LowEnergyConnectionManager>(
      hci_->command_channel()->AsWeakPtr(), le_address_manager_.get(), hci_le_connector_.get(),
      &peer_cache_, l2cap_.get(), gatt_, le_discovery_manager_->GetWeakPtr(),
      sm::SecurityManager::Create);
  le_connection_manager_->AttachInspect(adapter_node_, kInspectLowEnergyConnectionManagerNodeName);

  le_advertising_manager_ = std::make_unique<LowEnergyAdvertisingManager>(
      hci_le_advertiser_.get(), le_address_manager_.get());
  low_energy_ = std::make_unique<LowEnergyImpl>(this);

  // Initialize the BR/EDR manager objects if the controller supports BR/EDR.
  if (state_.IsBREDRSupported()) {
    DeviceAddress local_bredr_address(DeviceAddress::Type::kBREDR, state_.controller_address);

    bredr_connection_manager_ = std::make_unique<BrEdrConnectionManager>(
        hci_, &peer_cache_, local_bredr_address, l2cap_.get(),
        state_.features.HasBit(/*page=*/0, hci_spec::LMPFeature::kInterlacedPageScan),
        state_.IsLocalSecureConnectionsSupported());
    bredr_connection_manager_->AttachInspect(adapter_node_, kInspectBrEdrConnectionManagerNodeName);

    pw::bluetooth::emboss::InquiryMode mode = pw::bluetooth::emboss::InquiryMode::STANDARD;
    if (state_.features.HasBit(/*page=*/0, hci_spec::LMPFeature::kExtendedInquiryResponse)) {
      mode = pw::bluetooth::emboss::InquiryMode::EXTENDED;
    } else if (state_.features.HasBit(/*page=*/0, hci_spec::LMPFeature::kRSSIwithInquiryResults)) {
      mode = pw::bluetooth::emboss::InquiryMode::RSSI;
    }

    bredr_discovery_manager_ = std::make_unique<BrEdrDiscoveryManager>(
        hci_->command_channel()->AsWeakPtr(), mode, &peer_cache_);
    bredr_discovery_manager_->AttachInspect(adapter_node_, kInspectBrEdrDiscoveryManagerNodeName);

    sdp_server_ = std::make_unique<sdp::Server>(l2cap_.get());
    sdp_server_->AttachInspect(adapter_node_);

    bredr_ = std::make_unique<BrEdrImpl>(this);
  }

  // Override the current privacy setting and always use the local stable identity address (i.e.
  // not a RPA) when initiating connections. This improves interoperability with certain Bluetooth
  // peripherals that fail to authenticate following a RPA rotation.
  //
  // The implication here is that the public address is revealed in LL connection request PDUs. LE
  // central privacy is still preserved during an active scan, i.e. in LL scan request PDUs.
  //
  // TODO(fxbug.dev/63123): Remove this temporary fix once we determine the root cause for
  // authentication failures.
  hci_le_connector_->UseLocalIdentityAddress();

  // Update properties before callback called so properties can be verified in unit tests.
  UpdateInspectProperties();

  // Assign a default name and device class before notifying completion.
  auto self = weak_self_.GetWeakPtr();
  SetLocalName(kDefaultLocalName, [self](auto status) mutable {
    // Set the default device class - a computer with audio.
    // TODO(fxbug.dev/1234): set this from a platform configuration file
    DeviceClass dev_class(DeviceClass::MajorClass::kComputer);
    dev_class.SetServiceClasses({DeviceClass::ServiceClass::kAudio});
    self->SetDeviceClass(dev_class,
                         [self](const auto&) { self->CompleteInitialization(/*success=*/true); });
  });
}

bool AdapterImpl::CompleteInitialization(bool success) {
  if (!init_cb_) {
    return false;
  }

  if (success) {
    init_state_ = State::kInitialized;
  } else {
    CleanUp();
  }

  init_cb_(success);
  return true;
}

void AdapterImpl::InitQueueReadLMPFeatureMaskPage(uint8_t page) {
  BT_DEBUG_ASSERT(init_seq_runner_);
  BT_DEBUG_ASSERT(init_seq_runner_->IsReady());

  if (max_lmp_feature_page_index_.has_value() && page > max_lmp_feature_page_index_.value()) {
    bt_log(WARN, "gap", "Maximum value of LMP features mask page is %lu. Received page %hu",
           max_lmp_feature_page_index_.value(), page);
    return;
  }

  if (page == 0) {
    init_seq_runner_->QueueCommand(
        hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ReadLocalSupportedFeaturesCommandView>(
            hci_spec::kReadLocalSupportedFeatures),
        [this, page](const hci::EventPacket& cmd_complete) {
          if (hci_is_error(cmd_complete, WARN, "gap", "read local supported features failed")) {
            return;
          }
          auto params =
              cmd_complete.return_params<hci_spec::ReadLocalSupportedFeaturesReturnParams>();
          state_.features.SetPage(page, le64toh(params->lmp_features));
        });
    return;
  }

  if (!state_.features.HasBit(/*page=*/0u, hci_spec::LMPFeature::kExtendedFeatures)) {
    bt_log(WARN, "gap", "LMP features mask does not have extended features");
    max_lmp_feature_page_index_ = 0;
    return;
  }

  if (!max_lmp_feature_page_index_.has_value() || page <= max_lmp_feature_page_index_.value()) {
    // HCI_Read_Local_Extended_Features
    auto cmd_packet = hci::EmbossCommandPacket::New<
        pw::bluetooth::emboss::ReadLocalExtendedFeaturesCommandWriter>(
        hci_spec::kReadLocalExtendedFeatures);
    cmd_packet.view_t().page_number().Write(page);  // Try to read |page|

    init_seq_runner_->QueueCommand(
        std::move(cmd_packet), [this, page](const hci::EventPacket& cmd_complete) {
          if (hci_is_error(cmd_complete, WARN, "gap", "read local extended features failed")) {
            return;
          }
          auto params =
              cmd_complete.return_params<hci_spec::ReadLocalExtendedFeaturesReturnParams>();
          state_.features.SetPage(page, le64toh(params->extended_lmp_features));
          max_lmp_feature_page_index_ = params->maximum_page_number;
        });
  }
}

void AdapterImpl::UpdateInspectProperties() {
  inspect_properties_.adapter_id = adapter_node_.CreateString("adapter_id", identifier_.ToString());
  inspect_properties_.hci_version =
      adapter_node_.CreateString("hci_version", hci_spec::HCIVersionToString(state_.hci_version));

  inspect_properties_.bredr_max_num_packets = adapter_node_.CreateUint(
      "bredr_max_num_packets", state_.bredr_data_buffer_info.max_num_packets());
  inspect_properties_.bredr_max_data_length = adapter_node_.CreateUint(
      "bredr_max_data_length", state_.bredr_data_buffer_info.max_data_length());

  inspect_properties_.le_max_num_packets = adapter_node_.CreateUint(
      "le_max_num_packets", state_.low_energy_state.data_buffer_info().max_num_packets());
  inspect_properties_.le_max_data_length = adapter_node_.CreateUint(
      "le_max_data_length", state_.low_energy_state.data_buffer_info().max_data_length());

  inspect_properties_.sco_max_num_packets =
      adapter_node_.CreateUint("sco_max_num_packets", state_.sco_buffer_info.max_num_packets());
  inspect_properties_.sco_max_data_length =
      adapter_node_.CreateUint("sco_max_data_length", state_.sco_buffer_info.max_data_length());

  inspect_properties_.lmp_features =
      adapter_node_.CreateString("lmp_features", state_.features.ToString());

  auto le_features =
      bt_lib_cpp_string::StringPrintf("0x%016lx", state_.low_energy_state.supported_features());
  inspect_properties_.le_features = adapter_node_.CreateString("le_features", le_features);
}

void AdapterImpl::CleanUp() {
  if (init_state_ == State::kNotInitialized) {
    bt_log(DEBUG, "gap", "clean up: not initialized");
    return;
  }

  init_state_ = State::kNotInitialized;
  state_ = AdapterState();
  transport_error_cb_ = nullptr;

  // Destroy objects in reverse order of construction.
  low_energy_ = nullptr;
  bredr_ = nullptr;
  sdp_server_ = nullptr;
  bredr_discovery_manager_ = nullptr;
  le_advertising_manager_ = nullptr;
  le_connection_manager_ = nullptr;
  le_discovery_manager_ = nullptr;

  hci_le_connector_ = nullptr;
  hci_le_advertiser_ = nullptr;
  hci_le_scanner_ = nullptr;

  le_address_manager_ = nullptr;

  l2cap_ = nullptr;

  hci_.reset();
}

void AdapterImpl::OnTransportError() {
  bt_log(INFO, "gap", "HCI transport error");
  if (CompleteInitialization(/*success=*/false)) {
    return;
  }
  if (transport_error_cb_) {
    transport_error_cb_();
  }
}

void AdapterImpl::OnLeAutoConnectRequest(Peer* peer) {
  BT_DEBUG_ASSERT(le_connection_manager_);
  BT_DEBUG_ASSERT(peer);
  BT_DEBUG_ASSERT(peer->le());

  PeerId peer_id = peer->identifier();

  if (!peer->le()->should_auto_connect()) {
    bt_log(DEBUG, "gap",
           "ignoring auto-connection (peer->should_auto_connect() is false) (peer: %s)",
           bt_str(peer_id));
    return;
  }

  LowEnergyConnectionOptions options{.auto_connect = true};

  auto self = weak_self_.GetWeakPtr();
  le_connection_manager_->Connect(
      peer_id,
      [self, peer_id](auto result) {
        if (!self.is_alive()) {
          bt_log(DEBUG, "gap", "ignoring auto-connection (adapter destroyed)");
          return;
        }

        if (result.is_error()) {
          bt_log(INFO, "gap", "failed to auto-connect (peer: %s, error: %s)", bt_str(peer_id),
                 HostErrorToString(result.error_value()).c_str());
          return;
        }

        auto conn = std::move(result).value();
        BT_ASSERT(conn);
        bt_log(INFO, "gap", "peer auto-connected (peer: %s)", bt_str(peer_id));
        if (self->auto_conn_cb_) {
          self->auto_conn_cb_(std::move(conn));
        }
      },
      options);
}

bool AdapterImpl::IsLeRandomAddressChangeAllowed() {
  return hci_le_advertiser_->AllowsRandomAddressChange() &&
         hci_le_scanner_->AllowsRandomAddressChange() &&
         hci_le_connector_->AllowsRandomAddressChange();
}

std::unique_ptr<Adapter> Adapter::Create(hci::Transport::WeakPtr hci, gatt::GATT::WeakPtr gatt,
                                         std::unique_ptr<l2cap::ChannelManager> l2cap) {
  return std::make_unique<AdapterImpl>(hci, gatt, std::move(l2cap));
}

}  // namespace bt::gap
