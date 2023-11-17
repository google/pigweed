// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_CLIENT_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_CLIENT_H_

#include <lib/fit/result.h>

#include <functional>
#include <unordered_map>

#include "pw_bluetooth_sapphire/internal/host/common/error.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/scoped_channel.h"
#include "pw_bluetooth_sapphire/internal/host/sdp/pdu.h"
#include "pw_bluetooth_sapphire/internal/host/sdp/sdp.h"

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
      fit::result<
          Error<>,
          std::reference_wrapper<const std::map<AttributeId, DataElement>>>)>;
  virtual void ServiceSearchAttributes(
      std::unordered_set<UUID> search_pattern,
      const std::unordered_set<AttributeId>& req_attributes,
      SearchResultFunction result_cb) = 0;
};

}  // namespace bt::sdp

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_CLIENT_H_
