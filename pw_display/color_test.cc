// Copyright 2024 The Pigweed Authors
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

#include "pw_display/color.h"

#include "gtest/gtest.h"
#include "pw_display/colors_endesga64.h"
#include "pw_display/colors_pico8.h"

namespace pw::display {
namespace {

TEST(ColorsPico8Rgb565, EncodedRgb565) {
  EXPECT_EQ(colors::rgb565::pico8::kDarkBlue, static_cast<ColorRgb565>(0x194a));
}

TEST(ColorsEndesga64Rgb565, EncodedRgb565) {
  EXPECT_EQ(colors::rgb565::e64::kBlood, static_cast<ColorRgb565>(0xf808));
  EXPECT_EQ(colors::rgb565::e64::kRed0, static_cast<ColorRgb565>(0x50e4));
}

TEST(ColorToRGB565, FromRGB) {
  EXPECT_EQ(EncodeRgb565(0x1d, 0x2b, 0x53), colors::rgb565::pico8::kDarkBlue);
  // Check each channel
  EXPECT_EQ(EncodeRgb565(0xff, 0x00, 0x00),
            static_cast<ColorRgb565>(0b1111100000000000));
  EXPECT_EQ(EncodeRgb565(0x00, 0xff, 0x00),
            static_cast<ColorRgb565>(0b0000011111100000));
  EXPECT_EQ(EncodeRgb565(0x00, 0x00, 0xff),
            static_cast<ColorRgb565>(0b0000000000011111));
}

TEST(ColorToRGBA8888, FromRGB) {
  EXPECT_EQ(EncodeRgba8888(0xff, 0x00, 0x00, 0x00),
            static_cast<ColorRgba8888>(0b00000000000000000000000011111111));
  EXPECT_EQ(EncodeRgba8888(0x00, 0xff, 0x00, 0x00),
            static_cast<ColorRgba8888>(0b00000000000000001111111100000000));
  EXPECT_EQ(EncodeRgba8888(0x00, 0x00, 0xff, 0x00),
            static_cast<ColorRgba8888>(0b00000000111111110000000000000000));
  EXPECT_EQ(EncodeRgba8888(0x00, 0x00, 0x00, 0xff),
            static_cast<ColorRgba8888>(0b11111111000000000000000000000000));
}

TEST(ColorToRGB565, FromRGBA8888) {
  EXPECT_EQ(EncodeRgb565(static_cast<ColorRgba8888>(0xff43143b)),
            colors::rgb565::e64::kPurple0);
}

}  // namespace
}  // namespace pw::display
