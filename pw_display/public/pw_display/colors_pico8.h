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
#pragma once

#include "pw_display/color.h"

namespace pw::display::colors::rgb565::pico8 {

/// @defgroup pw_display_color_palette_pico8
/// Named colors for the Pico-8 palette.
/// @{

// clang-format off
constexpr ColorRgb565 kBlack      = EncodeRgb565(0x00, 0x00, 0x00);
constexpr ColorRgb565 kDarkBlue   = EncodeRgb565(0x1d, 0x2b, 0x53);
constexpr ColorRgb565 kDarkPurple = EncodeRgb565(0x7e, 0x25, 0x53);
constexpr ColorRgb565 kDarkGreen  = EncodeRgb565(0x00, 0x87, 0x51);
constexpr ColorRgb565 kBrown      = EncodeRgb565(0xab, 0x52, 0x36);
constexpr ColorRgb565 kDarkGray   = EncodeRgb565(0x5f, 0x57, 0x4f);
constexpr ColorRgb565 kLightGray  = EncodeRgb565(0xc2, 0xc3, 0xc7);
constexpr ColorRgb565 kWhite      = EncodeRgb565(0xff, 0xf1, 0xe8);
constexpr ColorRgb565 kRed        = EncodeRgb565(0xff, 0x00, 0x4d);
constexpr ColorRgb565 kOrange     = EncodeRgb565(0xff, 0xa3, 0x00);
constexpr ColorRgb565 kYellow     = EncodeRgb565(0xff, 0xec, 0x27);
constexpr ColorRgb565 kGreen      = EncodeRgb565(0x00, 0xe4, 0x36);
constexpr ColorRgb565 kBlue       = EncodeRgb565(0x29, 0xad, 0xff);
constexpr ColorRgb565 kIndigo     = EncodeRgb565(0x83, 0x76, 0x9c);
constexpr ColorRgb565 kPink       = EncodeRgb565(0xff, 0x77, 0xa8);
constexpr ColorRgb565 kPeach      = EncodeRgb565(0xff, 0xcc, 0xaa);
// clang-format on

/// @}

}  // namespace pw::display::colors::rgb565::pico8
