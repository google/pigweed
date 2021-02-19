// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include "src/connectivity/bluetooth/core/bt-host/sdp/data_element.h"

namespace bt::sdp {

void fuzz(const uint8_t* data, size_t size) {
  DynamicByteBuffer buf(size);
  memcpy(buf.mutable_data(), data, size);
  DataElement elem;
  DataElement::Read(&elem, buf);
}

}  // namespace bt::sdp

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  bt::sdp::fuzz(data, size);
  return 0;
}
