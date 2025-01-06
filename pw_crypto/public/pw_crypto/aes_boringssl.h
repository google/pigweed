// Copyright 2024 The Pigweed Authors
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

#pragma once

#include "pw_crypto/aes_backend_defs.h"

namespace pw::crypto::aes::backend {
/// The boringssl backend supports 128-bit, 192-bit, and 256-bit keys.
template <>
inline constexpr auto supported<AesOperation::kUnsafeEncryptBlock> =
    SupportedKeySize::k128 | SupportedKeySize::k192 | SupportedKeySize::k256;
}  // namespace pw::crypto::aes::backend
