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

#include <cstdint>

#include "pw_bluetooth/hci_events.emb.h"

namespace pw::bluetooth::proxy {

// Represents the Bluetooth ACL Data channel and tracks the Host<->Controller
// ACL data flow control.
//
// This currently only supports LE Packet-based Data Flow Control as defined in
// Core Spec v5.0, Vol 2, Part E, Section 4.1.1. Does not support sharing BR/EDR
// buffers.
class AclDataChannel {
 public:
  void ProcessReadBufferSizeCommandCompleteEvent(
      emboss::ReadBufferSizeCommandCompleteEventWriter read_buffer_event);

  // Returns the number of available LE ACL send credits for the proxy.
  // Can be zero if the controller has not yet been initialized by the host.
  uint16_t GetNumFreeLeAclPackets() const;

 private:
  // Set to true if channel has been initialized by the host.
  bool initialized_ = false;

  // The local number of HCI ACL Data packets that we have reserved for this
  // proxy host to use.
  // TODO: https://pwbug.dev/326499611 - Mutex once we are using for sends.
  uint16_t proxy_max_le_acl_packets_ = 0;
};

}  // namespace pw::bluetooth::proxy
