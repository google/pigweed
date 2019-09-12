// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LOW_ENERGY_ADVERTISER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LOW_ENERGY_ADVERTISER_H_

#include <lib/fit/function.h>

#include <memory>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/local_address_delegate.h"

namespace bt {
namespace hci {

class AdvertisingIntervalRange final {
 public:
  // Constructs an advertising interval range, capping the values based on the allowed range
  // (Vol 2, Part E, 7.8.5).
  constexpr AdvertisingIntervalRange(uint16_t min, uint16_t max)
      : min_(std::max(min, kLEAdvertisingIntervalMin)),
        max_(std::min(max, kLEAdvertisingIntervalMax)) {
    ZX_ASSERT(min <= max);
  }

  uint16_t min() const { return min_; }
  uint16_t max() const { return max_; }

 private:
  uint16_t min_, max_;
};

class LowEnergyAdvertiser : public LocalAddressClient {
 public:
  virtual ~LowEnergyAdvertiser() = default;

  // Get the current limit in bytes of the advertisement data supported.
  virtual size_t GetSizeLimit() = 0;

  // Get the current limit of number of advertising sets supported, including
  // currently enabled ones.  This can change when the state of advertising
  // changes.  It should be checked before trying to add an advertisement.
  virtual size_t GetMaxAdvertisements() const = 0;

  // TODO(armansito): The |address| parameter of this function doesn't always
  // correspond to the advertised device address as the local address for an
  // advertisement cannot always be configured by the advertiser. This is the
  // case especially in the following conditions:
  //
  //   1. The type of |address| is "LE Public". The advertised address always
  //   corresponds to the controller's BD_ADDR. This is the case in both legacy
  //   and extended advertising.
  //
  //   2. The type of |address| is "LE Random" and the advertiser implements
  //   legacy advertising. Since the controller local address is shared between
  //   scan, initiation, and advertising procedures, the advertiser cannot
  //   configure this address without interfering with the state of other
  //   ongoing procedures.
  //
  // We should either revisit this interface or update the documentation to
  // reflect the fact that the |address| is sometimes a hint and may or may not
  // end up being advertised. Currently the GAP layer decides which address to
  // pass to this call but the layering should be revisited when we add support
  // for extended advertising.
  //
  // -----
  //
  // Attempt to start advertising |data| with scan response |scan_rsp| using
  // advertising address |address|.  If |anonymous| is set, |address| is
  // ignored.
  //
  // If |address| is currently advertised, the advertisement is updated.
  //
  // If |connect_callback| is provided, the advertisement will be connectable,
  // and the provided callback will be called with a connection reference
  // when this advertisement is connected to and the advertisement has been
  // stopped.
  //
  // |interval| must be a value in "controller timeslices". For hci/hci_constants.h for the valid
  // range.
  //
  // Provides results in |callback|. If advertising is setup, the final
  // interval of advertising is provided in |interval| and |status|
  // is kSuccess. Otherwise, |status| indicates the type of error and |interval| has no meaning.
  //
  // |callback| may be called before this function returns, but will
  // be called before any calls to |connect_callback|.
  //
  // The maxium advertising and scan response data sizes are determined by the Bluetooth controller
  // (4.x supports up to 31 bytes while 5.x is extended up to 251). If |data| and |scan_rsp| exceed
  // this internal limit, a HostError::kAdvertisingDataTooLong or HostError::kScanResponseTooLong
  // error will be generated.
  using ConnectionCallback = fit::function<void(ConnectionPtr link)>;
  virtual void StartAdvertising(const DeviceAddress& address, const ByteBuffer& data,
                                const ByteBuffer& scan_rsp, ConnectionCallback connect_callback,
                                AdvertisingIntervalRange interval, bool anonymous,
                                StatusCallback callback) = 0;

  // Stops any advertisement currently active on |address|. Idempotent and
  // asynchronous. Returns true if advertising will be stopped, false otherwise.
  virtual bool StopAdvertising(const DeviceAddress& address) = 0;

  // Callback for an incoming LE connection. This function should be called
  // in reaction to any connection that was not initiated locally. This object
  // will determine if it was a result of an active advertisement and route the
  // connection accordingly.
  // TODO(armansito): Require advertising handle.
  virtual void OnIncomingConnection(ConnectionHandle handle, Connection::Role role,
                                    const DeviceAddress& peer_address,
                                    const LEConnectionParameters& conn_params) = 0;
};

}  // namespace hci
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LOW_ENERGY_ADVERTISER_H_
