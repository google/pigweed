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

#include <array>
#include <cstddef>
#include <memory>
#include <utility>

#include "pw_function/function.h"
#include "pw_i2c/i2c.pwpb.h"
#include "pw_i2c/i2c.rpc.pwpb.h"
#include "pw_i2c/initiator.h"
#include "pw_rpc/pwpb/server_reader_writer.h"

namespace pw::i2c {

/// RPC service for performing I2C transactions.
class I2cService final : public pw_rpc::pwpb::I2c::Service<I2cService> {
 public:
  /// A callback that returns a `pw::i2c::Initiator` instance for the given
  /// bus index position or ``nullptr`` if the position is not valid for this
  /// I2C device.
  using InitiatorSelector = pw::Function<Initiator*(size_t pos)>;

  /// Creates an I2cService instance.
  explicit I2cService(InitiatorSelector&& initiator_selector)
      : initiator_selector_(std::move(initiator_selector)) {}

  /// Writes a message to the specified I2C device register.
  void I2cWrite(
      const pwpb::I2cWriteRequest::Message& request,
      pw::rpc::PwpbUnaryResponder<pwpb::I2cWriteResponse::Message>& responder);

  /// Reads a message from the specified I2C device register.
  void I2cRead(
      const pwpb::I2cReadRequest::Message& request,
      pw::rpc::PwpbUnaryResponder<pwpb::I2cReadResponse::Message>& responder);

 private:
  InitiatorSelector initiator_selector_;
};

}  // namespace pw::i2c
