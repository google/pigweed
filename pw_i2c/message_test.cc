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

#include "pw_i2c/message.h"

#include "pw_i2c/address.h"
#include "pw_unit_test/framework.h"

namespace pw::i2c {
namespace {

TEST(Message, WriteAccessors) {
  constexpr Address kAddr = Address::SevenBit<0x3a>();
  constexpr std::array<std::byte, 1> kWriteData1 = {std::byte{0x01}};
  ConstByteSpan check_span{kWriteData1};

  Message m = Message::WriteMessage(kAddr, kWriteData1);

  ConstByteSpan data = m.GetData();
  EXPECT_EQ(data.size(), check_span.size());
  EXPECT_EQ(memcmp(data.data(), check_span.data(), data.size()), 0);
}

TEST(Message, ReadAccessors) {
  constexpr Address kAddr = Address::SevenBit<0x3a>();
  std::array<std::byte, 1> read_data = {};
  ByteSpan check_span{read_data};

  Message m = Message::ReadMessage(kAddr, read_data);

  ConstByteSpan data = m.GetData();
  EXPECT_EQ(data.size(), check_span.size());
  EXPECT_EQ(memcmp(data.data(), check_span.data(), data.size()), 0);

  ByteSpan data_m = m.GetMutableData();
  EXPECT_EQ(data_m.size(), check_span.size());
  EXPECT_EQ(memcmp(data_m.data(), check_span.data(), data_m.size()), 0);

  // Verify that changing the returned span affects the original array.
  constexpr std::byte kChangeByte{0x42};
  EXPECT_NE(read_data[0], kChangeByte);
  data_m[0] = kChangeByte;
  EXPECT_EQ(read_data[0], kChangeByte);
}

}  // namespace
}  // namespace pw::i2c
