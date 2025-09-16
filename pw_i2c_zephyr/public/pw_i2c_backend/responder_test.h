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
#pragma once

#include <zephyr/drivers/i2c.h>

#include "pw_i2c/address.h"
#include "pw_i2c/responder_test_interface.h"
#include "pw_i2c_zephyr/responder.h"

namespace pw::i2c::backend {
constexpr Address kTestAddress = Address::SevenBit<0x20>();

inline constexpr const ::device* kTestInitiatorDevice =
    DEVICE_DT_GET(DT_NODELABEL(i2c0));
inline constexpr const ::device* kTestResponderDevice =
    DEVICE_DT_GET(DT_NODELABEL(i2c1));

extern test::ResponderEventsForTest kResponderEvents;
extern ZephyrResponder kTestResponder;

class NativeResponderTest
    : public ::pw::i2c::test::NativeResponderTestInterface {
 public:
  NativeResponderTest() = default;

  Responder& GetResponder() override { return kTestResponder; }

  Status SimulateInitiatorWrite(ConstByteSpan write_data,
                                bool send_stop = true) override {
    struct i2c_msg msg = {
        .buf = const_cast<uint8_t*>(
            reinterpret_cast<const uint8_t*>(write_data.data())),
        .len = write_data.size(),
        .flags = static_cast<uint8_t>(I2C_MSG_WRITE |
                                      (send_stop ? I2C_MSG_STOP : 0)),
    };
    int rc =
        i2c_transfer(kTestInitiatorDevice, &msg, 1, kTestAddress.GetAddress());
    return rc == 0 ? pw::OkStatus() : pw::Status::Internal();
  }

  Status SimulateInitiatorRead(ByteSpan buffer,
                               bool send_stop = true) override {
    struct i2c_msg msg = {
        .buf = reinterpret_cast<uint8_t*>(buffer.data()),
        .len = buffer.size(),
        .flags =
            static_cast<uint8_t>(I2C_MSG_READ | (send_stop ? I2C_MSG_STOP : 0)),
    };
    int rc =
        i2c_transfer(kTestInitiatorDevice, &msg, 1, kTestAddress.GetAddress());
    return rc == 0 ? pw::OkStatus() : pw::Status::Internal();
  }
};
}  // namespace pw::i2c::backend
