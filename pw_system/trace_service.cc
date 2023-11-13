// Copyright 2023 The Pigweed Authors
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

#include "pw_system/trace_service.h"

#include "pw_trace_tokenized/trace_service_pwpb.h"
#include "pw_trace_tokenized/trace_tokenized.h"

namespace pw::system {
namespace {

// TODO: b/305795949 - place trace_data in a persistent region of memory
TracePersistentBuffer trace_data;
persistent_ram::PersistentBufferWriter trace_data_writer(
    trace_data.GetWriter());

trace::TraceService trace_service(trace::GetTokenizedTracer(),
                                  trace_data_writer);

}  // namespace

TracePersistentBuffer& GetTraceData() { return trace_data; }

void RegisterTraceService(rpc::Server& rpc_server, uint32_t transfer_id) {
  rpc_server.RegisterService(trace_service);
  trace_service.SetTransferId(transfer_id);
}

}  // namespace pw::system
