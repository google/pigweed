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

#include "pw_bluetooth_proxy/acl_data_channel.h"

#include <cstdint>

#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

template <class EventT>
void AclDataChannel::ProcessSpecificLEReadBufferSizeCommandCompleteEvent(
    EventT read_buffer_event) {
  if (initialized_) {
    PW_LOG_WARN(
        "AclDataChannel is already initialized, but encountered another "
        "ReadBufferSizeCommandCompleteEvent.");
  }

  initialized_ = true;

  uint16_t controller_max_le_acl_packets =
      read_buffer_event.total_num_le_acl_data_packets().Read();
  proxy_max_le_acl_packets_ =
      std::min(controller_max_le_acl_packets, le_acl_credits_to_reserve_);
  uint16_t host_max_le_acl_packets =
      controller_max_le_acl_packets - proxy_max_le_acl_packets_;
  read_buffer_event.total_num_le_acl_data_packets().Write(
      host_max_le_acl_packets);
  PW_LOG_INFO(
      "Bluetooth Proxy reserved %d ACL data credits. Passed %d on to host.",
      proxy_max_le_acl_packets_,
      host_max_le_acl_packets);

  if (proxy_max_le_acl_packets_ < le_acl_credits_to_reserve_) {
    PW_LOG_ERROR(
        "Only was able to reserve %d acl data credits rather than the "
        "configured %d from the controller provided's data credits of %d. ",
        proxy_max_le_acl_packets_,
        le_acl_credits_to_reserve_,
        controller_max_le_acl_packets);
  }
}

template void
AclDataChannel::ProcessSpecificLEReadBufferSizeCommandCompleteEvent<
    emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
    emboss::LEReadBufferSizeV1CommandCompleteEventWriter read_buffer_event);

template void
AclDataChannel::ProcessSpecificLEReadBufferSizeCommandCompleteEvent<
    emboss::LEReadBufferSizeV2CommandCompleteEventWriter>(
    emboss::LEReadBufferSizeV2CommandCompleteEventWriter read_buffer_event);

uint16_t AclDataChannel::GetNumFreeLeAclPackets() const {
  // TODO: https://pwbug.dev/326499611 - Subtract pending packets once we have
  // them.
  return proxy_max_le_acl_packets_;
}

}  // namespace pw::bluetooth::proxy
