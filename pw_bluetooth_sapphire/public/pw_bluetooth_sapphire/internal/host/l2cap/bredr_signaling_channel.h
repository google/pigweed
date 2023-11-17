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

#pragma once
#include <unordered_map>

#include "pw_bluetooth_sapphire/internal/host/l2cap/signaling_channel.h"

namespace bt::l2cap::internal {

// Implements packet processing for the BR/EDR signaling channel (CID = 1).
// Callbacks will be run on the thread where packet reception occurs, which is
// the L2CAP thread in production.
class BrEdrSignalingChannel final : public SignalingChannel {
 public:
  BrEdrSignalingChannel(Channel::WeakPtr chan,
                        pw::bluetooth::emboss::ConnectionRole role,
                        pw::async::Dispatcher& dispatcher);
  ~BrEdrSignalingChannel() override = default;

  // Test the link using an Echo Request command that can have an arbitrary
  // payload. The callback will be invoked with the remote's Echo Response
  // payload (if any) on the L2CAP thread, or with an empty buffer if the
  // remote responded with a rejection. Returns false if the request failed to
  // send.
  //
  // This is implemented as v5.0 Vol 3, Part A Section 4.8: "These requests may
  // be used for testing the link or for passing vendor specific information
  // using the optional data field."
  bool TestLink(const ByteBuffer& data, DataCallback cb);

 private:
  // SignalingChannel overrides
  void DecodeRxUnit(ByteBufferPtr sdu,
                    const SignalingPacketHandler& cb) override;
  bool IsSupportedResponse(CommandCode code) const override;
};

}  // namespace bt::l2cap::internal
