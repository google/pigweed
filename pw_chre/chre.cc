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

#include "pw_chre/chre.h"

#include "chre/core/event_loop.h"
#include "chre/core/event_loop_manager.h"
#include "chre/core/host_comms_manager.h"
#include "chre/core/init.h"
#include "chre/core/static_nanoapps.h"

namespace pw::chre {

void Init() {
  ::chre::init();
  ::chre::EventLoopManagerSingleton::get()->lateInit();
  ::chre::loadStaticNanoapps();
}

void Deinit() { ::chre::deinit(); }

void RunEventLoop() {
  ::chre::EventLoopManagerSingleton::get()->getEventLoop().run();
}

void StopEventLoop() {
  ::chre::EventLoopManagerSingleton::get()->getEventLoop().stop();
}

void SendMessageToNanoapp(pw::chre::NanoappMessage message) {
  ::chre::HostCommsManager& manager =
      ::chre::EventLoopManagerSingleton::get()->getHostCommsManager();
  manager.sendMessageToNanoappFromHost(message.nano_app_id,
                                       message.message_type,
                                       message.host_endpoint,
                                       message.data,
                                       message.length);
}

void FreeMessageToAp(MessageToApContext context) {
  auto& hostCommsManager =
      ::chre::EventLoopManagerSingleton::get()->getHostCommsManager();
  hostCommsManager.onMessageToHostComplete(
      static_cast<const ::chre::MessageToHost*>(context));
}

void SetEstimatedHostTimeOffset(int64_t offset) {
  ::chre::SystemTime::setEstimatedHostTimeOffset(offset);
}

}  // namespace pw::chre
