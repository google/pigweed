// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_CONNECTION_REQUEST_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_CONNECTION_REQUEST_H_

#include <lib/fit/function.h>

#include <list>

#include "fbl/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"

namespace bt {
namespace gap {

// A |ConnectionRequest| represents a request for the GAP to connect to a given
// |DeviceAddress| by one or more clients. The Appropriate (BrEdr|LowEnergy) mgr
// is responsible for tracking ConnectionRequests and passing them to the
// Connector when ready.
//
// There is at most One ConnectionRequest per address at any given time; if
// multiple clients wish to connect, they each append a callback to the list in
// the ConnectionRequest for the device they are interested in.
template <typename ConnectionRef>
class ConnectionRequest final {
 public:
  using OnComplete = fit::function<void(hci::Status, ConnectionRef)>;
  using RefFactory = fit::function<ConnectionRef()>;

  ConnectionRequest(const DeviceAddress& addr, OnComplete&& callback) : address_(addr) {
    callbacks_.push_back(std::move(callback));
  }

  ConnectionRequest(ConnectionRequest&&) = default;
  ConnectionRequest& operator=(ConnectionRequest&&) = default;

  void AddCallback(OnComplete cb) { callbacks_.push_back(std::move(cb)); }

  // Notifies all elements in |callbacks| with |status| and the result of
  // |generate_ref|. Called by the appropriate manager once a connection request
  // has completed, successfully or otherwise
  void NotifyCallbacks(hci::Status status, const RefFactory& generate_ref) {
    // If this request has been moved from, |callbacks_| may be empty.
    for (const auto& callback : callbacks_) {
      callback(status, generate_ref());
    }
  }

  DeviceAddress address() const { return address_; }

 private:
  DeviceAddress address_;
  std::list<OnComplete> callbacks_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ConnectionRequest);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_CONNECTION_REQUEST_H_
