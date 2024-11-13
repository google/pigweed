// Copyright 2024 The Pigweed Authors
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

#pragma once

#include "pw_containers/intrusive_forward_list.h"
#include "pw_function/function.h"
#include "pw_span/span.h"

namespace pw::bluetooth::proxy {

// Base class for peer-to-peer L2CAP-based channels supporting reading.
//
// Read channels invoke a client-supplied receive callback for packets sent by
// the peer to the channel.
class L2capReadChannel : public IntrusiveForwardList<L2capReadChannel>::Item {
 public:
  L2capReadChannel(L2capReadChannel&& other);

  virtual ~L2capReadChannel();

  // Handle an Rx L2CAP PDU.
  //
  // Implementations should call `CallReceiveFn` after recombining/processing
  // the SDU (e.g. after updating channel state and screening out certain SDUs).
  virtual void OnPduReceived(pw::span<uint8_t> l2cap_pdu) = 0;

  // Get the source L2CAP channel ID.
  uint16_t local_cid() const { return local_cid_; }

  // Get the ACL connection handle.
  uint16_t connection_handle() const { return connection_handle_; }

 protected:
  // TODO: saeedali@ - `read_channels` will be owned by a separate channel
  // manager class in a future CL in this chain, so it won't be passed directly
  // to `L2capReadChannel`.
  explicit L2capReadChannel(
      uint16_t connection_handle,
      uint16_t local_cid,
      IntrusiveForwardList<L2capReadChannel>& read_channels,
      pw::Function<void(pw::span<uint8_t> payload)>&& receive_fn);

  // Often the useful `payload` for clients is some subspan of the Rx SDU.
  void CallReceiveFn(pw::span<uint8_t> payload) {
    if (receive_fn_) {
      receive_fn_(payload);
    }
  }

 private:
  uint16_t connection_handle_;
  uint16_t local_cid_;
  IntrusiveForwardList<L2capReadChannel>& read_channels_;
  pw::Function<void(pw::span<uint8_t> payload)> receive_fn_;
};

}  // namespace pw::bluetooth::proxy
