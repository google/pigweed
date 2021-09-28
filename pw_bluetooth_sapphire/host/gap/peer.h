// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_PEER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_PEER_H_

#include <lib/fit/defer.h>
#include <lib/sys/inspect/cpp/component.h>

#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/inspectable.h"
#include "src/connectivity/bluetooth/core/bt-host/common/metrics.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_metrics.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/persisted_data.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/lmp_feature_set.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/security_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"

namespace bt::gap {

class PeerCache;

// Represents a remote Bluetooth device that is known to the current system due
// to discovery and/or connection and bonding procedures. These devices can be
// LE-only, Classic-only, or dual-mode.
//
// Instances should not be created directly and must be obtained via a
// PeerCache.
class Peer final {
 public:
  using PeerCallback = fit::function<void(const Peer&)>;

  // Describes the change(s) that caused the peer to notify listeners.
  enum class NotifyListenersChange {
    kBondNotUpdated,  // No persistent data has changed
    kBondUpdated,     // Persistent data has changed
  };
  using NotifyListenersCallback = fit::function<void(const Peer&, NotifyListenersChange)>;

  // Caller must ensure that callbacks are non-empty.
  // Note that the ctor is only intended for use by PeerCache.
  // Expanding access would a) violate the constraint that all Peers
  // are created through a PeerCache, and b) introduce lifetime issues
  // (do the callbacks outlive |this|?).
  Peer(NotifyListenersCallback notify_listeners_callback, PeerCallback update_expiry_callback,
       PeerCallback dual_mode_callback, PeerId identifier, const DeviceAddress& address,
       bool connectable, PeerMetrics* peer_metrics);

  // Connection state as considered by the GAP layer. This may not correspond
  // exactly with the presence or absence of a link at the link layer. For
  // example, GAP may consider a peer disconnected whilst the link disconnection
  // procedure is still continuing.
  enum class ConnectionState {
    // No link exists between the local adapter and peer or link is being torn
    // down (disconnection command has been sent).
    kNotConnected,

    // Currently establishing a link, performing service discovery, or
    // setting up encryption. In this state, a link may have been
    // established but it is not ready to use yet.
    kInitializing,

    // Link setup, service discovery, and any encryption setup has completed
    kConnected
  };
  static std::string ConnectionStateToString(Peer::ConnectionState);

  // Description of auto-connect behaviors.
  //
  // By default, the stack will auto-connect to any bonded devices as soon as
  // soon as they become available.
  enum class AutoConnectBehavior {
    // Always auto-connect device when possible.
    kAlways,

    // Ignore auto-connection possibilities, but reset to kAlways after the next
    // manual connection.
    kSkipUntilNextConnection,
  };

  static constexpr const char* kInspectPeerIdName = "peer_id";
  static constexpr const char* kInspectTechnologyName = "technology";
  static constexpr const char* kInspectAddressName = "address";
  static constexpr const char* kInspectConnectableName = "connectable";
  static constexpr const char* kInspectTemporaryName = "temporary";
  static constexpr const char* kInspectFeaturesName = "features";
  static constexpr const char* kInspectVersionName = "hci_version";
  static constexpr const char* kInspectManufacturerName = "manufacturer";

  // Attach peer as child node of |parent| with specified |name|.
  void AttachInspect(inspect::Node& parent, std::string name = "peer");

  enum class TokenType { kInitializing, kConnection };
  template <TokenType T>
  class TokenWithCallback {
   public:
    explicit TokenWithCallback(fit::callback<void()> on_destruction)
        : on_destruction_(std::move(on_destruction)) {}
    ~TokenWithCallback() = default;
    TokenWithCallback(TokenWithCallback&&) noexcept = default;
    TokenWithCallback& operator=(TokenWithCallback&&) noexcept = default;

   private:
    fit::deferred_callback on_destruction_;
    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(TokenWithCallback);
  };

  // InitializingConnectionToken is meant to be held by a connection request object. When the
  // request object is destroyed, the specified callback will be called to update the connection
  // state.
  using InitializingConnectionToken = TokenWithCallback<TokenType::kInitializing>;

  // ConnectionToken is meant to be held by a connection object. When the connection object is
  // destroyed, the specified callback will be called to update the connection state.
  using ConnectionToken = TokenWithCallback<TokenType::kConnection>;

  // Contains Peer data that apply only to the LE transport.
  class LowEnergyData final {
   public:
    static constexpr const char* kInspectNodeName = "le_data";
    static constexpr const char* kInspectConnectionStateName = "connection_state";
    static constexpr const char* kInspectAdvertisingDataParseFailureCountName =
        "adv_data_parse_failure_count";
    static constexpr const char* kInspectLastAdvertisingDataParseFailureName =
        "last_adv_data_parse_failure";
    static constexpr const char* kInspectBondDataName = "bonded";
    static constexpr const char* kInspectFeaturesName = "features";

    explicit LowEnergyData(Peer* owner);

    void AttachInspect(inspect::Node& parent, std::string name = kInspectNodeName);

    // Current connection state.
    ConnectionState connection_state() const {
      return connected()      ? ConnectionState::kConnected
             : initializing() ? ConnectionState::kInitializing
                              : ConnectionState::kNotConnected;
    }
    bool connected() const { return connection_tokens_count_ > 0; }
    bool initializing() const { return !connected() && initializing_tokens_count_ > 0; }

    bool bonded() const { return bond_data_->has_value(); }
    bool should_auto_connect() const {
      return bonded() && auto_conn_behavior_ == AutoConnectBehavior::kAlways;
    }

    // Advertising (and optionally scan response) data obtained during
    // discovery.
    const ByteBuffer& advertising_data() const { return adv_data_buffer_; }

    // Note that it is possible for `advertising_data()` to return a non-empty buffer while this
    // method returns std::nullopt, as AdvertisingData is only stored if it is parsed correctly.
    // TODO(fxbug.dev/85368): Migrate clients off of advertising_data, so that we do not need to
    // store the raw buffer after parsing it.
    const std::optional<AdvertisingData>& parsed_advertising_data() const {
      return parsed_adv_data_;
    }
    // Returns the timestamp associated with the most recently successfully parsed AdvertisingData.
    std::optional<zx::time> parsed_advertising_data_timestamp() const { return adv_timestamp_; }

    // Most recently used LE connection parameters. Has no value if the peer
    // has never been connected.
    const std::optional<hci_spec::LEConnectionParameters>& connection_parameters() const {
      return conn_params_;
    }

    // Preferred LE connection parameters as reported by the peer.
    const std::optional<hci_spec::LEPreferredConnectionParameters>&
    preferred_connection_parameters() const {
      return preferred_conn_params_;
    }

    // This peer's LE bond data, if bonded.
    const std::optional<sm::PairingData>& bond_data() const { return *bond_data_; }

    // Bit mask of LE features (Core Spec v5.2, Vol 6, Part B, Section 4.6).
    std::optional<hci_spec::LESupportedFeatures> features() const { return *features_; }

    // Setters:

    // Overwrites the stored advertising and scan response data with the contents of |data|
    // and updates the known RSSI and timestamp with the given values.
    void SetAdvertisingData(int8_t rssi, const ByteBuffer& data, zx::time timestamp);

    // Register a connection that is in the request/initializing state. A token is returned that
    // should be owned until the initialization is complete or canceled. The connection state may be
    // updated and listeners may be notified. Multiple initializating connections may be registered.
    [[nodiscard]] InitializingConnectionToken RegisterInitializingConnection();

    // Register a connection that is in the connected state. A token is returned that should be
    // owned until the connection is disconnected. The connection state may be updated and listeners
    // may be notified. Multiple connections may be registered.
    [[nodiscard]] ConnectionToken RegisterConnection();

    // Modify the current or preferred connection parameters.
    // The device must be connectable.
    void SetConnectionParameters(const hci_spec::LEConnectionParameters& value);
    void SetPreferredConnectionParameters(const hci_spec::LEPreferredConnectionParameters& value);

    // Stores LE bonding data and makes this "bonded."
    // Marks as non-temporary if necessary.
    void SetBondData(const sm::PairingData& bond_data);

    // Removes any stored keys. Does not make the peer temporary, even if it
    // is disconnected. Does not notify listeners.
    void ClearBondData();

    void SetFeatures(hci_spec::LESupportedFeatures features) { features_.Set(features); }

    // Get pieces of the GATT database that must be persisted for bonded peers.
    const gatt::ServiceChangedCCCPersistedData& get_service_changed_gatt_data() const {
      return service_changed_gatt_data_;
    }

    // Set pieces of the GATT database that must be persisted for bonded peers.
    void set_service_changed_gatt_data(const gatt::ServiceChangedCCCPersistedData& gatt_data) {
      service_changed_gatt_data_ = gatt_data;
    }

    void set_auto_connect_behavior(AutoConnectBehavior behavior) { auto_conn_behavior_ = behavior; }

    // TODO(armansito): Store most recently seen random address and identity
    // address separately, once PeerCache can index peers by multiple
    // addresses.

   private:
    struct InspectProperties {
      inspect::StringProperty connection_state;
      inspect::StringProperty last_adv_data_parse_failure;
    };

    // Called when the connection state changes.
    void OnConnectionStateMaybeChanged(ConnectionState previous);

    Peer* peer_;  // weak

    inspect::Node node_;
    InspectProperties inspect_properties_;

    uint16_t initializing_tokens_count_ = 0;
    uint16_t connection_tokens_count_ = 0;
    std::optional<hci_spec::LEConnectionParameters> conn_params_;
    std::optional<hci_spec::LEPreferredConnectionParameters> preferred_conn_params_;

    // Buffer containing advertising and scan response data appended to each other.
    // NOTE: Repeated fields in advertising and scan response data are not deduplicated, so
    // duplicate entries are possible. It is OK to assume that fields repeated in scan response
    // data supercede those in the original advertising data when processing fields in order.
    DynamicByteBuffer adv_data_buffer_;
    // Time when advertising data was last updated and successfully parsed.
    std::optional<zx::time> adv_timestamp_;
    // AdvertisingData parsed out of the adv_data_buffer_, if it is present and parses correctly.
    std::optional<AdvertisingData> parsed_adv_data_;

    BoolInspectable<std::optional<sm::PairingData>> bond_data_;

    IntInspectable<int64_t> adv_data_parse_failure_count_;

    AutoConnectBehavior auto_conn_behavior_ = AutoConnectBehavior::kAlways;

    StringInspectable<std::optional<hci_spec::LESupportedFeatures>> features_;

    // TODO(armansito): Store GATT service UUIDs.

    // Data persisted from GATT database for bonded peers.
    gatt::ServiceChangedCCCPersistedData service_changed_gatt_data_;
  };

  // Contains Peer data that apply only to the BR/EDR transport.
  class BrEdrData final {
   public:
    static constexpr const char* kInspectNodeName = "bredr_data";
    static constexpr const char* kInspectConnectionStateName = "connection_state";
    static constexpr const char* kInspectLinkKeyName = "bonded";
    static constexpr const char* kInspectServicesName = "services";

    BrEdrData(Peer* owner);

    // Attach peer inspect node as a child node of |parent|.
    void AttachInspect(inspect::Node& parent, std::string name = kInspectNodeName);

    // Current connection state.
    ConnectionState connection_state() const { return *conn_state_; }
    bool connected() const { return connection_state() == ConnectionState::kConnected; }
    bool bonded() const { return link_key_->has_value(); }

    // Returns the peer's BD_ADDR.
    const DeviceAddress& address() const { return address_; }

    // Returns the device class reported by the peer, if it is known.
    const std::optional<DeviceClass>& device_class() const { return device_class_; }

    // Returns the page scan repetition mode of the peer, if known.
    const std::optional<hci_spec::PageScanRepetitionMode>& page_scan_repetition_mode() const {
      return page_scan_rep_mode_;
    }

    // Returns the clock offset reported by the peer, if known and valid. The
    // clock offset will have the highest-order bit set and the rest represent
    // bits 16-2 of CLKNslave-CLK (see hci_spec::kClockOffsetFlagBit in
    // hci/hci_constants.h).
    const std::optional<uint16_t>& clock_offset() const { return clock_offset_; }
    const BufferView extended_inquiry_response() const { return eir_buffer_.view(0, eir_len_); }

    const std::optional<sm::LTK>& link_key() const { return *link_key_; }

    const std::unordered_set<UUID>& services() const { return *services_; }

    // Setters:

    // Updates the inquiry data and notifies listeners. These
    // methods expect HCI inquiry result structures as they are obtained from
    // the Bluetooth controller. Each field should be encoded in little-endian
    // byte order.
    void SetInquiryData(const hci_spec::InquiryResult& value);
    void SetInquiryData(const hci_spec::InquiryResultRSSI& value);
    void SetInquiryData(const hci_spec::ExtendedInquiryResultEventParams& value);

    // Updates the connection state and notifies listeners if necessary.
    void SetConnectionState(ConnectionState state);

    // Stores a link key resulting from Secure Simple Pairing and makes this
    // peer "bonded." Marks the peer as non-temporary if necessary. All
    // BR/EDR link keys are "long term" (reusable across sessions).
    void SetBondData(const sm::LTK& link_key);

    // Removes any stored link key. Does not make the device temporary, even if
    // it is disconnected. Does not notify listeners.
    void ClearBondData();

    // Adds a service discovered on the peer, identified by |uuid|, then notifies listeners. No-op
    // if already present.
    void AddService(UUID uuid);

    // TODO(armansito): Store BD_ADDR here, once PeerCache can index
    // devices by multiple addresses.

   private:
    // All multi-byte fields must be in little-endian byte order as they were
    // received from the controller.
    void SetInquiryData(DeviceClass device_class, uint16_t clock_offset,
                        hci_spec::PageScanRepetitionMode page_scan_rep_mode,
                        int8_t rssi = hci_spec::kRSSIInvalid,
                        const BufferView& eir_data = BufferView());

    // Updates the EIR data field and returns true if any properties changed.
    bool SetEirData(const ByteBuffer& data);

    Peer* peer_;  // weak
    inspect::Node node_;

    StringInspectable<ConnectionState> conn_state_;

    DeviceAddress address_;
    std::optional<DeviceClass> device_class_;
    std::optional<hci_spec::PageScanRepetitionMode> page_scan_rep_mode_;
    std::optional<uint16_t> clock_offset_;
    // TODO(jamuraa): Parse more of the Extended Inquiry Response fields
    size_t eir_len_;
    DynamicByteBuffer eir_buffer_;
    BoolInspectable<std::optional<sm::LTK>> link_key_;
    StringInspectable<std::unordered_set<UUID>> services_;
  };

  // Number that uniquely identifies this device with respect to the bt-host
  // that generated it.
  // TODO(armansito): Come up with a scheme that guarnatees the uniqueness of
  // this ID across all bt-hosts. Today this is guaranteed since we don't allow
  // clients to interact with multiple controllers simultaneously though this
  // could possibly lead to collisions if the active adapter gets changed
  // without clearing the previous adapter's cache.
  PeerId identifier() const { return *identifier_; }

  // The Bluetooth technologies that are supported by this device.
  TechnologyType technology() const { return *technology_; }

  // The known device address of this device. Depending on the technologies
  // supported by this device this has the following meaning:
  //
  //   * For BR/EDR devices this is the BD_ADDR.
  //
  //   * For LE devices this is identity address IF identity_known() returns
  //     true. This is always the case if the address type is LE Public.
  //
  //     For LE devices that use privacy, identity_known() will be set to false
  //     upon discovery. The address will be updated only once the identity
  //     address has been obtained during the pairing procedure.
  //
  //   * For BR/EDR/LE devices this is the BD_ADDR and the LE identity address.
  //     If a BR/EDR/LE device uses an identity address that is different from
  //     its BD_ADDR, then there will be two separate Peer entries for
  //     it.
  const DeviceAddress& address() const { return *address_; }
  bool identity_known() const { return identity_known_; }

  // The LMP version of this device obtained doing discovery.
  const std::optional<hci_spec::HCIVersion>& version() const { return *lmp_version_; }

  // Returns true if this is a connectable device.
  bool connectable() const { return *connectable_; }

  // Returns true if this device is connected over BR/EDR or LE transports.
  bool connected() const {
    return (le() && le()->connected()) || (bredr() && bredr()->connected());
  }

  // Returns true if this device has been bonded over BR/EDR or LE transports.
  bool bonded() const { return (le() && le()->bonded()) || (bredr() && bredr()->bonded()); }

  // Returns the most recently observed RSSI for this peer. Returns
  // hci_spec::kRSSIInvalid if the value is unknown.
  int8_t rssi() const { return rssi_; }

  // Gets the user-friendly name of the device, if it's known. This can be
  // assigned based on LE advertising data, BR/EDR inquiry data, or by directly
  // calling the SetName() method.
  const std::optional<std::string>& name() const { return name_; }

  // Returns the set of features of this device.
  const hci_spec::LMPFeatureSet& features() const { return *lmp_features_; }

  // A temporary device gets removed from the PeerCache after a period
  // of inactivity (see the |update_expiry_callback| argument to the
  // constructor). The following rules determine the temporary state of a
  // device:
  //   a. A device is temporary by default.
  //   b. A device becomes non-temporary when it gets connected.
  //   c. A device becomes temporary again when disconnected only if its
  //      identity is not known (i.e. identity_known() returns false). This only
  //      applies to LE devices that use the privacy feature.
  //
  // Temporary devices are never bonded.
  bool temporary() const { return *temporary_; }

  // Returns the LE transport specific data of this device, if any. This will be
  // present if information about this device is obtained using the LE discovery
  // and connection procedures.
  const std::optional<LowEnergyData>& le() const { return le_data_; }

  // Returns the BR/EDR transport specific data of this device, if any. This
  // will be present if information about this device is obtained using the
  // BR/EDR discovery and connection procedures.
  const std::optional<BrEdrData>& bredr() const { return bredr_data_; }

  // Returns a mutable reference to each transport-specific data structure,
  // initializing the structure if it is unitialized. Use these to mutate
  // members of the transport-specific structs. The caller must make sure to
  // invoke these only if the device is known to support said technology.
  LowEnergyData& MutLe();
  BrEdrData& MutBrEdr();

  // Returns a string representation of this device.
  std::string ToString() const;

  // The following methods mutate Peer properties:

  // Updates the name of this device. This will override the existing name (if
  // present) and notify listeners of the change.
  void SetName(const std::string& name);

  // Sets the value of the LMP |features| for the given |page| number.
  void SetFeaturePage(size_t page, uint64_t features) {
    lmp_features_.Mutable()->SetPage(page, features);
  }

  // Sets the last available LMP feature |page| number for this device.
  void set_last_page_number(uint8_t page) { lmp_features_.Mutable()->set_last_page_number(page); }

  void set_version(hci_spec::HCIVersion version, uint16_t manufacturer, uint16_t subversion) {
    lmp_version_.Set(version);
    lmp_manufacturer_.Set(manufacturer);
    lmp_subversion_ = subversion;
  }

  // Marks this device's identity as known. Called by PeerCache when
  // initializing a bonded device and by LowEnergyData when setting bond data
  // with an identity address.
  void set_identity_known(bool value) { identity_known_ = value; }

  // Stores the BR/EDR cross-transport key generated through LE pairing. This method will not mark
  // the peer as dual-mode if it is not already dual-mode. It will also not overwrite existing
  // BR/EDR link keys of stronger security than `ct_key`.
  void StoreBrEdrCrossTransportKey(sm::LTK ct_key);

  // Update the connectable status of this peer. This is useful if the peer sends both
  // non-connectable and connectable advertisements (e.g. when it is a beacon).
  void set_connectable(bool connectable) { connectable_.Set(connectable); }

  // The time when the most recent update occurred. Updates include:
  // * LE advertising data updated
  // * LE connection state updated
  // * LE bond state updated
  // * BR/EDR connection state updated
  // * BR/EDR inquiry data updated
  // * BR/EDR bond data updated
  // * BR/EDR services updated
  // * name is updated
  zx::time last_updated() const { return last_updated_; }

  fxl::WeakPtr<Peer> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  // Assigns a new value for the address of this device. Called by LowEnergyData
  // when a new identity address is assigned.
  void set_address(const DeviceAddress& address) { address_.Set(address); }

  // Updates the RSSI and returns true if it changed.
  bool SetRssiInternal(int8_t rssi);

  // Updates the name and returns true if there was a change without notifying
  // listeners.
  // TODO(armansito): Add similarly styled internal setters so that we can batch
  // more updates.
  bool SetNameInternal(const std::string& name);

  // Marks this device as non-temporary. This operation may fail due to one of
  // the conditions described above the |temporary()| method.
  //
  // TODO(armansito): Replace this with something more sophisticated when we
  // implement bonding procedures. This method is here to remind us that these
  // conditions are subtle and not fully supported yet.
  bool TryMakeNonTemporary();

  // Marks this device as temporary. This operation may fail due to one of
  // the conditions described above the |temporary()| method.
  bool TryMakeTemporary();

  // Tells the owning PeerCache to update the expiry state of this
  // device.
  void UpdateExpiry();

  // Signal to the cache to notify listeners.
  void NotifyListeners(NotifyListenersChange change);

  // Mark this device as dual mode and signal the cache.
  void MakeDualMode();

  // Updates the peer last updated timestamp.
  void OnPeerUpdate();

  // Updates the peer last updated timestamp an notifies listeners.
  void UpdatePeerAndNotifyListeners(NotifyListenersChange change);

  inspect::Node node_;

  // Callbacks used to notify state changes.
  NotifyListenersCallback notify_listeners_callback_;
  PeerCallback update_expiry_callback_;
  PeerCallback dual_mode_callback_;

  StringInspectable<PeerId> identifier_;
  StringInspectable<TechnologyType> technology_;

  StringInspectable<DeviceAddress> address_;
  bool identity_known_;

  std::optional<std::string> name_;
  StringInspectable<std::optional<hci_spec::HCIVersion>> lmp_version_;
  StringInspectable<std::optional<uint16_t>> lmp_manufacturer_;
  std::optional<uint16_t> lmp_subversion_;
  StringInspectable<hci_spec::LMPFeatureSet> lmp_features_;
  BoolInspectable<bool> connectable_;
  BoolInspectable<bool> temporary_;
  int8_t rssi_;

  // The spec does not explicitly prohibit LE->BREDR cross-transport key generation for LE-only
  // peers. This is used to store a CT-generated BR/EDR key for LE-only peers to avoid incorrectly
  // marking a peer as dual-mode.
  std::optional<sm::LTK> bredr_cross_transport_key_;

  // Data that only applies to the LE transport. This is present if this device
  // is known to support LE.
  std::optional<LowEnergyData> le_data_;

  // Data that only applies to the BR/EDR transport. This is present if this
  // device is known to support BR/EDR.
  std::optional<BrEdrData> bredr_data_;

  // Metrics counters used across all peer objects. Weak reference.
  PeerMetrics* peer_metrics_;

  // The time when the most recent update occurred.
  zx::time last_updated_;

  fxl::WeakPtrFactory<Peer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Peer);
};

}  // namespace bt::gap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_PEER_H_
