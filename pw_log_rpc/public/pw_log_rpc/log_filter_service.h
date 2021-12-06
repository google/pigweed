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

#include "pw_log/proto/log.raw_rpc.pb.h"
#include "pw_log_rpc/log_filter_map.h"
#include "pw_status/status_with_size.h"

namespace pw::log_rpc {

// Provides a way to retrieve and modify log filters.
class FilterService final
    : public log::pw_rpc::raw::Filters::Service<FilterService> {
 public:
  FilterService(FilterMap& filter_map) : filter_map_(filter_map) {}

  //  Modifies a log filter and its rules. The filter must be registered in the
  //  provided filter map.
  StatusWithSize SetFilter(ConstByteSpan request, ByteSpan);

  // Retrieves a log filter and its rules. The filter must be registered in the
  // provided filter map.
  StatusWithSize GetFilter(ConstByteSpan request, ByteSpan response);

  StatusWithSize ListFilterIds(ConstByteSpan, ByteSpan response);

 private:
  FilterMap& filter_map_;
};

}  // namespace pw::log_rpc
