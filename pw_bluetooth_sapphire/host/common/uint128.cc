// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "uint128.h"

#include "random.h"

namespace bt {

UInt128 RandomUInt128() { return Random<UInt128>(); }

}  // namespace bt
