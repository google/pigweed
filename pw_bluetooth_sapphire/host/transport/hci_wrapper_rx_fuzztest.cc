// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/async-testing/test_loop.h>

#include "hci_wrapper.h"

namespace bt::hci {

void fuzz(const uint8_t* data, size_t size) {
  zx::channel cmd0, cmd1;
  zx::channel acl0, acl1;
  BT_ASSERT(zx::channel::create(/*flags=*/0, &cmd0, &cmd1) == ZX_OK);
  BT_ASSERT(zx::channel::create(/*flags=*/0, &acl0, &acl1) == ZX_OK);
  auto device =
      std::make_unique<DummyDeviceWrapper>(std::move(cmd1), std::move(acl1), /*features=*/0);

  async::TestLoop loop;
  auto hci = HciWrapper::Create(std::move(device), loop.dispatcher());
  BT_ASSERT(hci->Initialize([](zx_status_t error) {}));

  hci->SetAclCallback([](auto) {});
  hci->SetEventCallback([](auto) {});

  cmd0.write(/*flags=*/0, data, static_cast<uint32_t>(size), /*handles=*/nullptr,
             /*num_handles=*/0);
  acl0.write(/*flags=*/0, data, static_cast<uint32_t>(size), /*handles=*/nullptr,
             /*num_handles=*/0);
  loop.RunUntilIdle();
}

}  // namespace bt::hci

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  bt::hci::fuzz(data, size);
  return 0;
}
