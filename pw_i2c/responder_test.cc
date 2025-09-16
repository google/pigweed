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

#include "pw_i2c_backend/responder_test.h"  // Provided by the backend

#include <array>
#include <vector>

#include "gtest/gtest.h"
#include "pw_bytes/byte_builder.h"
#include "pw_i2c/responder_test_interface.h"
#include "pw_i2c_backend/responder_test.h"
#include "pw_status/status.h"

namespace pw::i2c::test {
namespace {

// The NativeResponderTest class is expected to be defined in
// "pw_i2c_backend/responder_test.h" and inherit from ::testing::Test
// and pw::i2c::test::NativeResponderTestInterface.
//
// class ResponderTest : public NativeResponderTest {};
//
// However, to avoid issues with GTest's TEST_F macro needing the full
// definition of NativeResponderTest if it were a direct base class here,
// we will make ResponderTest a direct child of ::testing::Test and use
// a helper to manage the backend.
// A simpler approach for now, assuming the backend header defines
// a suitable NativeResponderTest that we can inherit from.
// The backend's NativeResponderTest will be like:
// namespace pw::i2c::backend { // Or a more specific namespace
// class NativeResponderTest : public ::testing::Test,
//                             public
//                             ::pw::i2c::test::NativeResponderTestInterface {
//  public:
//   NativeResponderTest(pw::i2c::test::ResponderCallbacksForTest& callbacks);
//   // ...
// };
// } // namespace

// If the backend provides NativeResponderTest in a specific namespace,
// adjust the using directive or the class inheritance.
using ::pw::i2c::backend::NativeResponderTest;

class ResponderTest : public NativeResponderTest {
 protected:
  ResponderTest() : NativeResponderTest() {
    // Default implementations for callbacks
    backend::kResponderEvents.SetOnStartReadCb([this] {
      on_start_read_called_ = true;
      return true;
    });
    backend::kResponderEvents.SetOnStartWriteCb([this] {
      on_start_write_called_ = true;
      return true;
    });
    backend::kResponderEvents.SetOnWriteCb([this](ConstByteSpan data) {
      on_write_called_ = true;
      std::copy(data.begin(), data.end(), std::back_inserter(received_data_));
      return true;
    });
    backend::kResponderEvents.SetOnReadCb([this]() -> Result<ConstByteSpan> {
      on_read_called_ = true;
      if (read_data_provider_) {
        return read_data_provider_();
      }
      return ByteSpan(read_buffer_);  // Return a copy or a managed span
    });
    backend::kResponderEvents.SetOnStopCb([this] {
      on_stop_called_ = true;
      return true;
    });
    GetResponder().Enable().IgnoreError();
  }
  ~ResponderTest() { GetResponder().Disable().IgnoreError(); }

  // Helper to reset call flags for each test
  void ResetTestState() {
    on_start_read_called_ = false;
    on_start_write_called_ = false;
    on_write_called_ = false;
    on_read_called_ = false;
    on_stop_called_ = false;
    received_data_.clear();
    read_buffer_.clear();
    read_data_provider_ = nullptr;
  }

  // Flags to verify callback invocation
  bool on_start_read_called_ = false;
  bool on_start_write_called_ = false;
  bool on_write_called_ = false;
  bool on_read_called_ = false;
  bool on_stop_called_ = false;

  std::vector<std::byte> received_data_;
  std::vector<std::byte> read_buffer_;  // Data to be returned by OnRead
  pw::Function<Result<ByteSpan>()> read_data_provider_;
};

TEST_F(ResponderTest, InitializationIsHandledByBackend) {
  // NativeResponderTest::SetUp() is responsible for initializing the
  // responder. If it fails, ASSERTs in SetUp() should fail the test.
  // GetResponder() should return a valid Responder instance.
  ASSERT_NE(&GetResponder(), nullptr);
  SUCCEED();  // If we reach here, SetUp didn't crash.
}

TEST_F(ResponderTest, WriteSingleByte) {
  ResetTestState();
  std::array<std::byte, 1> write_payload = {std::byte{0xAB}};

  ASSERT_EQ(OkStatus(),
            SimulateInitiatorWrite(ConstByteSpan(write_payload), true));

  EXPECT_TRUE(on_start_write_called_);
  EXPECT_TRUE(on_write_called_);
  EXPECT_EQ(received_data_.size(), 1u);
  EXPECT_EQ(received_data_[0], std::byte{0xAB});
  EXPECT_TRUE(on_stop_called_);
}

TEST_F(ResponderTest, ReadSingleByte) {
  ResetTestState();
  read_buffer_ = {std::byte{0xCD}};  // Data our mock OnRead will provide
  std::array<std::byte, 1>
      initiator_read_buffer;  // Buffer for initiator to read into

  Status read_result =
      SimulateInitiatorRead(ByteSpan(initiator_read_buffer), true);
  ASSERT_EQ(OkStatus(), read_result);

  EXPECT_TRUE(on_start_read_called_);
  EXPECT_TRUE(on_read_called_);
  EXPECT_EQ(initiator_read_buffer[0], std::byte{0xCD});
  EXPECT_TRUE(on_stop_called_);
}

TEST_F(ResponderTest, WriteMultipleBytes) {
  ResetTestState();
  std::array<std::byte, 3> write_payload = {
      std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};

  ASSERT_EQ(OkStatus(),
            SimulateInitiatorWrite(ConstByteSpan(write_payload), true));

  EXPECT_TRUE(on_start_write_called_);
  EXPECT_TRUE(on_write_called_);  // May be called multiple times by backend
  ASSERT_EQ(received_data_.size(), 3u);
  EXPECT_EQ(received_data_[0], std::byte{0x01});
  EXPECT_EQ(received_data_[1], std::byte{0x02});
  EXPECT_EQ(received_data_[2], std::byte{0x03});
  EXPECT_TRUE(on_stop_called_);
}

TEST_F(ResponderTest, ReadMultipleBytes) {
  ResetTestState();
  read_buffer_ = {std::byte{0x11},
                  std::byte{0x22},
                  std::byte{0x33}};                // Data OnRead provides
  std::array<std::byte, 3> initiator_read_buffer;  // Buffer for initiator

  Status read_result =
      SimulateInitiatorRead(ByteSpan(initiator_read_buffer), true);
  ASSERT_EQ(OkStatus(), read_result);

  EXPECT_TRUE(on_start_read_called_);
  EXPECT_TRUE(on_read_called_);  // May be called multiple times by backend
  EXPECT_EQ(initiator_read_buffer[0], std::byte{0x11});
  EXPECT_EQ(initiator_read_buffer[1], std::byte{0x22});
  EXPECT_EQ(initiator_read_buffer[2], std::byte{0x33});
  EXPECT_TRUE(on_stop_called_);
}

TEST_F(ResponderTest, OnStartWriteReturnsError) {
  ResetTestState();
  std::array<std::byte, 1> write_payload = {std::byte{0xFF}};

  backend::kResponderEvents.SetOnStartWriteCb([this] {
    on_start_write_called_ = true;
    return false;  // Simulate error
  });

  // When writing using pio (programmed input/output) the start event is
  // ACKed based on the result of the OnStart() function. But when the
  // controller is running in buffered mode, the start condition is
  // automatically ACKed by the hardware and the data will go through regardless
  // of the OnStart() result. Therefore we cannot make any assumptions about the
  // simulated write, it will fail when the bus is running in pio mode but will
  // pass in buffered mode.
  SimulateInitiatorWrite(ConstByteSpan(write_payload), true).IgnoreError();

  EXPECT_TRUE(on_start_write_called_);
  EXPECT_FALSE(on_write_called_);  // Should not be called if start fails

  // Similar to the above, we will get a stop condition in buffered mode, but
  // will never get to the stop condition when running in pio mode. This means
  // we cannot expect a stop in a generic test.
}

TEST_F(ResponderTest, OnStartReadReturnsError) {
  ResetTestState();

  backend::kResponderEvents.SetOnStartReadCb([this] {
    on_start_read_called_ = true;
    return false;  // Simulate error
  });

  std::array<std::byte, 1> initiator_read_buffer;
  Status read_result =
      SimulateInitiatorRead(ByteSpan(initiator_read_buffer), true);

  // Expect the simulation to report an error.
  // The exact error might be backend-dependent.
  EXPECT_NE(OkStatus(), read_result);
  // Depending on the backend, value() might not be valid if status is not OK.
  // If it returns a size, it should likely be 0 on error.
  // if (read_result.ok()) { EXPECT_EQ(read_result.value(), 0u); }

  EXPECT_TRUE(on_start_read_called_);
  EXPECT_FALSE(on_read_called_);
  // Whether OnStop is called can be backend-dependent if the transaction aborts
  // early. The test plan suggests OnStop is not called. Let's assume that for
  // now. If the backend *does* call OnStop, this expectation might need
  // adjustment or the backend simulation needs to be more specific.
  EXPECT_FALSE(on_stop_called_);
}

}  // namespace
}  // namespace pw::i2c::test
