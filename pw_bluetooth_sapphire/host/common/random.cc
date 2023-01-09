// Copyright 2023 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "random.h"

namespace bt {
namespace {

pw::random::RandomGenerator* g_random_generator = nullptr;

}

pw::random::RandomGenerator* random_generator() { return g_random_generator; }

void set_random_generator(pw::random::RandomGenerator* generator) {
  BT_ASSERT(!generator || !g_random_generator);
  g_random_generator = generator;
}

}  // namespace bt
