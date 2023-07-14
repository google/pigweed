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

// RPC service to perform I2C transactions.
class I2cService final : public pw_rpc::pwpb::I2c::Service<I2cService> {
 public:
  // Callback which returns an initiator for the given position or nullptr if
  // the position not valid on this device.
  using InitiatorSelector = pw::Function<Initiator*(size_t pos)>;

  explicit I2cService(InitiatorSelector&& initiator_selector)
      : initiator_selector_(std::move(initiator_selector)) {}

  void I2cWrite(
      const pwpb::I2cWriteRequest::Message& request,
      pw::rpc::PwpbUnaryResponder<pwpb::I2cWriteResponse::Message>& responder);
  void I2cRead(
      const pwpb::I2cReadRequest::Message& request,
      pw::rpc::PwpbUnaryResponder<pwpb::I2cReadResponse::Message>& responder);

 private:
  InitiatorSelector initiator_selector_;
};

}  // namespace pw::i2c
