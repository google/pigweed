// Copyright 2025 The Pigweed Authors
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

#include "pw_i2c/initiator.h"

#include "pw_bytes/span.h"
#include "pw_chrono/system_clock.h"
#include "pw_containers/vector.h"
#include "pw_i2c/address.h"
#include "pw_i2c/message.h"
#include "pw_status/status.h"

namespace pw::i2c {

// This method is now a non-preferred API. Implement DoTransferFor(Messages)
// instead.
//
// For legacy implementors, WriteReadFor() will be overridden and perform the
// bus transaction directly.
//
// For new initiators that implement DoTransferFor(Messages), the default code
// implementation immediately below will forward to TransferFor(Messages).
Status Initiator::WriteReadFor(Address device_address,
                               ConstByteSpan tx_buffer,
                               ByteSpan rx_buffer,
                               chrono::SystemClock::duration timeout) {
  Vector<Message, 2> messages;
  if (!tx_buffer.empty()) {
    messages.push_back(Message::WriteMessage(device_address, tx_buffer));
  }
  if (!rx_buffer.empty()) {
    messages.push_back(Message::ReadMessage(device_address, rx_buffer));
  }

  return TransferFor(messages, timeout);
}

// This method should be overridden by implementations of Initiator.
// All messages in one call to DoTransferFor() should be executed
// as one transaction.
//
// For legacy initiators that only implement WriteReadFor(), the default
// implementation immediately below will forward any new-style Message API
// calls to WriteReadFor().
Status Initiator::DoTransferFor(span<const Message> messages,
                                chrono::SystemClock::duration timeout) {
  // When a driver doesn't yet implement this new Message API, we will
  // attempt to use the old, more restrictive API. This function should
  // only be called by older Initiator's that implemented/overrode the
  // other DoWriteReadFor() and not this one.

  if (messages.size() == 2) {
    // In the context of compatibiltiy with the old API, if there are two
    // messages:
    // a) both must have the same address
    // b) the first must be a write
    // c) the second must be a read.
    if (!(messages[0].GetAddress() == messages[1].GetAddress()) ||
        messages[0].IsRead() || !messages[1].IsRead()) {
      // This case could be hit if a new client accesses an older
      // Initiator that does not yet implement the Message interface,
      return Status::Unimplemented();
    }
    return DoWriteReadFor(messages[0].GetAddress(),
                          messages[0].GetData(),
                          messages[1].GetData(),
                          timeout);
  }

  if (messages.size() == 1) {
    if (messages[0].IsRead()) {
      return DoWriteReadFor(
          messages[0].GetAddress(), {}, messages[0].GetData(), timeout);
    } else {
      return DoWriteReadFor(
          messages[0].GetAddress(), messages[0].GetData(), {}, timeout);
    }
  }

  // Can't emulate call correctly this with the old API.
  // This case could be hit if a new client accesses an older
  // Initiator that does not yet implement the Message interface.
  return Status::Unimplemented();
}

}  // namespace pw::i2c
