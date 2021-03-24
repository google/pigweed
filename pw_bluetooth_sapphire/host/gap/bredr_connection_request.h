// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_REQUEST_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_REQUEST_H_

#include <lib/fit/function.h>

#include <list>

#include "fbl/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"

namespace bt::gap {

class BrEdrConnection;

// A |BrEdrConnectionRequest| represents a request for the GAP to connect to a given
// |DeviceAddress| by one or more clients. BrEdrConnectionManager
// is responsible for tracking ConnectionRequests and passing them to the
// Connector when ready.
//
// There is at most One BrEdrConnectionRequest per address at any given time; if
// multiple clients wish to connect, they each append a callback to the list in
// the ConnectionRequest for the device they are interested in.
//
// If a remote peer makes an incoming request for a connection, we track that
// here also - whether an incoming request is pending is indicated by
// HasIncoming()
class BrEdrConnectionRequest final {
 public:
  using OnComplete = fit::function<void(hci::Status, BrEdrConnection*)>;
  using RefFactory = fit::function<BrEdrConnection*()>;

  // Construct without a callback. Can be used for incoming only requests
  explicit BrEdrConnectionRequest(const DeviceAddress& addr)
      : address_(addr), has_incoming_(false) {}

  BrEdrConnectionRequest(const DeviceAddress& addr, OnComplete&& callback)
      : address_(addr), has_incoming_(false) {
    callbacks_.push_back(std::move(callback));
  }

  BrEdrConnectionRequest(BrEdrConnectionRequest&&) = default;
  BrEdrConnectionRequest& operator=(BrEdrConnectionRequest&&) = default;

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

  void BeginIncoming() { has_incoming_ = true; }
  void CompleteIncoming() { has_incoming_ = false; }
  bool HasIncoming() { return has_incoming_; }
  bool AwaitingOutgoing() { return !callbacks_.empty(); }

  DeviceAddress address() const { return address_; }

 private:
  DeviceAddress address_;
  std::list<OnComplete> callbacks_;
  bool has_incoming_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrConnectionRequest);
};

}  // namespace bt::gap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_REQUEST_H_
