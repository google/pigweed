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
#pragma once

#include <span>

#include "pw_router/egress.h"

namespace pw::router {

// Router egress that dispatches to a free function.
class EgressFunction final : public Egress {
 public:
  constexpr EgressFunction(Status (*func)(ConstByteSpan)) : func_(*func) {}

  Status SendPacket(ConstByteSpan packet) final { return func_(packet); }

 private:
  Status (&func_)(ConstByteSpan);
};

}  // namespace pw::router
