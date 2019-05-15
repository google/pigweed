// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "src/connectivity/bluetooth/core/bt-host/l2cap/fragmenter.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/pdu.h"

namespace bt {
namespace l2cap {
namespace internal {

// PduGenerator splits a raw buffer into a sequence of PDUs.
class PduGenerator final {
 public:
  // Constructs a generator from |input_buf|, where |input_buf| contains at
  // least |buflen| bytes. The generator will attempt to provide |num_pdus|
  // equal-sized PDUs.
  PduGenerator(const uint8_t* input_buf, size_t buflen, size_t num_pdus);

  // Returns a PDU if possible, or std::nullopt otherwise.
  std::optional<PDU> GetNextPdu();

 private:
  const uint8_t* const input_buf_;
  const size_t buf_len_bytes_;
  const size_t pdu_len_bytes_;

  Fragmenter fragmenter_;
  size_t input_pos_;
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bt
