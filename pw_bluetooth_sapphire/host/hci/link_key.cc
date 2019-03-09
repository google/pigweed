// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "link_key.h"

namespace btlib {
namespace hci {

LinkKey::LinkKey() : rand_(0), ediv_(0) { value_.fill(0); }

LinkKey::LinkKey(const common::UInt128& ltk, uint64_t rand, uint16_t ediv)
    : value_(ltk), rand_(rand), ediv_(ediv) {}

}  // namespace hci
}  // namespace btlib
