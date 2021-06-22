// Copyright 2021 The Pigweed Authors
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

namespace pw::tls_client {

Session::Session(const SessionOptions&) {}

Session::~Session() = default;

Result<Session*> Session::Create(const SessionOptions&) {
  // TODO(pwbug/398): To implement
  return PW_STATUS_UNIMPLEMENTED;
}

Status Session::Open() {
  // TODO(pwbug/398): To implement
  return PW_STATUS_UNIMPLEMENTED;
}

Status Session::Close() {
  // TODO(pwbug/398): To implement
  return PW_STATUS_UNIMPLEMENTED;
}

StatusWithSize Session::DoRead(ByteSpan) {
  // TODO(pwbug/398): To implement
  return StatusWithSize(PW_STATUS_UNIMPLEMENTED, 0);
}

Status Session::DoWrite(ConstByteSpan) {
  // TODO(pwbug/398): To implement
  return PW_STATUS_UNIMPLEMENTED;
}

TLSStatus Session::GetLastTLSStatus() { return TLSStatus::kOk; }

}  // namespace pw::tls_client
