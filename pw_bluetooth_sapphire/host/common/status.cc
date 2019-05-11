// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "status.h"

namespace bt {

std::string HostErrorToString(HostError error) {
  switch (error) {
    case HostError::kNoError:
      return "success";
    case HostError::kNotFound:
      return "not found";
    case HostError::kNotReady:
      return "not ready";
    case HostError::kTimedOut:
      return "timed out";
    case HostError::kInvalidParameters:
      return "invalid parameters";
    case HostError::kCanceled:
      return "canceled";
    case HostError::kInProgress:
      return "in progress";
    case HostError::kNotSupported:
      return "not supported";
    case HostError::kPacketMalformed:
      return "packet malformed";
    case HostError::kLinkDisconnected:
      return "link disconnected";
    case HostError::kOutOfMemory:
      return "out of memory";
    case HostError::kInsufficientSecurity:
      return "insufficient security";
    case HostError::kProtocolError:
      return "protocol error";
    case HostError::kFailed:
      return "failed";
  }
  return "(unknown)";
}

}  // namespace bt
