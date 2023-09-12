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

#include <cstddef>
#include <cstdint>

#include "pw_chre/host_link.h"

namespace pw::chre {

/// A message to be sent to a CHRE nanoapp.
/// This message originated from the Application Processor (AP).
struct NanoappMessage {
  /// The id of the nanoapp this message is sent to.
  uint64_t nano_app_id;
  /// The type of message this is.
  uint32_t message_type;
  /// The id of the host on the AP that sent this request.
  uint16_t host_endpoint;
  /// The actual message data.
  const uint8_t* data;
  /// The size in bytes of the message data.
  size_t length;
};

/// Initialize the CHRE environment and load any static nanoapps that exist.
/// This must be called before the event loop has been started.
void Init();

/// Teardown the CHRE environment.
/// This must be called after Init and after the event loop has been stopped.
void Deinit();

/// Run the CHRE event loop.
/// This function will not return until `StopEventLoop` is called.
void RunEventLoop();

/// Stop the CHRE event loop.
/// This can be called from any thread.
void StopEventLoop();

/// Send a message to a nano app.
/// This can be called from any thread.
/// @param[in] message The message being send to the nano app.
void SendMessageToNanoapp(NanoappMessage message);

/// Free a message that CHRE created to send to the AP (via `SendMessageToAp`).
/// This function must be called after the message is finishd being used.
/// After this function is called, the message data must not be accessed.
/// This can be called from any thread.
/// @param[in] context The message being freed.
void FreeMessageToAp(MessageToApContext context);

/// Set the estimated offset between the AP time and CHRE's time.
/// @param[in] offset The offset time in nanoseconds.
void SetEstimatedHostTimeOffset(int64_t offset);

}  // namespace pw::chre
