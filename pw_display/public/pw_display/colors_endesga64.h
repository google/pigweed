// Copyright 2022 The Pigweed Authors
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

namespace pw::display::colors::rgb565::e64 {

/// @defgroup pw_display_color_palette_endesga64
/// Named colors for the Endesga64 palette.
/// @{

// clang-format off
constexpr ColorRgb565 kBlood         = EncodeRgb565(0xff, 0x00, 0x40);
constexpr ColorRgb565 kBlack0        = EncodeRgb565(0x13, 0x13, 0x13);
constexpr ColorRgb565 kBlack1        = EncodeRgb565(0x1b, 0x1b, 0x1b);
constexpr ColorRgb565 kGray0         = EncodeRgb565(0x27, 0x27, 0x27);
constexpr ColorRgb565 kGray1         = EncodeRgb565(0x3d, 0x3d, 0x3d);
constexpr ColorRgb565 kGray2         = EncodeRgb565(0x5d, 0x5d, 0x5d);
constexpr ColorRgb565 kGray3         = EncodeRgb565(0x85, 0x85, 0x85);
constexpr ColorRgb565 kGray4         = EncodeRgb565(0xb4, 0xb4, 0xb4);
constexpr ColorRgb565 kWhite         = EncodeRgb565(0xff, 0xff, 0xff);
constexpr ColorRgb565 kSteel6        = EncodeRgb565(0xc7, 0xcf, 0xdd);
constexpr ColorRgb565 kSteel5        = EncodeRgb565(0x92, 0xa1, 0xb9);
constexpr ColorRgb565 kSteel4        = EncodeRgb565(0x65, 0x73, 0x92);
constexpr ColorRgb565 kSteel3        = EncodeRgb565(0x42, 0x4c, 0x6e);
constexpr ColorRgb565 kSteel2        = EncodeRgb565(0x2a, 0x2f, 0x4e);
constexpr ColorRgb565 kSteel1        = EncodeRgb565(0x1a, 0x19, 0x32);
constexpr ColorRgb565 kSteel0        = EncodeRgb565(0x0e, 0x07, 0x1b);
constexpr ColorRgb565 kCoffee0       = EncodeRgb565(0x1c, 0x12, 0x1c);
constexpr ColorRgb565 kCoffee1       = EncodeRgb565(0x39, 0x1f, 0x21);
constexpr ColorRgb565 kCoffee2       = EncodeRgb565(0x5d, 0x2c, 0x28);
constexpr ColorRgb565 kCoffee3       = EncodeRgb565(0x8a, 0x48, 0x36);
constexpr ColorRgb565 kCoffee4       = EncodeRgb565(0xbf, 0x6f, 0x4a);
constexpr ColorRgb565 kCoffee5       = EncodeRgb565(0xe6, 0x9c, 0x69);
constexpr ColorRgb565 kCoffee6       = EncodeRgb565(0xf6, 0xca, 0x9f);
constexpr ColorRgb565 kCoffee7       = EncodeRgb565(0xf9, 0xe6, 0xcf);
constexpr ColorRgb565 kOrange3       = EncodeRgb565(0xed, 0xab, 0x50);
constexpr ColorRgb565 kOrange2       = EncodeRgb565(0xe0, 0x74, 0x38);
constexpr ColorRgb565 kOrange1       = EncodeRgb565(0xc6, 0x45, 0x24);
constexpr ColorRgb565 kOrange0       = EncodeRgb565(0x8e, 0x25, 0x1d);
constexpr ColorRgb565 kBrightOrange0 = EncodeRgb565(0xff, 0x50, 0x00);
constexpr ColorRgb565 kBrightOrange1 = EncodeRgb565(0xed, 0x76, 0x14);
constexpr ColorRgb565 kBrightOrange2 = EncodeRgb565(0xff, 0xa2, 0x14);
constexpr ColorRgb565 kYellow0       = EncodeRgb565(0xff, 0xc8, 0x25);
constexpr ColorRgb565 kYellow1       = EncodeRgb565(0xff, 0xeb, 0x57);
constexpr ColorRgb565 kGreen5        = EncodeRgb565(0xd3, 0xfc, 0x7e);
constexpr ColorRgb565 kGreen4        = EncodeRgb565(0x99, 0xe6, 0x5f);
constexpr ColorRgb565 kGreen3        = EncodeRgb565(0x5a, 0xc5, 0x4f);
constexpr ColorRgb565 kGreen2        = EncodeRgb565(0x33, 0x98, 0x4b);
constexpr ColorRgb565 kGreen1        = EncodeRgb565(0x1e, 0x6f, 0x50);
constexpr ColorRgb565 kGreen0        = EncodeRgb565(0x13, 0x4c, 0x4c);
constexpr ColorRgb565 kOcean0        = EncodeRgb565(0x0c, 0x2e, 0x44);
constexpr ColorRgb565 kOcean1        = EncodeRgb565(0x00, 0x39, 0x6d);
constexpr ColorRgb565 kOcean2        = EncodeRgb565(0x00, 0x69, 0xaa);
constexpr ColorRgb565 kOcean3        = EncodeRgb565(0x00, 0x98, 0xdc);
constexpr ColorRgb565 kOcean4        = EncodeRgb565(0x00, 0xcd, 0xf9);
constexpr ColorRgb565 kOcean5        = EncodeRgb565(0x0c, 0xf1, 0xff);
constexpr ColorRgb565 kOcean6        = EncodeRgb565(0x94, 0xfd, 0xff);
constexpr ColorRgb565 kCandyGrape3   = EncodeRgb565(0xfd, 0xd2, 0xed);
constexpr ColorRgb565 kCandyGrape2   = EncodeRgb565(0xf3, 0x89, 0xf5);
constexpr ColorRgb565 kCandyGrape1   = EncodeRgb565(0xdb, 0x3f, 0xfd);
constexpr ColorRgb565 kCandyGrape0   = EncodeRgb565(0x7a, 0x09, 0xfa);
constexpr ColorRgb565 kRoyalBlue2    = EncodeRgb565(0x30, 0x03, 0xd9);
constexpr ColorRgb565 kRoyalBlue1    = EncodeRgb565(0x0c, 0x02, 0x93);
constexpr ColorRgb565 kRoyalBlue0    = EncodeRgb565(0x03, 0x19, 0x3f);
constexpr ColorRgb565 kPurple0       = EncodeRgb565(0x3b, 0x14, 0x43);
constexpr ColorRgb565 kPurple1       = EncodeRgb565(0x62, 0x24, 0x61);
constexpr ColorRgb565 kPurple2       = EncodeRgb565(0x93, 0x38, 0x8f);
constexpr ColorRgb565 kPurple3       = EncodeRgb565(0xca, 0x52, 0xc9);
constexpr ColorRgb565 kSalmon0       = EncodeRgb565(0xc8, 0x50, 0x86);
constexpr ColorRgb565 kSalmon1       = EncodeRgb565(0xf6, 0x81, 0x87);
constexpr ColorRgb565 kRed4          = EncodeRgb565(0xf5, 0x55, 0x5d);
constexpr ColorRgb565 kRed3          = EncodeRgb565(0xea, 0x32, 0x3c);
constexpr ColorRgb565 kRed2          = EncodeRgb565(0xc4, 0x24, 0x30);
constexpr ColorRgb565 kRed1          = EncodeRgb565(0x89, 0x1e, 0x2b);
constexpr ColorRgb565 kRed0          = EncodeRgb565(0x57, 0x1c, 0x27);
// clang-format on

/// @}

}  // namespace pw::display::colors::rgb565::e64
