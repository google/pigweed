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

#include "pw_bluetooth_proxy/internal/l2cap_write_channel.h"

namespace pw::bluetooth::proxy {

/// `GattNotifyChannel` supports sending GATT characteristic notifications to a
/// remote peer.
class GattNotifyChannel : public L2capWriteChannel {
 public:
  /// Send a GATT Notify to the remote peer.
  ///
  /// @param[in] attribute_value The data to be sent. Data will be copied
  ///                            before function completes.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///  OK: If notify was successfully queued for send.
  ///  UNAVAILABLE: If channel could not acquire the resources to queue the send
  ///               at this time (transient error).
  ///  INVALID_ARGUMENT: If `attribute_value` is too large.
  /// @endrst
  pw::Status Write(pw::span<const uint8_t> attribute_value);

 protected:
  static pw::Result<GattNotifyChannel> Create(AclDataChannel& acl_data_channel,
                                              H4Storage& h4_storage,
                                              uint16_t connection_handle,
                                              uint16_t attribute_handle);

 private:
  // TODO: https://pwbug.dev/349602172 - Define ATT CID in pw_bluetooth.
  static constexpr uint16_t kAttributeProtocolCID = 0x0004;

  explicit GattNotifyChannel(AclDataChannel& acl_data_channel,
                             H4Storage& h4_storage,
                             uint16_t connection_handle,
                             uint16_t attribute_handle);

  uint16_t attribute_handle_;
};

}  // namespace pw::bluetooth::proxy
