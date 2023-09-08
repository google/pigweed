// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_CLIENT_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_CLIENT_H_

#include <lib/async/cpp/task.h>
#include <lib/fit/result.h>

#include <functional>
#include <unordered_map>

#include "src/connectivity/bluetooth/core/bt-host/common/error.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/scoped_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/pdu.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/sdp.h"

namespace bt::sdp {

// The SDP client connects to the SDP server on a remote device and can perform
// search requests and returns results.  It is expected to be short-lived.
// More than one client can be connected to the same host.
class Client {
 public:
  // Create a new SDP client on the given |channel|.  |channel| must be
  // un-activated. |channel| must not be null.
  static std::unique_ptr<Client> Create(l2cap::Channel::WeakPtr channel,
                                        pw::async::Dispatcher& dispatcher);

  virtual ~Client() = default;

  // Perform a ServiceSearchAttribute transaction, searching for the UUIDs in
  // |search_pattern|, and requesting the attributes in |req_attributes|.
  // If |req_attributes| is empty, all attributes will be requested.
  // Results are returned asynchronously:
  //   - |result_cb| is called for each service which matches the pattern with
  //     the attributes requested. As long as true is returned, it can still
  //     be called.
  //   - when no more services remain, the result_cb status will be
  //     HostError::kNotFound. The return value is ignored.
  using SearchResultFunction = fit::function<bool(
      fit::result<Error<>, std::reference_wrapper<const std::map<AttributeId, DataElement>>>)>;
  virtual void ServiceSearchAttributes(std::unordered_set<UUID> search_pattern,
                                       const std::unordered_set<AttributeId>& req_attributes,
                                       SearchResultFunction result_cb) = 0;
};

}  // namespace bt::sdp

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_CLIENT_H_
