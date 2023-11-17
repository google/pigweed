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

#include "pw_bluetooth_sapphire/internal/host/l2cap/fcs.h"

namespace bt::l2cap {
namespace {

// When constructed, memoizes the remainders yielded by running
// RemainderForOctet on each of the possible octet values.
class RemainderTable {
 public:
  constexpr RemainderTable() {
    for (unsigned i = 0; i < sizeof(remainders_) / sizeof(remainders_[0]);
         i++) {
      remainders_[i] = ComputeRemainderForOctet(static_cast<uint8_t>(i));
    }
  }

  constexpr uint16_t GetRemainderForOctet(uint8_t b) const {
    return remainders_[b];
  }

 private:
  // Subtrahend for each iteration of the linear feedback shift register (LFSR)
  // representing the polynomial D**16 + D**15 + D**2 + D**0, written in
  // LSb-left form with the (implicit) D**16 coefficient omitted.
  static constexpr uint16_t kPolynomial = 0b1010'0000'0000'0001;

  // Compute the remainder of |b| divided by |kPolynomial| in which each bit
  // position is a mod 2 coefficient of a polynomial, with the rightmost bit as
  // the coefficient of the greatest power. This performs the same function as
  // loading LFSR bits [8:15] (v5.0, Vol 3, Part A, Section 3.3.5, Figure 3.4)
  // with |b| bits [7:0], then operating it for eight cycles, and returns the
  // value in the register (i.e. as if the Figure 3.4 switch were set to
  // position 2).
  //
  // Note: polynomial mod 2 arithmetic implies that addition and subtraction do
  // not carry. Hence both are equivalent to XOR, and the use of "add" or
  // "subtract" are analogies to regular long division.
  static constexpr uint16_t ComputeRemainderForOctet(const uint8_t b) {
    uint16_t accumulator = b;
    for (size_t i = 0; i < 8; i++) {
      // Subtract the divisor if accumulator is greater than it. Polynomial mod
      // 2 comparison only looks at the most powerful coefficient (which is the
      // rightmost bit).
      const bool subtract = accumulator & 0b1;

      // Because data shifts in LSb-first (Figure 3.4), this shifts in the
      // MSb-to-LSb direction, which is towards the right.
      accumulator >>= 1;
      if (subtract) {
        accumulator ^= kPolynomial;
      }
    }
    return accumulator;
  }

  uint16_t remainders_[1 << 8] = {};
};

constexpr RemainderTable gRemainderTable;

}  // namespace

// First principles derivation of the Bluetooth FCS specification referenced "A
// Painless Guide to CRC Error Detection Algorithms" by Ross Williams, accessed
// at https://archive.org/stream/PainlessCRC/crc_v3.txt
FrameCheckSequence ComputeFcs(BufferView view,
                              FrameCheckSequence initial_value) {
  // Initial state of the accumulation register is all zeroes per Figure 3.5.
  uint16_t remainder = initial_value.fcs;

  // Each iteration operates the LFSR for eight cycles, shifting in an octet of
  // message.
  for (const uint8_t byte : view) {
    // Add the remainder to the next eight bits of message.
    const uint8_t dividend = static_cast<uint8_t>(byte ^ remainder);

    // Because only the least significant eight bits are fed back into the LFSR,
    // the other bits can be shifted within the accumulation register without
    // modification.
    const uint16_t unused_remainder = remainder >> 8;

    // Perform eight rounds of division. Add the remainder produced to the bits
    // of the remainder that we haven't used (the above shifting is associative
    // wrt this addition).
    remainder =
        gRemainderTable.GetRemainderForOctet(dividend) ^ unused_remainder;
  }

  return FrameCheckSequence{remainder};
}

}  // namespace bt::l2cap
