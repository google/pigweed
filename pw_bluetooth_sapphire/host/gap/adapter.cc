// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "adapter.h"

#include <endian.h>
#include <lib/fit/thread_checker.h>

#include "bredr_connection_manager.h"
#include "bredr_discovery_manager.h"
#include "low_energy_address_manager.h"
#include "low_energy_advertising_manager.h"
#include "low_energy_connection_manager.h"
#include "low_energy_discovery_manager.h"
#include "peer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/metrics.h"
#include "src/connectivity/bluetooth/core/bt-host/common/random.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/util.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/extended_low_energy_advertiser.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/legacy_low_energy_advertiser.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/legacy_low_energy_scanner.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_connector.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/sequential_command_runner.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::gap {

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
  explicit AdapterImpl(fxl::WeakPtr<hci::Transport> hci, fxl::WeakPtr<gatt::GATT> gatt,
                       std::optional<fbl::RefPtr<l2cap::L2cap>> l2cap);
  ~AdapterImpl() override;

  AdapterId identifier() const override { return identifier_; }

  bool Initialize(InitializeCallback callback, fit::closure transport_closed_callback) override;

  void ShutDown() override;

  bool IsInitializing() const override { return init_state_ == State::kInitializing; }

  bool IsInitialized() const override { return init_state_ == State::kInitialized; }

  const AdapterState& state() const override { return state_; }

  class LowEnergyImpl final : public LowEnergy {
   public:
    explicit LowEnergyImpl(AdapterImpl* adapter) : adapter_(adapter) {}

    void Connect(PeerId peer_id, ConnectionResultCallback callback,
                 LowEnergyConnectionOptions connection_options) override {
      adapter_->le_connection_manager_->Connect(peer_id, std::move(callback),
                                                std::move(connection_options));
      adapter_->metrics_.le.outgoing_connection_requests.Add();
    }

    bool Disconnect(PeerId peer_id) override {
      return adapter_->le_connection_manager_->Disconnect(peer_id);
    }

    void Pair(PeerId peer_id, sm::SecurityLevel pairing_level, sm::BondableMode bondable_mode,
              sm::StatusCallback cb) override {
      adapter_->le_connection_manager_->Pair(peer_id, pairing_level, bondable_mode, std::move(cb));
      adapter_->metrics_.le.pair_requests.Add();
    }

    void SetSecurityMode(LeSecurityMode mode) override {
      adapter_->le_connection_manager_->SetSecurityMode(mode);
    }

    LeSecurityMode security_mode() const override {
      return adapter_->le_connection_manager_->security_mode();
    }

    void StartAdvertising(AdvertisingData data, AdvertisingData scan_rsp,
                          AdvertisingInterval interval, bool anonymous, bool include_tx_power_level,
                          std::optional<ConnectableAdvertisingParameters> connectable,
                          AdvertisingStatusCallback status_callback) override {
      LowEnergyAdvertisingManager::ConnectionCallback advertisement_connect_cb = nullptr;
      if (connectable) {
        ZX_ASSERT(connectable->connection_cb);

        // All advertisement connections are first registered with LowEnergyConnectionManager before
        // being reported to higher layers.
        advertisement_connect_cb = [this, connectable = std::move(connectable)](
                                       AdvertisementId advertisement_id,
                                       std::unique_ptr<hci::Connection> link) mutable {
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
              hci::StatusCallback callback) override {
      adapter_->bredr_connection_manager_->Pair(peer_id, security, std::move(callback));
      adapter_->metrics_.bredr.pair_requests.Add();
    }

    void SetConnectable(bool connectable, hci::StatusCallback status_cb) override {
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
        PeerId peer_id, hci_spec::SynchronousConnectionParameters parameters,
        sco::ScoConnectionManager::OpenConnectionCallback callback) override {
      return adapter_->bredr_connection_manager_->OpenScoConnection(peer_id, parameters,
                                                                    std::move(callback));
    }
    std::optional<ScoRequestHandle> AcceptScoConnection(
        PeerId peer_id, std::vector<hci_spec::SynchronousConnectionParameters> parameters,
        sco::ScoConnectionManager::AcceptConnectionCallback callback) override {
      return adapter_->bredr_connection_manager_->AcceptScoConnection(peer_id, parameters,
                                                                      std::move(callback));
    }

   private:
    AdapterImpl* adapter_;
  };

  BrEdr* bredr() const override { return bredr_.get(); }

  PeerCache* peer_cache() override { return &peer_cache_; }

  bool AddBondedPeer(BondingData bonding_data) override;

  void SetPairingDelegate(fxl::WeakPtr<PairingDelegate> delegate) override;

  bool IsDiscoverable() const override;

  bool IsDiscovering() const override;

  void SetLocalName(std::string name, hci::StatusCallback callback) override;

  std::string local_name() const override { return bredr_discovery_manager_->local_name(); }

  void SetDeviceClass(DeviceClass dev_class, hci::StatusCallback callback) override;

  void set_auto_connect_callback(AutoConnectCallback callback) override {
    auto_conn_cb_ = std::move(callback);
  }

  void AttachInspect(inspect::Node& parent, std::string name) override;

  fxl::WeakPtr<Adapter> AsWeakPtr() override { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  // Second step of the initialization sequence. Called by Initialize() when the
  // first batch of HCI commands have been sent.
  void InitializeStep2(InitializeCallback callback);

  // Third step of the initialization sequence. Called by InitializeStep2() when
  // the second batch of HCI commands have been sent.
  void InitializeStep3(InitializeCallback callback);

  // Fourth step of the initialization sequence. Called by InitializeStep3()
  // when the third batch of HCI commands have been sent.
  void InitializeStep4(InitializeCallback callback);

  // Assigns properties to |adapter_node_| using values discovered during other initialization
  // steps.
  void UpdateInspectProperties();

  // Builds and returns the HCI event mask based on our supported host side
  // features and controller capabilities. This is used to mask events that we
  // do not know how to handle.
  uint64_t BuildEventMask();

  // Builds and returns the LE event mask based on our supported host side
  // features and controller capabilities. This is used to mask LE events that
  // we do not know how to handle.
  uint64_t BuildLEEventMask();

  // Called by ShutDown() and during Initialize() in case of failure. This
  // synchronously cleans up the transports and resets initialization state.
  void CleanUp();

  // Called by Transport after it has been unexpectedly closed.
  void OnTransportClosed();

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
    if (state_.low_energy_state().IsFeatureSupported(feature)) {
      bt_log(INFO, "gap", "controller supports extended advertising, using extended LE commands");
      return std::make_unique<hci::ExtendedLowEnergyAdvertiser>(hci_);
    }

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
  fxl::WeakPtr<hci::Transport> hci_;

  // Callback invoked to notify clients when the underlying transport is closed.
  fit::closure transport_closed_cb_;

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

  // Contains the global adapter state.
  AdapterState state_;

  // The maximum LMP feature page that we will read.
  size_t max_lmp_feature_page_index_;

  // Provides access to discovered, connected, and/or bonded remote Bluetooth
  // devices.
  PeerCache peer_cache_;

  // The data domain used by GAP to interact with L2CAP and RFCOMM layers.
  fbl::RefPtr<l2cap::L2cap> l2cap_;

  // The GATT profile. We use this reference to add and remove data bearers and
  // for service discovery.
  fxl::WeakPtr<gatt::GATT> gatt_;

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

  fit::thread_checker thread_checker_;

  // This must remain the last member to make sure that all weak pointers are
  // invalidating before other members are destroyed.
  fxl::WeakPtrFactory<AdapterImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AdapterImpl);
};

AdapterImpl::AdapterImpl(fxl::WeakPtr<hci::Transport> hci, fxl::WeakPtr<gatt::GATT> gatt,
                         std::optional<fbl::RefPtr<l2cap::L2cap>> l2cap)
    : identifier_(Random<AdapterId>()),
      dispatcher_(async_get_default_dispatcher()),
      hci_(std::move(hci)),
      init_state_(State::kNotInitialized),
      max_lmp_feature_page_index_(0),
      peer_cache_(),
      gatt_(gatt),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(hci_);
  ZX_DEBUG_ASSERT(gatt_);
  ZX_DEBUG_ASSERT_MSG(dispatcher_, "must create on a thread with a dispatcher");

  init_seq_runner_ = std::make_unique<hci::SequentialCommandRunner>(dispatcher_, hci_);

  if (l2cap.has_value()) {
    l2cap_ = *l2cap;
  }

  auto self = weak_ptr_factory_.GetWeakPtr();
  hci_->SetTransportClosedCallback([self] {
    if (self) {
      self->OnTransportClosed();
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

bool AdapterImpl::Initialize(InitializeCallback callback, fit::closure transport_closed_cb) {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
  ZX_DEBUG_ASSERT(callback);
  ZX_DEBUG_ASSERT(transport_closed_cb);

  if (IsInitialized()) {
    bt_log(WARN, "gap", "Adapter already initialized");
    return false;
  }

  ZX_DEBUG_ASSERT(!IsInitializing());

  init_state_ = State::kInitializing;

  ZX_DEBUG_ASSERT(init_seq_runner_->IsReady());
  ZX_DEBUG_ASSERT(!init_seq_runner_->HasQueuedCommands());

  transport_closed_cb_ = std::move(transport_closed_cb);

  state_.vendor_features_ = hci_->GetVendorFeatures();

  // Start by resetting the controller to a clean state and then send
  // informational parameter commands that are not specific to LE or BR/EDR. The
  // commands sent here are mandatory for all LE controllers.
  //
  // NOTE: It's safe to pass capture |this| directly in the callbacks as
  // |init_seq_runner_| will internally invalidate the callbacks if it ever gets
  // deleted.

  // HCI_Reset
  init_seq_runner_->QueueCommand(hci::CommandPacket::New(hci_spec::kReset));

  // HCI_Read_Local_Version_Information
  init_seq_runner_->QueueCommand(
      hci::CommandPacket::New(hci_spec::kReadLocalVersionInfo),
      [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "read local version info failed")) {
          return;
        }
        auto params = cmd_complete.return_params<hci_spec::ReadLocalVersionInfoReturnParams>();
        state_.hci_version_ = params->hci_version;
      });

  // HCI_Read_Local_Supported_Commands
  init_seq_runner_->QueueCommand(
      hci::CommandPacket::New(hci_spec::kReadLocalSupportedCommands),
      [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "read local supported commands failed")) {
          return;
        }
        auto params =
            cmd_complete.return_params<hci_spec::ReadLocalSupportedCommandsReturnParams>();
        std::memcpy(state_.supported_commands_, params->supported_commands,
                    sizeof(params->supported_commands));
      });

  // HCI_Read_Local_Supported_Features
  init_seq_runner_->QueueCommand(
      hci::CommandPacket::New(hci_spec::kReadLocalSupportedFeatures),
      [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "read local supported features failed")) {
          return;
        }
        auto params =
            cmd_complete.return_params<hci_spec::ReadLocalSupportedFeaturesReturnParams>();
        state_.features_.SetPage(0, le64toh(params->lmp_features));
      });

  // HCI_Read_BD_ADDR
  init_seq_runner_->QueueCommand(
      hci::CommandPacket::New(hci_spec::kReadBDADDR), [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "read BR_ADDR failed")) {
          return;
        }
        auto params = cmd_complete.return_params<hci_spec::ReadBDADDRReturnParams>();
        state_.controller_address_ = params->bd_addr;
      });

  init_seq_runner_->RunCommands([callback = std::move(callback), this](hci::Status status) mutable {
    if (!status) {
      bt_log(ERROR, "gap", "Failed to obtain initial controller information: %s",
             status.ToString().c_str());
      CleanUp();
      callback(false);
      return;
    }

    InitializeStep2(std::move(callback));
  });

  return true;
}

void AdapterImpl::ShutDown() {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
  bt_log(DEBUG, "gap", "adapter shutting down");

  if (IsInitializing()) {
    ZX_DEBUG_ASSERT(!init_seq_runner_->IsReady());
    init_seq_runner_->Cancel();
  }

  CleanUp();
}

bool AdapterImpl::AddBondedPeer(BondingData bonding_data) {
  return peer_cache()->AddBondedPeer(bonding_data);
}

void AdapterImpl::SetPairingDelegate(fxl::WeakPtr<PairingDelegate> delegate) {
  le_connection_manager_->SetPairingDelegate(delegate);
  bredr_connection_manager_->SetPairingDelegate(delegate);
}

bool AdapterImpl::IsDiscoverable() const {
  return (le_advertising_manager_ && le_advertising_manager_->advertising()) ||
         (bredr_discovery_manager_ && bredr_discovery_manager_->discoverable());
}

bool AdapterImpl::IsDiscovering() const {
  return (le_discovery_manager_ && le_discovery_manager_->discovering()) ||
         (bredr_discovery_manager_ && bredr_discovery_manager_->discovering());
}

void AdapterImpl::SetLocalName(std::string name, hci::StatusCallback callback) {
  // TODO(fxbug.dev/40836): set the public LE advertisement name from |name|
  // If BrEdr is not supported, skip the name update.
  if (!bredr_discovery_manager_) {
    callback(hci::Status(bt::HostError::kNotSupported));
    return;
  }

  // Make a copy of |name| to move separately into the lambda.
  std::string name_copy(name);
  bredr_discovery_manager_->UpdateLocalName(
      std::move(name),
      [this, cb = std::move(callback), local_name = std::move(name_copy)](auto status) {
        if (!bt_is_error(status, WARN, "gap", "set local name failed")) {
          state_.local_name_ = local_name;
        }
        cb(status);
      });
}

void AdapterImpl::SetDeviceClass(DeviceClass dev_class, hci::StatusCallback callback) {
  auto write_dev_class = hci::CommandPacket::New(hci_spec::kWriteClassOfDevice,
                                                 sizeof(hci_spec::WriteClassOfDeviceCommandParams));
  write_dev_class->mutable_payload<hci_spec::WriteClassOfDeviceCommandParams>()->class_of_device =
      dev_class;
  hci_->command_channel()->SendCommand(
      std::move(write_dev_class), [cb = std::move(callback)](auto, const hci::EventPacket& event) {
        hci_is_error(event, WARN, "gap", "set device class failed");
        cb(event.ToStatus());
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

void AdapterImpl::InitializeStep2(InitializeCallback callback) {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
  ZX_DEBUG_ASSERT(IsInitializing());

  // Low Energy MUST be supported. We don't support BR/EDR-only controllers.
  if (!state_.IsLowEnergySupported()) {
    bt_log(ERROR, "gap", "Bluetooth LE not supported by controller");
    CleanUp();
    callback(false);
    return;
  }

  // Check the HCI version. We officially only support 4.2+ only but for now we
  // just log a warning message if the version is legacy.
  if (state_.hci_version() < hci_spec::HCIVersion::k4_2) {
    bt_log(WARN, "gap", "controller is using legacy HCI version %s",
           hci_spec::HCIVersionToString(state_.hci_version()).c_str());
  }

  ZX_DEBUG_ASSERT(init_seq_runner_->IsReady());

  // If the controller supports the Read Buffer Size command then send it.
  // Otherwise we'll default to 0 when initializing the ACLDataChannel.
  if (state_.IsCommandSupported(14, hci_spec::SupportedCommand::kReadBufferSize)) {
    // HCI_Read_Buffer_Size
    init_seq_runner_->QueueCommand(
        hci::CommandPacket::New(hci_spec::kReadBufferSize),
        [this](const hci::EventPacket& cmd_complete) {
          if (hci_is_error(cmd_complete, WARN, "gap", "read buffer size failed")) {
            return;
          }
          auto params = cmd_complete.return_params<hci_spec::ReadBufferSizeReturnParams>();
          uint16_t mtu = le16toh(params->hc_acl_data_packet_length);
          uint16_t max_count = le16toh(params->hc_total_num_acl_data_packets);
          if (mtu && max_count) {
            state_.bredr_data_buffer_info_ = hci::DataBufferInfo(mtu, max_count);
          }
        });
  }

  // HCI_LE_Read_Local_Supported_Features
  init_seq_runner_->QueueCommand(
      hci::CommandPacket::New(hci_spec::kLEReadLocalSupportedFeatures),
      [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "LE read local supported features failed")) {
          return;
        }
        auto params =
            cmd_complete.return_params<hci_spec::LEReadLocalSupportedFeaturesReturnParams>();
        state_.le_state_.supported_features_ = le64toh(params->le_features);
      });

  // HCI_LE_Read_Supported_States
  init_seq_runner_->QueueCommand(
      hci::CommandPacket::New(hci_spec::kLEReadSupportedStates),
      [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "LE read local supported states failed")) {
          return;
        }
        auto params = cmd_complete.return_params<hci_spec::LEReadSupportedStatesReturnParams>();
        state_.le_state_.supported_states_ = le64toh(params->le_states);
      });

  // HCI_LE_Read_Buffer_Size
  init_seq_runner_->QueueCommand(
      hci::CommandPacket::New(hci_spec::kLEReadBufferSize),
      [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "LE read buffer size failed")) {
          return;
        }
        auto params = cmd_complete.return_params<hci_spec::LEReadBufferSizeReturnParams>();
        uint16_t mtu = le16toh(params->hc_le_acl_data_packet_length);
        uint8_t max_count = params->hc_total_num_le_acl_data_packets;
        if (mtu && max_count) {
          state_.le_state_.data_buffer_info_ = hci::DataBufferInfo(mtu, max_count);
        }
      });

  if (state_.features().HasBit(0u, hci_spec::LMPFeature::kSecureSimplePairing)) {
    // HCI_Write_Simple_Pairing_Mode
    auto write_ssp = hci::CommandPacket::New(hci_spec::kWriteSimplePairingMode,
                                             sizeof(hci_spec::WriteSimplePairingModeCommandParams));
    write_ssp->mutable_payload<hci_spec::WriteSimplePairingModeCommandParams>()
        ->simple_pairing_mode = hci_spec::GenericEnableParam::kEnable;
    init_seq_runner_->QueueCommand(std::move(write_ssp), [](const auto& event) {
      // Warn if the command failed
      hci_is_error(event, WARN, "gap", "write simple pairing mode failed");
    });
  }

  // If there are extended features then try to read the first page of the
  // extended features.
  if (state_.features().HasBit(0u, hci_spec::LMPFeature::kExtendedFeatures)) {
    // Page index 1 must be available.
    max_lmp_feature_page_index_ = 1;

    // HCI_Read_Local_Extended_Features
    auto cmd_packet =
        hci::CommandPacket::New(hci_spec::kReadLocalExtendedFeatures,
                                sizeof(hci_spec::ReadLocalExtendedFeaturesCommandParams));

    // Try to read page 1.
    cmd_packet->mutable_payload<hci_spec::ReadLocalExtendedFeaturesCommandParams>()->page_number =
        1;

    init_seq_runner_->QueueCommand(
        std::move(cmd_packet), [this](const hci::EventPacket& cmd_complete) {
          if (hci_is_error(cmd_complete, WARN, "gap", "read local extended features failed")) {
            return;
          }
          auto params =
              cmd_complete.return_params<hci_spec::ReadLocalExtendedFeaturesReturnParams>();
          state_.features_.SetPage(1, le64toh(params->extended_lmp_features));
          max_lmp_feature_page_index_ = params->maximum_page_number;
        });
  }

  init_seq_runner_->RunCommands([callback = std::move(callback), this](hci::Status status) mutable {
    if (bt_is_error(status, ERROR, "gap",
                    "failed to obtain initial controller information (step 2)")) {
      CleanUp();
      callback(false);
      return;
    }
    InitializeStep3(std::move(callback));
  });
}

void AdapterImpl::InitializeStep3(InitializeCallback callback) {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
  ZX_DEBUG_ASSERT(IsInitializing());

  if (!state_.bredr_data_buffer_info().IsAvailable() &&
      !state_.low_energy_state().data_buffer_info().IsAvailable()) {
    bt_log(ERROR, "gap", "Both BR/EDR and LE buffers are unavailable");
    CleanUp();
    callback(false);
    return;
  }

  // Now that we have all the ACL data buffer information it's time to
  // initialize the ACLDataChannel.
  if (!hci_->InitializeACLDataChannel(state_.bredr_data_buffer_info(),
                                      state_.low_energy_state().data_buffer_info())) {
    bt_log(ERROR, "gap", "Failed to initialize ACLDataChannel (step 3)");
    CleanUp();
    callback(false);
    return;
  }

  // Create the data domain, if we haven't been provided one. Doing so here lets us guarantee that
  // AclDataChannel's lifetime is a superset of Data L2cap's lifetime.
  // TODO(fxbug.dev/35228) We currently allow tests to inject their own domain in the adapter
  // constructor, as the adapter_unittests rely on injecting a fake domain to avoid concurrency in
  // the unit tests.  Once we move to a single threaded model, we would like to remove this and have
  // the adapter always be responsible for creating the domain.
  if (!l2cap_) {
    // Initialize the data L2cap to make L2CAP available for the next initialization step. The
    // ACLDataChannel must be initialized before creating the data domain
    auto l2cap = l2cap::L2cap::Create(hci_->acl_data_channel(), /*random_channel_ids=*/true);
    if (!l2cap) {
      bt_log(ERROR, "gap", "Failed to initialize Data L2cap");
      CleanUp();
      callback(false);
      return;
    }
    l2cap->AttachInspect(adapter_node_, l2cap::L2cap::kInspectNodeName);
    l2cap_ = l2cap;
  }

  ZX_DEBUG_ASSERT(init_seq_runner_->IsReady());
  ZX_DEBUG_ASSERT(!init_seq_runner_->HasQueuedCommands());

  // HCI_Set_Event_Mask
  {
    uint64_t event_mask = BuildEventMask();
    auto cmd_packet = hci::CommandPacket::New(hci_spec::kSetEventMask,
                                              sizeof(hci_spec::SetEventMaskCommandParams));
    cmd_packet->mutable_payload<hci_spec::SetEventMaskCommandParams>()->event_mask =
        htole64(event_mask);
    init_seq_runner_->QueueCommand(std::move(cmd_packet), [](const auto& event) {
      hci_is_error(event, WARN, "gap", "set event mask failed");
    });
  }

  // HCI_LE_Set_Event_Mask
  {
    uint64_t event_mask = BuildLEEventMask();
    auto cmd_packet = hci::CommandPacket::New(hci_spec::kLESetEventMask,
                                              sizeof(hci_spec::LESetEventMaskCommandParams));
    cmd_packet->mutable_payload<hci_spec::LESetEventMaskCommandParams>()->le_event_mask =
        htole64(event_mask);
    init_seq_runner_->QueueCommand(std::move(cmd_packet), [](const auto& event) {
      hci_is_error(event, WARN, "gap", "LE set event mask failed");
    });
  }

  // HCI_Write_LE_Host_Support if the appropriate feature bit is not set AND if
  // the controller supports this command.
  if (!state_.features().HasBit(1, hci_spec::LMPFeature::kLESupportedHost) &&
      state_.IsCommandSupported(24, hci_spec::SupportedCommand::kWriteLEHostSupport)) {
    auto cmd_packet = hci::CommandPacket::New(hci_spec::kWriteLEHostSupport,
                                              sizeof(hci_spec::WriteLEHostSupportCommandParams));
    auto params = cmd_packet->mutable_payload<hci_spec::WriteLEHostSupportCommandParams>();
    params->le_supported_host = hci_spec::GenericEnableParam::kEnable;
    params->simultaneous_le_host = 0x00;  // note: ignored
    init_seq_runner_->QueueCommand(std::move(cmd_packet), [](const auto& event) {
      hci_is_error(event, WARN, "gap", "write LE host support failed");
    });
  }

  // If we know that Page 2 of the extended features bitfield is available, then
  // request it.
  if (max_lmp_feature_page_index_ > 1) {
    auto cmd_packet =
        hci::CommandPacket::New(hci_spec::kReadLocalExtendedFeatures,
                                sizeof(hci_spec::ReadLocalExtendedFeaturesCommandParams));

    // Try to read page 2.
    cmd_packet->mutable_payload<hci_spec::ReadLocalExtendedFeaturesCommandParams>()->page_number =
        2;

    // HCI_Read_Local_Extended_Features
    init_seq_runner_->QueueCommand(
        std::move(cmd_packet), [this](const hci::EventPacket& cmd_complete) {
          if (hci_is_error(cmd_complete, WARN, "gap", "read local extended features failed")) {
            return;
          }
          auto params =
              cmd_complete.return_params<hci_spec::ReadLocalExtendedFeaturesReturnParams>();
          state_.features_.SetPage(2, le64toh(params->extended_lmp_features));
          max_lmp_feature_page_index_ = params->maximum_page_number;
        });
  }

  init_seq_runner_->RunCommands([callback = std::move(callback), this](hci::Status status) mutable {
    if (bt_is_error(status, ERROR, "gap",
                    "failed to obtain initial controller information (step 3)")) {
      CleanUp();
      callback(false);
      return;
    }
    InitializeStep4(std::move(callback));
  });
}

void AdapterImpl::InitializeStep4(InitializeCallback callback) {
  // Initialize the scan manager and low energy adapters based on current feature support
  ZX_DEBUG_ASSERT(IsInitializing());

  // We use the public controller address as the local LE identity address.
  DeviceAddress adapter_identity(DeviceAddress::Type::kLEPublic, state_.controller_address());

  // Initialize the LE local address manager.
  le_address_manager_ = std::make_unique<LowEnergyAddressManager>(
      adapter_identity, fit::bind_member(this, &AdapterImpl::IsLeRandomAddressChangeAllowed), hci_);

  // Initialize the HCI adapters.
  hci_le_advertiser_ = CreateAdvertiser();
  hci_le_connector_ = std::make_unique<hci::LowEnergyConnector>(
      hci_, le_address_manager_.get(), dispatcher_,
      fit::bind_member(hci_le_advertiser_.get(), &hci::LowEnergyAdvertiser::OnIncomingConnection));
  hci_le_scanner_ =
      std::make_unique<hci::LegacyLowEnergyScanner>(le_address_manager_.get(), hci_, dispatcher_);

  // Initialize the LE manager objects
  le_discovery_manager_ =
      std::make_unique<LowEnergyDiscoveryManager>(hci_, hci_le_scanner_.get(), &peer_cache_);
  le_discovery_manager_->AttachInspect(adapter_node_, kInspectLowEnergyDiscoveryManagerNodeName);
  le_discovery_manager_->set_peer_connectable_callback(
      fit::bind_member(this, &AdapterImpl::OnLeAutoConnectRequest));

  le_connection_manager_ = std::make_unique<LowEnergyConnectionManager>(
      hci_, le_address_manager_.get(), hci_le_connector_.get(), &peer_cache_, l2cap_, gatt_,
      le_discovery_manager_->GetWeakPtr(), sm::SecurityManager::Create);
  le_connection_manager_->AttachInspect(adapter_node_, kInspectLowEnergyConnectionManagerNodeName);

  le_advertising_manager_ = std::make_unique<LowEnergyAdvertisingManager>(
      hci_le_advertiser_.get(), le_address_manager_.get());
  low_energy_ = std::make_unique<LowEnergyImpl>(this);

  // Initialize the BR/EDR manager objects if the controller supports BR/EDR.
  if (state_.IsBREDRSupported()) {
    DeviceAddress local_bredr_address(DeviceAddress::Type::kBREDR, state_.controller_address());

    bredr_connection_manager_ = std::make_unique<BrEdrConnectionManager>(
        hci_, &peer_cache_, local_bredr_address, l2cap_,
        state_.features().HasBit(0, hci_spec::LMPFeature::kInterlacedPageScan));
    bredr_connection_manager_->AttachInspect(adapter_node_, kInspectBrEdrConnectionManagerNodeName);

    hci_spec::InquiryMode mode = hci_spec::InquiryMode::kStandard;
    if (state_.features().HasBit(0, hci_spec::LMPFeature::kExtendedInquiryResponse)) {
      mode = hci_spec::InquiryMode::kExtended;
    } else if (state_.features().HasBit(0, hci_spec::LMPFeature::kRSSIwithInquiryResults)) {
      mode = hci_spec::InquiryMode::kRSSI;
    }

    bredr_discovery_manager_ = std::make_unique<BrEdrDiscoveryManager>(hci_, mode, &peer_cache_);
    bredr_discovery_manager_->AttachInspect(adapter_node_, kInspectBrEdrDiscoveryManagerNodeName);

    sdp_server_ = std::make_unique<sdp::Server>(l2cap_);
    sdp_server_->AttachInspect(adapter_node_);

    bredr_ = std::make_unique<BrEdrImpl>(this);
  }

  // Override the current privacy setting and always use the local stable identity address (i.e. not
  // a RPA) when initiating connections. This improves interoperability with certain Bluetooth
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
  auto self = weak_ptr_factory_.GetWeakPtr();
  SetLocalName(kDefaultLocalName, [self, callback = std::move(callback)](auto status) mutable {
    // Set the default device class - a computer with audio.
    // TODO(fxbug.dev/1234): set this from a platform configuration file
    DeviceClass dev_class(DeviceClass::MajorClass::kComputer);
    dev_class.SetServiceClasses({DeviceClass::ServiceClass::kAudio});
    self->SetDeviceClass(dev_class, [self, callback = std::move(callback)](const auto&) {
      // This completes the initialization sequence.
      self->init_state_ = State::kInitialized;
      callback(true);
    });
  });
}

void AdapterImpl::UpdateInspectProperties() {
  inspect_properties_.adapter_id = adapter_node_.CreateString("adapter_id", identifier_.ToString());
  inspect_properties_.hci_version =
      adapter_node_.CreateString("hci_version", hci_spec::HCIVersionToString(state_.hci_version()));

  inspect_properties_.bredr_max_num_packets = adapter_node_.CreateUint(
      "bredr_max_num_packets", state_.bredr_data_buffer_info().max_num_packets());
  inspect_properties_.bredr_max_data_length = adapter_node_.CreateUint(
      "bredr_max_data_length", state_.bredr_data_buffer_info().max_data_length());

  inspect_properties_.le_max_num_packets = adapter_node_.CreateUint(
      "le_max_num_packets", state_.low_energy_state().data_buffer_info().max_num_packets());
  inspect_properties_.le_max_data_length = adapter_node_.CreateUint(
      "le_max_data_length", state_.low_energy_state().data_buffer_info().max_data_length());

  inspect_properties_.lmp_features =
      adapter_node_.CreateString("lmp_features", state_.features().ToString());

  auto le_features =
      bt_lib_cpp_string::StringPrintf("0x%016lx", state_.low_energy_state().supported_features());
  inspect_properties_.le_features = adapter_node_.CreateString("le_features", le_features);
}

uint64_t AdapterImpl::BuildEventMask() {
  uint64_t event_mask = 0;

#define ENABLE_EVT(event) event_mask |= static_cast<uint64_t>(hci_spec::EventMask::event)

  // Enable events that are needed for basic functionality. (alphabetic)
  ENABLE_EVT(kAuthenticationCompleteEvent);
  ENABLE_EVT(kConnectionCompleteEvent);
  ENABLE_EVT(kConnectionRequestEvent);
  ENABLE_EVT(kDataBufferOverflowEvent);
  ENABLE_EVT(kDisconnectionCompleteEvent);
  ENABLE_EVT(kEncryptionChangeEvent);
  ENABLE_EVT(kEncryptionKeyRefreshCompleteEvent);
  ENABLE_EVT(kExtendedInquiryResultEvent);
  ENABLE_EVT(kHardwareErrorEvent);
  ENABLE_EVT(kInquiryCompleteEvent);
  ENABLE_EVT(kInquiryResultEvent);
  ENABLE_EVT(kInquiryResultWithRSSIEvent);
  ENABLE_EVT(kIOCapabilityRequestEvent);
  ENABLE_EVT(kIOCapabilityResponseEvent);
  ENABLE_EVT(kLEMetaEvent);
  ENABLE_EVT(kLinkKeyRequestEvent);
  ENABLE_EVT(kLinkKeyNotificationEvent);
  ENABLE_EVT(kRemoteOOBDataRequestEvent);
  ENABLE_EVT(kRemoteNameRequestCompleteEvent);
  ENABLE_EVT(kReadRemoteSupportedFeaturesCompleteEvent);
  ENABLE_EVT(kReadRemoteVersionInformationCompleteEvent);
  ENABLE_EVT(kReadRemoteExtendedFeaturesCompleteEvent);
  ENABLE_EVT(kRoleChangeEvent);
  ENABLE_EVT(kSimplePairingCompleteEvent);
  ENABLE_EVT(kSynchronousConnectionCompleteEvent);
  ENABLE_EVT(kUserConfirmationRequestEvent);
  ENABLE_EVT(kUserPasskeyRequestEvent);
  ENABLE_EVT(kUserPasskeyNotificationEvent);

#undef ENABLE_EVT

  return event_mask;
}

uint64_t AdapterImpl::BuildLEEventMask() {
  uint64_t event_mask = 0;

#define ENABLE_EVT(event) event_mask |= static_cast<uint64_t>(hci_spec::LEEventMask::event)

  ENABLE_EVT(kLEAdvertisingReport);
  ENABLE_EVT(kLEConnectionComplete);
  ENABLE_EVT(kLEConnectionUpdateComplete);
  ENABLE_EVT(kLEExtendedAdvertisingSetTerminated);
  ENABLE_EVT(kLELongTermKeyRequest);
  ENABLE_EVT(kLEReadRemoteFeaturesComplete);

#undef ENABLE_EVT

  return event_mask;
}

void AdapterImpl::CleanUp() {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());

  if (init_state_ == State::kNotInitialized) {
    bt_log(DEBUG, "gap", "clean up: not initialized");
    return;
  }

  init_state_ = State::kNotInitialized;
  state_ = AdapterState();
  transport_closed_cb_ = nullptr;

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

  hci_ = nullptr;
}

void AdapterImpl::OnTransportClosed() {
  bt_log(INFO, "gap", "HCI transport was closed");
  if (transport_closed_cb_)
    transport_closed_cb_();
}

void AdapterImpl::OnLeAutoConnectRequest(Peer* peer) {
  ZX_DEBUG_ASSERT(le_connection_manager_);
  ZX_DEBUG_ASSERT(peer);
  ZX_DEBUG_ASSERT(peer->le());

  PeerId peer_id = peer->identifier();

  if (!peer->le()->should_auto_connect()) {
    bt_log(DEBUG, "gap",
           "ignoring auto-connection (peer->should_auto_connect() is false) (peer: %s)",
           bt_str(peer_id));
    return;
  }

  LowEnergyConnectionOptions options{.auto_connect = true};

  auto self = weak_ptr_factory_.GetWeakPtr();
  le_connection_manager_->Connect(
      peer_id,
      [self, peer_id](auto result) {
        if (!self) {
          bt_log(DEBUG, "gap", "ignoring auto-connection (adapter destroyed)");
          return;
        }

        if (result.is_error()) {
          bt_log(INFO, "gap", "failed to auto-connect (peer: %s, error: %s)", bt_str(peer_id),
                 HostErrorToString(result.error()).c_str());
          return;
        }

        auto conn = result.take_value();
        ZX_ASSERT(conn);
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

std::unique_ptr<Adapter> Adapter::Create(fxl::WeakPtr<hci::Transport> hci,
                                         fxl::WeakPtr<gatt::GATT> gatt,
                                         std::optional<fbl::RefPtr<l2cap::L2cap>> l2cap) {
  return std::make_unique<AdapterImpl>(hci, gatt, l2cap);
}

}  // namespace bt::gap
