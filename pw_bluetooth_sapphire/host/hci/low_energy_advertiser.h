// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LOW_ENERGY_ADVERTISER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LOW_ENERGY_ADVERTISER_H_

#include <memory>

#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/local_address_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/sequential_command_runner.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/error.h"

namespace bt {
class AdvertisingData;

namespace hci {
class Transport;

class AdvertisingIntervalRange final {
 public:
  // Constructs an advertising interval range, capping the values based on the allowed range (Vol 2,
  // Part E, 7.8.5).
  constexpr AdvertisingIntervalRange(uint16_t min, uint16_t max)
      : min_(std::max(min, hci_spec::kLEAdvertisingIntervalMin)),
        max_(std::min(max, hci_spec::kLEAdvertisingIntervalMax)) {
    BT_ASSERT(min < max);
  }

  uint16_t min() const { return min_; }
  uint16_t max() const { return max_; }

 private:
  uint16_t min_, max_;
};

class LowEnergyAdvertiser : public LocalAddressClient {
 public:
  explicit LowEnergyAdvertiser(hci::Transport::WeakPtr hci);
  ~LowEnergyAdvertiser() override = default;

  // Get the current limit in bytes of the advertisement data supported.
  virtual size_t GetSizeLimit() const = 0;

  // TODO(armansito): The |address| parameter of this function doesn't always correspond to the
  // advertised device address as the local address for an advertisement cannot always be configured
  // by the advertiser. This is the case especially in the following conditions:
  //
  //   1. The type of |address| is "LE Public". The advertised address always corresponds to the
  //      controller's BD_ADDR. This is the case in both legacy and extended advertising.
  //
  //   2. The type of |address| is "LE Random" and the advertiser implements legacy advertising.
  //      Since the controller local address is shared between scan, initiation, and advertising
  //      procedures, the advertiser cannot configure this address without interfering with the
  //      state of other ongoing procedures.
  //
  // We should either revisit this interface or update the documentation to reflect the fact that
  // the |address| is sometimes a hint and may or may not end up being advertised. Currently the GAP
  // layer decides which address to pass to this call but the layering should be revisited when we
  // add support for extended advertising.
  //
  // -----
  //
  // Attempt to start advertising |data| with |options.flags| and scan response |scan_rsp| using
  // advertising address |address|. If |options.anonymous| is set, |address| is ignored.
  //
  // If |address| is currently advertised, the advertisement is updated.
  //
  // If |connect_callback| is provided, the advertisement will be connectable, and the provided
  // |status_callback| will be called with a connection reference when this advertisement is
  // connected to and the advertisement has been stopped.
  //
  // |options.interval| must be a value in "controller timeslices". See hci-spec/hci_constants.h
  // for the valid range.
  //
  // Provides results in |status_callback|. If advertising is setup, the final interval of
  // advertising is provided in |interval| and |status| is kSuccess. Otherwise, |status| indicates
  // the type of error and |interval| has no meaning.
  //
  // |status_callback| may be called before this function returns, but will be called before any
  // calls to |connect_callback|.
  //
  // The maximum advertising and scan response data sizes are determined by the Bluetooth controller
  // (4.x supports up to 31 bytes while 5.x is extended up to 251). If |data| and |scan_rsp| exceed
  // this internal limit, a HostError::kAdvertisingDataTooLong or HostError::kScanResponseTooLong
  // error will be generated.
  struct AdvertisingOptions {
    AdvertisingOptions(AdvertisingIntervalRange interval, bool anonymous, AdvFlags flags,
                       bool include_tx_power_level)
        : interval(interval),
          anonymous(anonymous),
          flags(flags),
          include_tx_power_level(include_tx_power_level) {}

    AdvertisingIntervalRange interval;
    bool anonymous;  // TODO(fxbug.dev/77537): anonymous advertising is currently not supported
    AdvFlags flags;
    bool include_tx_power_level;
  };
  using ConnectionCallback = fit::function<void(std::unique_ptr<hci::LowEnergyConnection> link)>;
  virtual void StartAdvertising(const DeviceAddress& address, const AdvertisingData& data,
                                const AdvertisingData& scan_rsp, AdvertisingOptions options,
                                ConnectionCallback connect_callback,
                                ResultFunction<> result_callback) = 0;

  // Stops advertisement on all currently advertising addresses. Idempotent and asynchronous.
  virtual void StopAdvertising();

  // Stops any advertisement currently active on |address|. Idempotent and asynchronous.
  virtual void StopAdvertising(const DeviceAddress& address) = 0;

  // Callback for an incoming LE connection. This function should be called in reaction to any
  // connection that was not initiated locally. This object will determine if it was a result of an
  // active advertisement and route the connection accordingly.
  virtual void OnIncomingConnection(hci_spec::ConnectionHandle handle,
                                    pw::bluetooth::emboss::ConnectionRole role,
                                    const DeviceAddress& peer_address,
                                    const hci_spec::LEConnectionParameters& conn_params) = 0;

  // Returns true if currently advertising at all
  bool IsAdvertising() const { return !connection_callbacks_.empty(); }

  // Returns true if currently advertising for the given address
  bool IsAdvertising(const DeviceAddress& address) const {
    return connection_callbacks_.count(address) != 0;
  }

  // Returns the number of advertisements currently registered
  size_t NumAdvertisements() const { return connection_callbacks_.size(); }

  // Returns the maximum number of advertisements that can be supported
  virtual size_t MaxAdvertisements() const = 0;

 protected:
  // Build the HCI command packet to enable advertising for the flavor of low energy advertising
  // being implemented.
  virtual std::optional<EmbossCommandPacket> BuildEnablePacket(
      const DeviceAddress& address, pw::bluetooth::emboss::GenericEnableParam enable) = 0;

  // Build the HCI command packet to set the advertising parameters for the flavor of low energy
  // advertising being implemented.
  virtual CommandChannel::CommandPacketVariant BuildSetAdvertisingParams(
      const DeviceAddress& address, pw::bluetooth::emboss::LEAdvertisingType type,
      pw::bluetooth::emboss::LEOwnAddressType own_address_type,
      AdvertisingIntervalRange interval) = 0;

  // Build the HCI command packet to set the advertising data for the flavor of low energy
  // advertising being implemented.
  virtual CommandChannel::CommandPacketVariant BuildSetAdvertisingData(const DeviceAddress& address,
                                                                       const AdvertisingData& data,
                                                                       AdvFlags flags) = 0;

  // Build the HCI command packet to delete the advertising parameters from the controller for the
  // flavor of low energy advertising being implemented. This method is used when stopping an
  // advertisement.
  virtual CommandChannel::CommandPacketVariant BuildUnsetAdvertisingData(
      const DeviceAddress& address) = 0;

  // Build the HCI command packet to set the data sent in a scan response (if requested) for the
  // flavor of low energy advertising being implemented.
  virtual CommandChannel::CommandPacketVariant BuildSetScanResponse(
      const DeviceAddress& address, const AdvertisingData& scan_rsp) = 0;

  // Build the HCI command packet to delete the advertising parameters from the controller for the
  // flavor of low energy advertising being implemented.
  virtual CommandChannel::CommandPacketVariant BuildUnsetScanResponse(
      const DeviceAddress& address) = 0;

  // Build the HCI command packet to remove the advertising set entirely from the controller's
  // memory for the flavor of low energy advertising being implemented.
  virtual std::optional<EmbossCommandPacket> BuildRemoveAdvertisingSet(
      const DeviceAddress& address) = 0;

  // Called when the command packet created with BuildSetAdvertisingParams returns with a result
  virtual void OnSetAdvertisingParamsComplete(const EventPacket& event) {}

  // Called when a sequence of HCI commands that form a single operation (e.g. start advertising,
  // stop advertising) completes in its entirety. Subclasses can override this method to be notified
  // when the HCI command runner is available once again.
  virtual void OnCurrentOperationComplete() {}

  // Check whether we can actually start advertising given the combination of input parameters (e.g.
  // check that the requested advertising data and scan response will actually fit within the size
  // limitations of the advertising PDUs)
  fit::result<HostError> CanStartAdvertising(const DeviceAddress& address,
                                             const AdvertisingData& data,
                                             const AdvertisingData& scan_rsp,
                                             const AdvertisingOptions& options) const;

  // Unconditionally start advertising (all checks must be performed in the methods that call this
  // one).
  void StartAdvertisingInternal(const DeviceAddress& address, const AdvertisingData& data,
                                const AdvertisingData& scan_rsp, AdvertisingIntervalRange interval,
                                AdvFlags flags, ConnectionCallback connect_callback,
                                hci::ResultFunction<> callback);

  // Unconditionally stop advertising (all checks muts be performed in the methods that call this
  // one).
  void StopAdvertisingInternal(const DeviceAddress& address);

  // Handle shared housekeeping tasks when an incoming connection is completed (e.g. clean up
  // internal state, call callbacks, etc)
  void CompleteIncomingConnection(hci_spec::ConnectionHandle handle,
                                  pw::bluetooth::emboss::ConnectionRole role,
                                  const DeviceAddress& local_address,
                                  const DeviceAddress& peer_address,
                                  const hci_spec::LEConnectionParameters& conn_params);

  SequentialCommandRunner& hci_cmd_runner() const { return *hci_cmd_runner_; }
  hci::Transport::WeakPtr hci() const { return hci_; }

  const std::unordered_map<DeviceAddress, ConnectionCallback>& connection_callbacks() const {
    return connection_callbacks_;
  }

 private:
  struct StagedParameters {
    AdvertisingData data;
    AdvertisingData scan_rsp;

    void reset() {
      AdvertisingData blank;
      blank.Copy(&data);
      blank.Copy(&scan_rsp);
    }
  };

  // Continuation function for starting advertising, called automatically via callbacks in
  // StartAdvertisingInternal. Developers should not call this function directly.
  bool StartAdvertisingInternalStep2(const DeviceAddress& address, AdvFlags flags,
                                     ConnectionCallback connect_callback,
                                     hci::ResultFunction<> status_callback);

  // Enqueue onto the HCI command runner the HCI commands necessary to stop advertising and
  // completely remove a given address from the controller's memory. If even one of the HCI commands
  // cannot be generated for some reason, no HCI commands are enqueued.
  bool EnqueueStopAdvertisingCommands(const DeviceAddress& address);

  hci::Transport::WeakPtr hci_;
  std::unique_ptr<SequentialCommandRunner> hci_cmd_runner_;
  std::unordered_map<DeviceAddress, ConnectionCallback> connection_callbacks_;
  StagedParameters staged_parameters_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyAdvertiser);
};

}  // namespace hci
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LOW_ENERGY_ADVERTISER_H_
