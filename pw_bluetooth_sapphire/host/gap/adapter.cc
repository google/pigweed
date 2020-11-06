// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "adapter.h"

#include <endian.h>

#include "bredr_connection_manager.h"
#include "bredr_discovery_manager.h"
#include "low_energy_address_manager.h"
#include "low_energy_advertising_manager.h"
#include "low_energy_connection_manager.h"
#include "low_energy_discovery_manager.h"
#include "peer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/random.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/legacy_low_energy_advertiser.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/legacy_low_energy_scanner.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_connector.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/sequential_command_runner.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_manager.h"

namespace bt::gap {

// All asynchronous callbacks are posted on the Loop on which this Adapter
// instance is created.
class AdapterImpl final : public Adapter {
 public:
  // There must be an async_t dispatcher registered as a default when an AdapterImpl
  // instance is created. The Adapter instance will use it for all of its
  // asynchronous tasks.
  explicit AdapterImpl(fxl::WeakPtr<hci::Transport> hci, fxl::WeakPtr<gatt::GATT> gatt,
                       std::optional<fbl::RefPtr<l2cap::L2cap>> l2cap);
  ~AdapterImpl();

  AdapterId identifier() const override { return identifier_; }

  bool Initialize(InitializeCallback callback, fit::closure transport_closed_callback) override;

  void ShutDown() override;

  bool IsInitializing() const override { return init_state_ == State::kInitializing; }

  bool IsInitialized() const override { return init_state_ == State::kInitialized; }

  const AdapterState& state() const override { return state_; }

  fxl::WeakPtr<Adapter> AsWeakPtr() override { return weak_ptr_factory_.GetWeakPtr(); }

  BrEdrConnectionManager* bredr_connection_manager() const override {
    return bredr_connection_manager_.get();
  }

  BrEdrDiscoveryManager* bredr_discovery_manager() const override {
    return bredr_discovery_manager_.get();
  }

  sdp::Server* sdp_server() const override { return sdp_server_.get(); }

  LowEnergyAddressManager* le_address_manager() const override {
    ZX_DEBUG_ASSERT(le_address_manager_);
    return le_address_manager_.get();
  }

  LowEnergyDiscoveryManager* le_discovery_manager() const override {
    ZX_DEBUG_ASSERT(le_discovery_manager_);
    return le_discovery_manager_.get();
  }

  LowEnergyConnectionManager* le_connection_manager() const override {
    ZX_DEBUG_ASSERT(le_connection_manager_);
    return le_connection_manager_.get();
  }

  LowEnergyAdvertisingManager* le_advertising_manager() const override {
    ZX_DEBUG_ASSERT(le_advertising_manager_);
    return le_advertising_manager_.get();
  }

  PeerCache* peer_cache() override { return &peer_cache_; }

  bool AddBondedPeer(BondingData bonding_data) override;

  void SetPairingDelegate(fxl::WeakPtr<PairingDelegate> delegate) override;

  bool IsDiscoverable() const override;

  bool IsDiscovering() const override;

  void SetLocalName(std::string name, hci::StatusCallback callback) override;

  void SetDeviceClass(DeviceClass dev_class, hci::StatusCallback callback) override;

  using AutoConnectCallback = fit::function<void(LowEnergyConnectionRefPtr)>;
  void set_auto_connect_callback(AutoConnectCallback callback) override {
    auto_conn_cb_ = std::move(callback);
  }

  void AttachInspect(inspect::Node& parent, std::string name = "adapter") override;

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

  // Objects that perform BR/EDR procedures.
  std::unique_ptr<BrEdrConnectionManager> bredr_connection_manager_;
  std::unique_ptr<BrEdrDiscoveryManager> bredr_discovery_manager_;
  std::unique_ptr<sdp::Server> sdp_server_;

  // Callback to propagate ownership of an auto-connected LE link.
  AutoConnectCallback auto_conn_cb_;

  fxl::ThreadChecker thread_checker_;

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
}

AdapterImpl::~AdapterImpl() {
  if (IsInitialized())
    ShutDown();
}

bool AdapterImpl::Initialize(InitializeCallback callback, fit::closure transport_closed_cb) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
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
  init_seq_runner_->QueueCommand(hci::CommandPacket::New(hci::kReset));

  // HCI_Read_Local_Version_Information
  init_seq_runner_->QueueCommand(
      hci::CommandPacket::New(hci::kReadLocalVersionInfo),
      [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "read local version info failed")) {
          return;
        }
        auto params = cmd_complete.return_params<hci::ReadLocalVersionInfoReturnParams>();
        state_.hci_version_ = params->hci_version;
      });

  // HCI_Read_Local_Supported_Commands
  init_seq_runner_->QueueCommand(
      hci::CommandPacket::New(hci::kReadLocalSupportedCommands),
      [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "read local supported commands failed")) {
          return;
        }
        auto params = cmd_complete.return_params<hci::ReadLocalSupportedCommandsReturnParams>();
        std::memcpy(state_.supported_commands_, params->supported_commands,
                    sizeof(params->supported_commands));
      });

  // HCI_Read_Local_Supported_Features
  init_seq_runner_->QueueCommand(
      hci::CommandPacket::New(hci::kReadLocalSupportedFeatures),
      [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "read local supported features failed")) {
          return;
        }
        auto params = cmd_complete.return_params<hci::ReadLocalSupportedFeaturesReturnParams>();
        state_.features_.SetPage(0, le64toh(params->lmp_features));
      });

  // HCI_Read_BD_ADDR
  init_seq_runner_->QueueCommand(
      hci::CommandPacket::New(hci::kReadBDADDR), [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "read BR_ADDR failed")) {
          return;
        }
        auto params = cmd_complete.return_params<hci::ReadBDADDRReturnParams>();
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
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
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
  le_connection_manager()->SetPairingDelegate(delegate);
  bredr_connection_manager()->SetPairingDelegate(delegate);
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
  if (!bredr_discovery_manager()) {
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
  auto write_dev_class = hci::CommandPacket::New(hci::kWriteClassOfDevice,
                                                 sizeof(hci::WriteClassOfDeviceCommandParams));
  write_dev_class->mutable_payload<hci::WriteClassOfDeviceCommandParams>()->class_of_device =
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
}

void AdapterImpl::InitializeStep2(InitializeCallback callback) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
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
  if (state_.hci_version() < hci::HCIVersion::k4_2) {
    bt_log(WARN, "gap", "controller is using legacy HCI version %s",
           hci::HCIVersionToString(state_.hci_version()).c_str());
  }

  ZX_DEBUG_ASSERT(init_seq_runner_->IsReady());

  // If the controller supports the Read Buffer Size command then send it.
  // Otherwise we'll default to 0 when initializing the ACLDataChannel.
  if (state_.IsCommandSupported(14, hci::SupportedCommand::kReadBufferSize)) {
    // HCI_Read_Buffer_Size
    init_seq_runner_->QueueCommand(
        hci::CommandPacket::New(hci::kReadBufferSize),
        [this](const hci::EventPacket& cmd_complete) {
          if (hci_is_error(cmd_complete, WARN, "gap", "read buffer size failed")) {
            return;
          }
          auto params = cmd_complete.return_params<hci::ReadBufferSizeReturnParams>();
          uint16_t mtu = le16toh(params->hc_acl_data_packet_length);
          uint16_t max_count = le16toh(params->hc_total_num_acl_data_packets);
          if (mtu && max_count) {
            state_.bredr_data_buffer_info_ = hci::DataBufferInfo(mtu, max_count);
          }
        });
  }

  // HCI_LE_Read_Local_Supported_Features
  init_seq_runner_->QueueCommand(
      hci::CommandPacket::New(hci::kLEReadLocalSupportedFeatures),
      [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "LE read local supported features failed")) {
          return;
        }
        auto params = cmd_complete.return_params<hci::LEReadLocalSupportedFeaturesReturnParams>();
        state_.le_state_.supported_features_ = le64toh(params->le_features);
      });

  // HCI_LE_Read_Supported_States
  init_seq_runner_->QueueCommand(
      hci::CommandPacket::New(hci::kLEReadSupportedStates),
      [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "LE read local supported states failed")) {
          return;
        }
        auto params = cmd_complete.return_params<hci::LEReadSupportedStatesReturnParams>();
        state_.le_state_.supported_states_ = le64toh(params->le_states);
      });

  // HCI_LE_Read_Buffer_Size
  init_seq_runner_->QueueCommand(
      hci::CommandPacket::New(hci::kLEReadBufferSize),
      [this](const hci::EventPacket& cmd_complete) {
        if (hci_is_error(cmd_complete, WARN, "gap", "LE read buffer size failed")) {
          return;
        }
        auto params = cmd_complete.return_params<hci::LEReadBufferSizeReturnParams>();
        uint16_t mtu = le16toh(params->hc_le_acl_data_packet_length);
        uint8_t max_count = params->hc_total_num_le_acl_data_packets;
        if (mtu && max_count) {
          state_.le_state_.data_buffer_info_ = hci::DataBufferInfo(mtu, max_count);
        }
      });

  if (state_.features().HasBit(0u, hci::LMPFeature::kSecureSimplePairing)) {
    // HCI_Write_Simple_Pairing_Mode
    auto write_ssp = hci::CommandPacket::New(hci::kWriteSimplePairingMode,
                                             sizeof(hci::WriteSimplePairingModeCommandParams));
    write_ssp->mutable_payload<hci::WriteSimplePairingModeCommandParams>()->simple_pairing_mode =
        hci::GenericEnableParam::kEnable;
    init_seq_runner_->QueueCommand(std::move(write_ssp), [](const auto& event) {
      // Warn if the command failed
      hci_is_error(event, WARN, "gap", "write simple pairing mode failed");
    });
  }

  // If there are extended features then try to read the first page of the
  // extended features.
  if (state_.features().HasBit(0u, hci::LMPFeature::kExtendedFeatures)) {
    // Page index 1 must be available.
    max_lmp_feature_page_index_ = 1;

    // HCI_Read_Local_Extended_Features
    auto cmd_packet = hci::CommandPacket::New(hci::kReadLocalExtendedFeatures,
                                              sizeof(hci::ReadLocalExtendedFeaturesCommandParams));

    // Try to read page 1.
    cmd_packet->mutable_payload<hci::ReadLocalExtendedFeaturesCommandParams>()->page_number = 1;

    init_seq_runner_->QueueCommand(
        std::move(cmd_packet), [this](const hci::EventPacket& cmd_complete) {
          if (hci_is_error(cmd_complete, WARN, "gap", "read local extended features failed")) {
            return;
          }
          auto params = cmd_complete.return_params<hci::ReadLocalExtendedFeaturesReturnParams>();
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
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
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
    auto l2cap = l2cap::L2cap::Create(hci_, /*random_channel_ids=*/true);
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
    auto cmd_packet =
        hci::CommandPacket::New(hci::kSetEventMask, sizeof(hci::SetEventMaskCommandParams));
    cmd_packet->mutable_payload<hci::SetEventMaskCommandParams>()->event_mask = htole64(event_mask);
    init_seq_runner_->QueueCommand(std::move(cmd_packet), [](const auto& event) {
      hci_is_error(event, WARN, "gap", "set event mask failed");
    });
  }

  // HCI_LE_Set_Event_Mask
  {
    uint64_t event_mask = BuildLEEventMask();
    auto cmd_packet =
        hci::CommandPacket::New(hci::kLESetEventMask, sizeof(hci::LESetEventMaskCommandParams));
    cmd_packet->mutable_payload<hci::LESetEventMaskCommandParams>()->le_event_mask =
        htole64(event_mask);
    init_seq_runner_->QueueCommand(std::move(cmd_packet), [](const auto& event) {
      hci_is_error(event, WARN, "gap", "LE set event mask failed");
    });
  }

  // HCI_Write_LE_Host_Support if the appropriate feature bit is not set AND if
  // the controller supports this command.
  if (!state_.features().HasBit(1, hci::LMPFeature::kLESupportedHost) &&
      state_.IsCommandSupported(24, hci::SupportedCommand::kWriteLEHostSupport)) {
    auto cmd_packet = hci::CommandPacket::New(hci::kWriteLEHostSupport,
                                              sizeof(hci::WriteLEHostSupportCommandParams));
    auto params = cmd_packet->mutable_payload<hci::WriteLEHostSupportCommandParams>();
    params->le_supported_host = hci::GenericEnableParam::kEnable;
    params->simultaneous_le_host = 0x00;  // note: ignored
    init_seq_runner_->QueueCommand(std::move(cmd_packet), [](const auto& event) {
      hci_is_error(event, WARN, "gap", "write LE host support failed");
    });
  }

  // If we know that Page 2 of the extended features bitfield is available, then
  // request it.
  if (max_lmp_feature_page_index_ > 1) {
    auto cmd_packet = hci::CommandPacket::New(hci::kReadLocalExtendedFeatures,
                                              sizeof(hci::ReadLocalExtendedFeaturesCommandParams));

    // Try to read page 2.
    cmd_packet->mutable_payload<hci::ReadLocalExtendedFeaturesCommandParams>()->page_number = 2;

    // HCI_Read_Local_Extended_Features
    init_seq_runner_->QueueCommand(
        std::move(cmd_packet), [this](const hci::EventPacket& cmd_complete) {
          if (hci_is_error(cmd_complete, WARN, "gap", "read local extended features failed")) {
            return;
          }
          auto params = cmd_complete.return_params<hci::ReadLocalExtendedFeaturesReturnParams>();
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
  ZX_DEBUG_ASSERT(IsInitializing());

  // Initialize the scan manager based on current feature support.
  if (state_.low_energy_state().IsFeatureSupported(
          hci::LESupportedFeature::kLEExtendedAdvertising)) {
    bt_log(INFO, "gap", "controller supports extended advertising");

    // TODO(armansito): Initialize |hci_le_*| objects here with extended-mode
    // versions.
    bt_log(WARN, "gap", "5.0 not yet supported; using legacy mode");
  }

  // We use the public controller address as the local LE identity address.
  DeviceAddress adapter_identity(DeviceAddress::Type::kLEPublic, state_.controller_address());

  // Initialize the LE local address manager.
  le_address_manager_ = std::make_unique<LowEnergyAddressManager>(
      adapter_identity, fit::bind_member(this, &AdapterImpl::IsLeRandomAddressChangeAllowed), hci_);

  // Initialize the HCI adapters.
  hci_le_advertiser_ = std::make_unique<hci::LegacyLowEnergyAdvertiser>(hci_);
  hci_le_connector_ = std::make_unique<hci::LowEnergyConnector>(
      hci_, le_address_manager_.get(), dispatcher_,
      fit::bind_member(hci_le_advertiser_.get(), &hci::LowEnergyAdvertiser::OnIncomingConnection));
  hci_le_scanner_ =
      std::make_unique<hci::LegacyLowEnergyScanner>(le_address_manager_.get(), hci_, dispatcher_);

  // Initialize the LE manager objects
  le_discovery_manager_ =
      std::make_unique<LowEnergyDiscoveryManager>(hci_, hci_le_scanner_.get(), &peer_cache_);
  le_discovery_manager_->set_peer_connectable_callback(
      fit::bind_member(this, &AdapterImpl::OnLeAutoConnectRequest));
  le_connection_manager_ = std::make_unique<LowEnergyConnectionManager>(
      hci_, le_address_manager_.get(), hci_le_connector_.get(), &peer_cache_, l2cap_, gatt_,
      sm::SecurityManager::Create);
  le_advertising_manager_ = std::make_unique<LowEnergyAdvertisingManager>(
      hci_le_advertiser_.get(), le_address_manager_.get());

  // Initialize the BR/EDR manager objects if the controller supports BR/EDR.
  if (state_.IsBREDRSupported()) {
    DeviceAddress local_bredr_address(DeviceAddress::Type::kBREDR, state_.controller_address());

    bredr_connection_manager_ = std::make_unique<BrEdrConnectionManager>(
        hci_, &peer_cache_, local_bredr_address, l2cap_,
        state_.features().HasBit(0, hci::LMPFeature::kInterlacedPageScan));

    hci::InquiryMode mode = hci::InquiryMode::kStandard;
    if (state_.features().HasBit(0, hci::LMPFeature::kExtendedInquiryResponse)) {
      mode = hci::InquiryMode::kExtended;
    } else if (state_.features().HasBit(0, hci::LMPFeature::kRSSIwithInquiryResults)) {
      mode = hci::InquiryMode::kRSSI;
    }

    bredr_discovery_manager_ = std::make_unique<BrEdrDiscoveryManager>(hci_, mode, &peer_cache_);

    sdp_server_ = std::make_unique<sdp::Server>(l2cap_);
    sdp_server_->AttachInspect(adapter_node_);
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
      adapter_node_.CreateString("hci_version", hci::HCIVersionToString(state_.hci_version()));

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

  auto le_features = fxl::StringPrintf("0x%016lx", state_.low_energy_state().supported_features());
  inspect_properties_.le_features = adapter_node_.CreateString("le_features", le_features);
}

uint64_t AdapterImpl::BuildEventMask() {
  uint64_t event_mask = 0;

#define ENABLE_EVT(event) event_mask |= static_cast<uint64_t>(hci::EventMask::event)

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

#define ENABLE_EVT(event) event_mask |= static_cast<uint64_t>(hci::LEEventMask::event)

  ENABLE_EVT(kLEAdvertisingReport);
  ENABLE_EVT(kLEConnectionComplete);
  ENABLE_EVT(kLEConnectionUpdateComplete);
  ENABLE_EVT(kLELongTermKeyRequest);
  ENABLE_EVT(kLEReadRemoteFeaturesComplete);

#undef ENABLE_EVT

  return event_mask;
}

void AdapterImpl::CleanUp() {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  if (init_state_ == State::kNotInitialized) {
    bt_log(DEBUG, "gap", "clean up: not initialized");
    return;
  }

  init_state_ = State::kNotInitialized;
  state_ = AdapterState();
  transport_closed_cb_ = nullptr;

  // Destroy objects in reverse order of construction.
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

  if (!peer->le()->should_auto_connect()) {
    bt_log(TRACE, "gap", "ignoring auto-connection (peer->should_auto_connect() is false)");
    return;
  }

  auto self = weak_ptr_factory_.GetWeakPtr();
  le_connection_manager_->Connect(peer->identifier(), [self](auto status, auto conn) {
    if (!self) {
      bt_log(DEBUG, "gap", "ignoring auto-connection (adapter destroyed)");
      return;
    }

    if (bt_is_error(status, ERROR, "gap", "failed to auto-connect")) {
      return;
    }

    ZX_DEBUG_ASSERT(conn);
    PeerId id = conn->peer_identifier();
    bt_log(INFO, "gap", "peer auto-connected (id: %s)", bt_str(id));
    if (self->auto_conn_cb_) {
      self->auto_conn_cb_(std::move(conn));
    }
  });
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
