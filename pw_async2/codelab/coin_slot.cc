// Copyright 2025 The Pigweed Authors
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

#include "coin_slot.h"

#include <mutex>
#include <utility>

namespace codelab {

pw::async2::Poll<unsigned> CoinSlot::Pend(pw::async2::Context& context) {
  std::lock_guard lock(lock_);
  unsigned coins = std::exchange(coins_deposited_, 0);
  if (coins > 0) {
    return coins;
  }
  PW_ASYNC_STORE_WAKER(context, waker_, "coin deposit");
  return pw::async2::Pending();
}

void CoinSlot::Deposit() {
  std::lock_guard lock(lock_);
  coins_deposited_ += 1;
  std::move(waker_).Wake();
}

}  // namespace codelab
