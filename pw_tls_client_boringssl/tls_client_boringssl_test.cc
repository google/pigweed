// Copyright 2020 The Pigweed Authors
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

#include "pw_tls_client/session.h"
#include "pw_unit_test/framework.h"

// The following header contains a set of test certificates and keys.
// It is generated by
// third_party/boringssl/py/boringssl/generate_test_data.py.
#include "test_certs_and_keys.h"

namespace pw::tls_client {
namespace {

TEST(TLSClientBoringSSL, SessionCreationSucceeds) {
  SessionOptions options;
  auto res = Session::Create(options);
  ASSERT_EQ(res.status(), PW_STATUS_UNIMPLEMENTED);
}

}  // namespace
}  // namespace pw::tls_client
