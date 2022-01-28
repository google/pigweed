// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include "src/connectivity/bluetooth/core/bt-host/sdp/pdu.h"

namespace bt::sdp {

void fuzz(const uint8_t* data, size_t size) {
  FuzzedDataProvider fuzzed_data(data, size);
  uint8_t type = fuzzed_data.ConsumeIntegral<uint8_t>();
  std::vector<uint8_t> remaining_bytes = fuzzed_data.ConsumeRemainingBytes<uint8_t>();
  DynamicByteBuffer buf(remaining_bytes.size());
  memcpy(buf.mutable_data(), remaining_bytes.data(), remaining_bytes.size());
  fitx::result<Error<>> status = fitx::ok();
  ErrorResponse error_response;
  ServiceSearchResponse service_search_response;
  ServiceAttributeResponse service_attribute_response;
  ServiceSearchAttributeResponse service_search_attribute_response;
  switch (type % 4) {
    case 0:
      status = error_response.Parse(buf);
      break;
    case 1:
      status = service_search_response.Parse(buf);
      break;
    case 2:
      status = service_attribute_response.Parse(buf);
      break;
    case 3:
      status = service_search_attribute_response.Parse(buf);
      break;
  }
}

}  // namespace bt::sdp

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  bt::sdp::fuzz(data, size);
  return 0;
}
