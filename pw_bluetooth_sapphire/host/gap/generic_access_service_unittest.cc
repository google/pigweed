// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/generic_access_service.h"

#include "gtest/gtest.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer.h"

namespace bt {
namespace gap {
namespace {

using PreferredConnectionParameters = hci::LEPreferredConnectionParameters;

TEST(GAP_GenericAccessServiceTest, DeviceName) {
  GenericAccessService svc(gatt::testing::FakeLayer::Create());
  EXPECT_EQ(svc.GetDeviceName(), "fuchsia");

  // Set a new device name
  svc.UpdateDeviceName("NewName");
  EXPECT_EQ(svc.GetDeviceName(), "NewName");

  // Set a device name that is too long (can't be longer than 248 octects
  // as per Core v5.1, Vol 3, Part C, 12.1)
  std::string long_name =
      "PVYfiDmbwene1VN3HFvWOYQGLxGVqOkg6isyMAjZT3JW2hlY73cO5kE747OTHtIVacQ3x2ZF"
      "lgb4WK978KGY44sbuCdXWMzDJLWRmGl0oOzdHZo3b2KLEty2tyKlfWuKAJZ1IAG9T576jG9Q"
      "8vyyIvXY417WuGml9r8ls0RcbpAw5P1kpViFyDmR3auM30BmLs6KVkP91BMFL6hiTlWhR4GP"
      "ZO10RfxwoZAZ0Epfi5JVGFIfqhWp9E5eL7T6lSh";
  std::string expected_used_name =
      "PVYfiDmbwene1VN3HFvWOYQGLxGVqOkg6isyMAjZT3JW2hlY73cO5kE747OTHtIVacQ3x2ZF"
      "lgb4WK978KGY44sbuCdXWMzDJLWRmGl0oOzdHZo3b2KLEty2tyKlfWuKAJZ1IAG9T576jG9Q"
      "8vyyIvXY417WuGml9r8ls0RcbpAw5P1kpViFyDmR3auM30BmLs6KVkP91BMFL6hiTlWhR4GP"
      "ZO10RfxwoZAZ0Epfi5JVGFIfqhWp9E5e";
  svc.UpdateDeviceName(std::move(long_name));
  EXPECT_EQ(svc.GetDeviceName(), expected_used_name);
}

TEST(GAP_GenericAccessServiceTest, Appearance) {
  GenericAccessService svc(gatt::testing::FakeLayer::Create());
  EXPECT_EQ(svc.GetAppearance(), AppearanceCategory::kUnknown);

  // Set a new device name
  svc.UpdateAppearance(AppearanceCategory::kGenericPhone);
  EXPECT_EQ(svc.GetAppearance(), AppearanceCategory::kGenericPhone);
}

TEST(GAP_GenericAccessServiceTest, PreferredConnectionParameters) {
  GenericAccessService svc(gatt::testing::FakeLayer::Create());
  EXPECT_FALSE(svc.GetPreferredConnectionParameters());

  // Update PreferredConnectionParameters.
  PreferredConnectionParameters parameters =
      PreferredConnectionParameters(0x00AA, 0x00AB, 0x00AA, 0x00AA);
  EXPECT_TRUE(svc.UpdatePreferredConnectionParameters(parameters));
  EXPECT_EQ(svc.GetPreferredConnectionParameters(), parameters);

  EXPECT_TRUE(svc.UpdatePreferredConnectionParameters(std::nullopt));
  EXPECT_FALSE(svc.GetPreferredConnectionParameters());

  // Test bounds of the ranges for each parameter.
  EXPECT_TRUE(svc.UpdatePreferredConnectionParameters(
      PreferredConnectionParameters(0x0006, 0x0006, 0x0000, 0x000A)));
  // Keeping this for later.
  parameters = PreferredConnectionParameters(0x0C80, 0x0C80, 0x01F3, 0x0C80);
  EXPECT_TRUE(svc.UpdatePreferredConnectionParameters(parameters));

  // Test out of bound values (changing only one at a time).
  EXPECT_FALSE(svc.UpdatePreferredConnectionParameters(
      PreferredConnectionParameters(0x0005, 0x0006, 0x0000, 0x000A)));
  EXPECT_FALSE(svc.UpdatePreferredConnectionParameters(
      PreferredConnectionParameters(0x0C81, 0x0006, 0x0000, 0x000A)));

  EXPECT_FALSE(svc.UpdatePreferredConnectionParameters(
      PreferredConnectionParameters(0x0006, 0x0005, 0x0000, 0x000A)));
  EXPECT_FALSE(svc.UpdatePreferredConnectionParameters(
      PreferredConnectionParameters(0x0006, 0x0C81, 0x0000, 0x000A)));

  EXPECT_FALSE(svc.UpdatePreferredConnectionParameters(
      PreferredConnectionParameters(0x0006, 0x0006, 0x01F4, 0x000A)));

  EXPECT_FALSE(svc.UpdatePreferredConnectionParameters(
      PreferredConnectionParameters(0x0006, 0x0006, 0x0000, 0x0009)));
  EXPECT_FALSE(svc.UpdatePreferredConnectionParameters(
      PreferredConnectionParameters(0x0006, 0x0006, 0x0000, 0x0C81)));

  // Make sure maximum connection interval cannot be less than minimum
  // connection interval.
  EXPECT_FALSE(svc.UpdatePreferredConnectionParameters(
      PreferredConnectionParameters(0x000B, 0x000A, 0x0000, 0x000A)));

  // Make sure no updates happened while
  // |UpdatePreferredConnectionParameters| returned false and
  // parameters are still the values set when
  // |UpdatePreferredConnectionParameters| returned true last.
  EXPECT_EQ(svc.GetPreferredConnectionParameters(), parameters);
}

TEST(GAP_GenericAccessServiceTest, OnReadValue) {
  GenericAccessService svc(gatt::testing::FakeLayer::Create());

  // Read default name
  svc.OnReadValue(GenericAccessService::kDisplayNameId, 0,
                  [](att::ErrorCode status, const ByteBuffer& buffer) {
                    EXPECT_EQ(status, att::ErrorCode::kNoError);
                    EXPECT_EQ("fuchsia",
                              std::string((char*)buffer.data(), buffer.size()));
                  });

  // Read default appearance
  svc.OnReadValue(
      GenericAccessService::kAppearanceId, 0,
      [](att::ErrorCode status, const ByteBuffer& buffer) {
        EXPECT_EQ(status, att::ErrorCode::kNoError);
        uint16_t val = buffer.data()[0] + (buffer.data()[1] << 8);
        EXPECT_EQ(val, static_cast<uint16_t>(AppearanceCategory::kUnknown));
      });

  // Attempt connection parameters read without setting any (default)
  svc.OnReadValue(
      GenericAccessService::kPeripheralPreferredConnectionParametersId, 0,
      [](att::ErrorCode status, const ByteBuffer& buffer) {
        EXPECT_EQ(status, att::ErrorCode::kReadNotPermitted);
      });

  // Update name and read
  std::string new_name("newname");
  svc.UpdateDeviceName(new_name);
  svc.OnReadValue(GenericAccessService::kDisplayNameId, 0,
                  [new_name = std::move(new_name)](att::ErrorCode status,
                                                   const ByteBuffer& buffer) {
                    EXPECT_EQ(status, att::ErrorCode::kNoError);
                    EXPECT_EQ(new_name,
                              std::string((char*)buffer.data(), buffer.size()));
                  });

  // Update name to a really long name and read
  //
  // Set a device name that is too long (can't be longer than 248 octects
  // as per Core v5.1, Vol 3, Part C, 12.1)
  std::string long_name =
      "PVYfiDmbwene1VN3HFvWOYQGLxGVqOkg6isyMAjZT3JW2hlY73cO5kE747OTHtIVacQ3x2ZF"
      "lgb4WK978KGY44sbuCdXWMzDJLWRmGl0oOzdHZo3b2KLEty2tyKlfWuKAJZ1IAG9T576jG9Q"
      "8vyyIvXY417WuGml9r8ls0RcbpAw5P1kpViFyDmR3auM30BmLs6KVkP91BMFL6hiTlWhR4GP"
      "ZO10RfxwoZAZ0Epfi5JVGFIfqhWp9E5eL7T6lSh";
  std::string expected_used_name =
      "PVYfiDmbwene1VN3HFvWOYQGLxGVqOkg6isyMAjZT3JW2hlY73cO5kE747OTHtIVacQ3x2ZF"
      "lgb4WK978KGY44sbuCdXWMzDJLWRmGl0oOzdHZo3b2KLEty2tyKlfWuKAJZ1IAG9T576jG9Q"
      "8vyyIvXY417WuGml9r8ls0RcbpAw5P1kpViFyDmR3auM30BmLs6KVkP91BMFL6hiTlWhR4GP"
      "ZO10RfxwoZAZ0Epfi5JVGFIfqhWp9E5e";
  svc.UpdateDeviceName(std::move(long_name));
  svc.OnReadValue(GenericAccessService::kDisplayNameId, 0,
                  [new_name = std::move(expected_used_name)](
                      att::ErrorCode status, const ByteBuffer& buffer) {
                    EXPECT_EQ(status, att::ErrorCode::kNoError);
                    EXPECT_EQ(new_name,
                              std::string((char*)buffer.data(), buffer.size()));
                  });

  // Update appearance and read
  AppearanceCategory new_appearance = AppearanceCategory::kGenericDisplay;
  svc.UpdateAppearance(new_appearance);
  svc.OnReadValue(
      GenericAccessService::kAppearanceId, 0,
      [new_appearance](att::ErrorCode status, const ByteBuffer& buffer) {
        EXPECT_EQ(status, att::ErrorCode::kNoError);
        EXPECT_EQ(buffer.size(), (size_t)2);
        uint16_t val = buffer.data()[0] + (buffer.data()[1] << 8);
        EXPECT_EQ(val, static_cast<uint16_t>(new_appearance));
      });

  // Update connection parameters to some value and read
  PreferredConnectionParameters parameters(0x0B72, 0x0C80, 0x01D3, 0x0A49);
  EXPECT_TRUE(svc.UpdatePreferredConnectionParameters(parameters));
  svc.OnReadValue(
      GenericAccessService::kPeripheralPreferredConnectionParametersId, 0,
      [parameters = std::move(parameters)](att::ErrorCode status,
                                           const ByteBuffer& buffer) {
        EXPECT_EQ(status, att::ErrorCode::kNoError);
        EXPECT_EQ(buffer.size(), (size_t)8);
        uint16_t min_interval = buffer.data()[0] + (buffer.data()[1] << 8);
        uint16_t max_interval = buffer.data()[2] + (buffer.data()[3] << 8);
        uint16_t max_latency = buffer.data()[4] + (buffer.data()[5] << 8);
        uint16_t supervision_timeout =
            buffer.data()[6] + (buffer.data()[7] << 8);
        EXPECT_EQ(
            PreferredConnectionParameters(min_interval, max_interval,
                                          max_latency, supervision_timeout),
            parameters);
      });

  // Update connection parameters to none value and read
  EXPECT_TRUE(svc.UpdatePreferredConnectionParameters(std::nullopt));
  svc.OnReadValue(
      GenericAccessService::kPeripheralPreferredConnectionParametersId, 0,
      [parameters = std::move(parameters)](att::ErrorCode status,
                                           const ByteBuffer& buffer) {
        EXPECT_EQ(status, att::ErrorCode::kReadNotPermitted);
      });

  // Invalid Id
  svc.OnReadValue(10, 0, [](att::ErrorCode status, const ByteBuffer& buffer) {
    EXPECT_EQ(status, att::ErrorCode::kReadNotPermitted);
  });

  // Invalid offset
  svc.OnReadValue(GenericAccessService::kDisplayNameId, 1,
                  [](att::ErrorCode status, const ByteBuffer& buffer) {
                    EXPECT_EQ(status, att::ErrorCode::kInvalidOffset);
                  });
  svc.OnReadValue(GenericAccessService::kAppearanceId, 1,
                  [](att::ErrorCode status, const ByteBuffer& buffer) {
                    EXPECT_EQ(status, att::ErrorCode::kInvalidOffset);
                  });
  svc.OnReadValue(
      GenericAccessService::kPeripheralPreferredConnectionParametersId, 1,
      [](att::ErrorCode status, const ByteBuffer& buffer) {
        EXPECT_EQ(status, att::ErrorCode::kInvalidOffset);
      });
}

}  // namespace
}  // namespace gap
}  // namespace bt
