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
#pragma once

#include "pw_assert/assert.h"
#include "pw_rpc/internal/service.h"

namespace pw::rpc::internal {

// Manages a list of registered RPC services.
class ServiceRegistry {
 public:
  constexpr ServiceRegistry() : first_service_(nullptr) {}

  ServiceRegistry(const ServiceRegistry& other) = delete;
  ServiceRegistry& operator=(const ServiceRegistry& other) = delete;

  void Register(Service& service) {
    service.next_ = first_service_;
    first_service_ = &service;
  }

  Service* Find(uint32_t id) const;

 private:
  Service* first_service_;
};

}  // namespace pw::rpc::internal
