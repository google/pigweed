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

#include <cstdint>

namespace pw::i2c {

// I3C CCC (Common Command Code) commands
// MIPI Specification for I3C, version 1.1.1
// 5.1.9.3 CCC Command Definitions
enum class I3cCcc : uint8_t {
  // Broadcast CCC commands
  kEncBroadcast = 0x00,
  kDisecBroadcast = 0x01,
  kEntdas0Broadcast = 0x02,
  kEntas1Broadcast = 0x03,
  kEntas2Broadcast = 0x04,
  kentas3Broadcast = 0x05,
  kRstdaaBroadcast = 0x06,
  kEntdaaBroadcast = 0x07,
  kDefslvsBroadcast = 0x08,
  kSetmwlBroadcast = 0x09,
  kSetmrlBroadcast = 0x0a,
  kEnttmBroadcast = 0x0b,
  kEnthdr0Broadcast = 0x20,
  kEnthdr1Broadcast = 0x21,
  kEnthdr2Broadcast = 0x22,
  kEnthdr3Broadcast = 0x23,
  kEnthdr4Broadcast = 0x24,
  kEnthdr5Broadcast = 0x25,
  kEnthdr6Broadcast = 0x26,
  kEnthdr7Broadcast = 0x27,
  kSetxtimeBroadcast = 0x28,
  kSetaasaBroadcast = 0x29,
  // Direct CCC commands
  kEncDirect = 0x80,
  kDisecDirect = 0x81,
  kEntas0Direct = 0x82,
  kEntas1Direct = 0x83,
  kEntas2Direct = 0x84,
  kEntas3Direct = 0x85,
  kRstdaaDirect = 0x86,
  kSetdasaDirect = 0x87,
  kSetnewdaDirect = 0x88,
  kSetmwlDirect = 0x89,
  kSetmrlDirect = 0x8a,
  kGetmwlDirect = 0x8b,
  kGetmrlDirect = 0x8c,
  kGetpidDirect = 0x8d,
  kGetbcrDirect = 0x8e,
  kGetdcrDirect = 0x8f,
  kGetstatusDirect = 0x90,
  kGetaccmstDirect = 0x91,
  kGetbrgtgtDirect = 0x93,
  kGetmxdsDirect = 0x94,
  kGethdrcapDirect = 0x95,
  kSetxtimeDirect = 0x98,
  kGetxtimeDirect = 0x99,
};

inline constexpr uint8_t kCccDirectBit = 1 << 7;

enum class I3cCccAction : bool {
  kWrite,
  kRead,
};

}  // namespace pw::i2c
