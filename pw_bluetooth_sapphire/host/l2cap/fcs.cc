// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

#include "fcs.h"

namespace bt::l2cap {
namespace {

// Subtrahend for each iteration of the linear feedback shift register (LFSR) representing the
// polynomial D**16 + D**15 + D**2 + D**0, written in LSb-left form with the (implicit) D**16
// coefficient omitted.
constexpr uint16_t kPolynomial = 0b1010'0000'0000'0001;

// Compute the remainder of |b| divided by |kPolynomial| in which each bit position is a mod 2
// coefficient of a polynomial, with the least significant bit as coefficient of the greatest power.
// This performs the same function as loading LFSR bits [8:15] (v5.0, Vol 3, Part A, Section 3.3.5,
// Figure 3.4) with |b| bits [7:0], then operating it for eight cycles, and returns the value in the
// register (i.e. as if the Figure 3.4 switch were set to position 2).
//
// Note: polynomial mod 2 arithmetic implies that addition and subtraction do not carry. Hence both
// are equivalent to XOR, and the use of "add" or "subtract" are analogies to regular long division.
//
// Memoization is left up to the compiler, not explicitly declared as a lookup table.
[[gnu::const]] constexpr uint16_t RemainderForOctet(const uint8_t b) {
  uint16_t accumulator = b;
  for (size_t i = 0; i < 8; i++) {
    // Subtract the divisor if accumulator is greater than it. Polynomial mod 2 comparison only
    // looks at the most powerful coefficient (which is the LSb).
    const bool subtract = accumulator & 0b1;

    // Because data shifts in LSb-first (Figure 3.4), this shifts in the MSb-to-LSb direction.
    accumulator >>= 1;
    if (subtract) {
      accumulator ^= kPolynomial;
    }
  }
  return accumulator;
}

}  // namespace

// First principles derivation of the Bluetooth FCS specification referenced "A Painless Guide to
// CRC Error Detection Algorithms" by Ross Williams, accessed at
// https://archive.org/stream/PainlessCRC/crc_v3.txt
FrameCheckSequence ComputeFcs(BufferView view, FrameCheckSequence initial_value) {
  // Initial state of the accumulation register is all zeroes per Figure 3.5.
  uint16_t remainder = initial_value.fcs;

  // Each iteration operates the LFSR for eight cycles, shifting in an octet of message.
  for (const uint8_t byte : view) {
    // Add the remainder to the next eight bits of message.
    const uint8_t dividend = byte ^ remainder;

    // Because only the least significant eight bits are fed back into the LFSR, the other bits can
    // be shifted within the accumulation register without modification.
    const uint8_t unused_remainder = remainder >> (8 * (sizeof(uint16_t) - 1));

    // Perform eight rounds of division. Add the remainder produced to the bits of the remainder
    // that we haven't used (the above shifting is associative wrt this addition).
    remainder = RemainderForOctet(dividend) ^ unused_remainder;
  }

  return FrameCheckSequence{remainder};
}

}  // namespace bt::l2cap
