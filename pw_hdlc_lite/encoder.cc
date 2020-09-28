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

#include "pw_hdlc_lite/encoder.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <span>

#include "pw_bytes/endian.h"
#include "pw_checksum/crc32.h"
#include "pw_hdlc_lite_private/protocol.h"

using std::byte;

namespace pw::hdlc_lite {
namespace {

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

// Encodes and writes HDLC frames.
class Encoder {
 public:
  constexpr Encoder(stream::Writer& output) : writer_(output) {}

  // Writes the header for an I-frame. After successfully calling
  // StartInformationFrame, WriteData may be called any number of times.
  Status StartInformationFrame(uint8_t address);

  // Writes data for an ongoing frame. Must only be called after a successful
  // StartInformationFrame call, and prior to a FinishFrame() call.
  Status WriteData(ConstByteSpan data);

  // Finishes a frame. Writes the frame check sequence and a terminating flag.
  Status FinishFrame();

 private:
  stream::Writer& writer_;
  checksum::Crc32 fcs_;
};

Status Encoder::StartInformationFrame(uint8_t address) {
  fcs_.clear();
  if (Status status = writer_.Write(kFlag); !status.ok()) {
    return status;
  }

  const byte address_and_control[] = {std::byte{address}, kUnusedControl};
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
      return Status::Ok();
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

}  // namespace

Status WriteInformationFrame(uint8_t address,
                             ConstByteSpan payload,
                             stream::Writer& writer) {
  Encoder encoder(writer);

  if (Status status = encoder.StartInformationFrame(address); !status.ok()) {
    return status;
  }
  if (Status status = encoder.WriteData(payload); !status.ok()) {
    return status;
  }
  return encoder.FinishFrame();
}

}  // namespace pw::hdlc_lite
