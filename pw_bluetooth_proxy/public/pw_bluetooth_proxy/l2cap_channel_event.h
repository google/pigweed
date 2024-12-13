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

namespace pw::bluetooth::proxy {

enum class L2capChannelEvent {
  /// The channel was closed by something other than `ProxyHost`. The channel is
  /// now `State::kClosed` and should be cleaned up. See logs for details.
  // TODO: https://pwbug.dev/360929142 - Listen for AclConnection closures &
  // L2CAP_DISCONNECTION_REQ/RSP packets and report this event accordingly.
  kChannelClosedByOther,
  /// An invalid packet was received. The channel is now `State::kStopped` and
  /// should be closed. See error logs for details.
  kRxInvalid,
  /// During Rx, the channel ran out of memory. The channel is now
  /// `State::kStopped` and should be closed.
  kRxOutOfMemory,
  /// The channel has received a packet while in the `State::kStopped` state.
  /// The channel should have been closed.
  kRxWhileStopped,
  /// `ProxyHost` has been reset. As a result, the channel is now
  /// `State::kStopped` and should be closed. (All channels are
  /// `State::kStopped` on a reset.)
  kReset,
  /// PDU recombination is not yet supported, but a fragmented L2CAP frame has
  /// been received. The channel is now `State::kStopped` and should be closed.
  // TODO: https://pwbug.dev/365179076 - Support recombination.
  kRxFragmented,
};

}  // namespace pw::bluetooth::proxy
