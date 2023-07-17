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

#pragma once

#include "pw_digital_io/digital_io.h"
#include "pw_digital_io/digital_io.pwpb.h"
#include "pw_digital_io/digital_io.rpc.pwpb.h"
#include "pw_function/function.h"
#include "pw_rpc/pwpb/server_reader_writer.h"
#include "pw_span/span.h"

namespace pw::digital_io {

// Implementation class for pw.digital_io.DigitalIo.
//
// Takes an array of DigitalIoOptional lines to be exposed via the service.
class DigitalIoService
    : public pw_rpc::pwpb::DigitalIo::Service<DigitalIoService> {
 public:
  explicit DigitalIoService(
      pw::span<std::reference_wrapper<DigitalIoOptional>> lines)
      : lines_(lines) {}

  void Enable(const pwpb::DigitalIoEnableRequest::Message& request,
              rpc::PwpbUnaryResponder<pwpb::DigitalIoEnableResponse::Message>&
                  responder);

  void SetState(
      const pwpb::DigitalIoSetStateRequest::Message& request,
      rpc::PwpbUnaryResponder<pwpb::DigitalIoSetStateResponse::Message>&
          responder);

  void GetState(
      const pwpb::DigitalIoGetStateRequest::Message& request,
      rpc::PwpbUnaryResponder<pwpb::DigitalIoGetStateResponse::Message>&
          responder);

 private:
  pw::span<std::reference_wrapper<DigitalIoOptional>> lines_;
};

}  // namespace pw::digital_io
