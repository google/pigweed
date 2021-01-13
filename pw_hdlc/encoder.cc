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

#include "pw_hdlc/encoder.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <span>

#include "pw_bytes/endian.h"
#include "pw_hdlc/internal/encoder.h"
#include "pw_hdlc_private/protocol.h"

using std::byte;

namespace pw::hdlc {
namespace internal {

// Indicates this an information packet with sequence numbers set to 0.
constexpr byte kUnusedControl = byte{0};

Status EscapeAndWrite(const byte b, stream::Writer& writer) {
  if (b == kFlag) {
    return writer.Write(kEscapedFlag);
  }
  if (b == kEscape) {
    return writer.Write(kEscapedEscape);
  }
  return writer.Write(b);
}

Status Encoder::StartInformationFrame(uint8_t address) {
  fcs_.clear();
  if (Status status = writer_.Write(kFlag); !status.ok()) {
    return status;
  }

  const byte address_and_control[] = {
      std::byte{address}, kUnusedControl, kUnusedControl};
  return WriteData(address_and_control);
}

Status Encoder::StartUnnumberedFrame(uint8_t address) {
  fcs_.clear();
  if (Status status = writer_.Write(kFlag); !status.ok()) {
    return status;
  }

  const byte address_and_control[] = {
      std::byte{address}, UFrameControl::UnnumberedInformation().data()};
  return WriteData(address_and_control);
}

Status Encoder::WriteData(ConstByteSpan data) {
  auto begin = data.begin();
  while (true) {
    auto end = std::find_if(begin, data.end(), NeedsEscaping);

    if (Status status = writer_.Write(std::span(begin, end)); !status.ok()) {
      return status;
    }
    if (end == data.end()) {
      fcs_.Update(data);
      return OkStatus();
    }
    if (Status status = EscapeAndWrite(*end, writer_); !status.ok()) {
      return status;
    }
    begin = end + 1;
  }
}

Status Encoder::FinishFrame() {
  if (Status status =
          WriteData(bytes::CopyInOrder(std::endian::little, fcs_.value()));
      !status.ok()) {
    return status;
  }
  return writer_.Write(kFlag);
}

size_t Encoder::MaxEncodedSize(uint8_t address, ConstByteSpan payload) {
  constexpr size_t kFcsMaxSize = 8;  // Worst case FCS: 0x7e7e7e7e.
  size_t encoded_address_size = NeedsEscaping(std::byte{address}) ? 2 : 1;
  size_t encoded_payload_size =
      payload.size() +
      std::count_if(payload.begin(), payload.end(), NeedsEscaping);

  return encoded_address_size + encoded_payload_size + kFcsMaxSize;
}

}  // namespace internal

Status WriteUIFrame(uint8_t address,
                    ConstByteSpan payload,
                    stream::Writer& writer) {
  if (internal::Encoder::MaxEncodedSize(address, payload) >
      writer.ConservativeWriteLimit()) {
    return Status::ResourceExhausted();
  }

  internal::Encoder encoder(writer);

  if (Status status = encoder.StartUnnumberedFrame(address); !status.ok()) {
    return status;
  }
  if (Status status = encoder.WriteData(payload); !status.ok()) {
    return status;
  }
  return encoder.FinishFrame();
}

}  // namespace pw::hdlc
