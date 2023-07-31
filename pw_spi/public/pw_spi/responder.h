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

#include "pw_bytes/span.h"
#include "pw_function/function.h"
#include "pw_status/status.h"

namespace pw::spi {

// The Responder class provides an abstract interface used to receive and
// transmit data on the responder side of a SPI bus.
class Responder {
 public:
  virtual ~Responder() = default;

  // Set `callback` to be called when SPI transaction completes. `callback` can
  // be called in an interrupt context. `callback` should not be changed during
  // execution of a completion.
  //
  // A value of CANCELLED for the Status parameter indicates Abort() was called.
  // Partially transferred data may be passed in that case as well.
  // Other Status values are implementer defined.
  void SetCompletionHandler(Function<ByteSpan, Status> callback);

  // `tx_data` is queued for tx when called, but only transmitted when
  //   the initiator starts the next transaction. It's up to the implementer to
  //   define how stuffing bytes are handled.
  // `rx_data` is populated as the initiator transfers data. A slice of
  //   `rx_data` is passed as a span to the completion callback.
  //
  // Only one outstanding request should be active. UNAVAILABLE will be returned
  // if a transaction is already established.
  //
  // The completion handler will always be invoked, even in the case of an
  // Abort(). In that case a Status value of CANCELLED will be passed.
  Status WriteReadAsync(ConstByteSpan tx_data, ByteSpan rx_data);

  // Cancel the outstanding `WriteReadAsync` call. The completion handler will
  // be called with a Status of CANCELLED after this is called.
  void Abort();
};

}  // namespace pw::spi
