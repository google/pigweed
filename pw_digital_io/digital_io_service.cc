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

#define PW_LOG_MODULE_NAME "IO"

#include "pw_digital_io/digital_io_service.h"

#include "pw_digital_io/digital_io.pwpb.h"
#include "pw_log/log.h"
#include "pw_result/result.h"
#include "pw_rpc/pwpb/server_reader_writer.h"
#include "pw_status/status.h"

namespace pw::digital_io {

void DigitalIoService::Enable(
    const pwpb::DigitalIoEnableRequest::Message& request,
    rpc::PwpbUnaryResponder<pwpb::DigitalIoEnableResponse::Message>&
        responder) {
  Status result = OkStatus();
  if (request.line_index >= lines_.size()) {
    result = Status::InvalidArgument();
  } else {
    auto& line = lines_[request.line_index].get();
    if (request.enable) {
      result = line.Enable();
    } else {
      result = line.Disable();
    }
  }

  if (const auto finished = responder.Finish({}, result); !finished.ok()) {
    PW_LOG_ERROR("Enable failed to send response %d", finished.code());
  }
}

void DigitalIoService::SetState(
    const pwpb::DigitalIoSetStateRequest::Message& request,
    rpc::PwpbUnaryResponder<pwpb::DigitalIoSetStateResponse::Message>&
        responder) {
  Status result = OkStatus();
  if (request.line_index >= lines_.size()) {
    result = Status::InvalidArgument();
  } else {
    auto& line = lines_[request.line_index].get();
    if (!line.provides_output()) {
      result = Status::InvalidArgument();
    } else {
      if (request.state == pwpb::DigitalIoState::kActive) {
        result = line.SetStateActive();
      } else {
        result = line.SetStateInactive();
      }
    }
  }

  if (const auto finished = responder.Finish({}, result); !finished.ok()) {
    PW_LOG_ERROR("SetState failed to send response %d", finished.code());
  }
}

void DigitalIoService::GetState(
    const pwpb::DigitalIoGetStateRequest::Message& request,
    rpc::PwpbUnaryResponder<pwpb::DigitalIoGetStateResponse::Message>&
        responder) {
  pw::Result<State> result;
  if (request.line_index >= lines_.size()) {
    result = pw::Status::InvalidArgument();
  } else {
    auto& line = lines_[request.line_index].get();
    if (!line.provides_input()) {
      result = pw::Status::InvalidArgument();
    } else {
      result = line.GetState();
    }
  }

  auto finished = pw::OkStatus();
  if (result.ok()) {
    pwpb::DigitalIoState state = *result == State::kActive
                                     ? pwpb::DigitalIoState::kActive
                                     : pwpb::DigitalIoState::kInactive;
    finished = responder.Finish({.state = state}, OkStatus());
  } else {
    finished = responder.Finish({}, result.status());
  }

  if (!finished.ok()) {
    PW_LOG_ERROR("GetState failed to send response %d", finished.code());
  }
}

}  // namespace pw::digital_io
