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

#include "chre/platform/host_link.h"

#include <algorithm>

#include "chre/core/host_comms_manager.h"
#include "public/pw_chre/host_link.h"
#include "pw_chre/host_link.h"

namespace chre {

// TODO: b/294106526 - Implement these, possibly by adding a facade.
void HostLink::flushMessagesSentByNanoapp(uint64_t) {}

bool HostLink::sendMessage(const MessageToHost* message) {
  pw::chre::MessageToAp pw_message{
      .nanoapp_id = message->appId,
      .message_type = message->toHostData.messageType,
      .app_permissions = message->toHostData.appPermissions,
      .message_permissions = message->toHostData.messagePermissions,
      .host_endpoint = message->toHostData.hostEndpoint,
      .woke_host = message->toHostData.wokeHost,
      .data = message->message.data(),
      .length = message->message.size(),
      .chre_context = message,
  };
  return pw::chre::SendMessageToAp(std::move(pw_message));
}

void HostLinkBase::sendNanConfiguration(bool) {}

}  // namespace chre
