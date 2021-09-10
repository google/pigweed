// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include "src/connectivity/bluetooth/core/bt-host/common/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"

namespace bt::common {

void fuzz(const uint8_t* data, size_t size) {
  FuzzedDataProvider fuzzed_data(data, size);
  auto adv_flags = fuzzed_data.ConsumeIntegral<AdvFlags>();
  bool include_adv_flags = fuzzed_data.ConsumeBool();
  auto write_buffer_size = fuzzed_data.ConsumeIntegralInRange(0, 2000);
  auto adv_data = fuzzed_data.ConsumeRemainingBytes<uint8_t>();

  AdvertisingData::ParseResult result = AdvertisingData::FromBytes(BufferView(adv_data));

  if (result.is_ok()) {
    DynamicByteBuffer write_buffer(write_buffer_size);
    result->WriteBlock(&write_buffer, include_adv_flags ? std::optional(adv_flags) : std::nullopt);
  }
}

}  // namespace bt::common

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  bt::common::fuzz(data, size);
  return 0;
}
