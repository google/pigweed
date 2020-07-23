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

#include "pw_checksum/ccitt_crc16.h"

using std::byte;

namespace pw::hdlc_lite {
namespace {

constexpr byte kHdlcFrameDelimiter = byte{0x7E};
constexpr byte kHdlcEscape = byte{0x7D};
constexpr std::array<byte, 2> kEscapedFrameDelimiterArray = {byte{0x7D},
                                                             byte{0x5E}};
constexpr std::array<byte, 2> kEscapedEscapeFlagArray = {byte{0x7D},
                                                         byte{0x5D}};

Status WriteFrameDelimiter(stream::Writer& writer) {
  return writer.Write(kHdlcFrameDelimiter);
}

Status EscapeAndWriteByte(const byte b, stream::Writer& writer) {
  if (b == kHdlcFrameDelimiter) {
    return writer.Write(kEscapedFrameDelimiterArray);
  } else if (b == kHdlcEscape) {
    return writer.Write(kEscapedEscapeFlagArray);
  }
  return writer.Write(b);
}

Status WriteCrc(uint16_t crc, stream::Writer& writer) {
  if (Status status = EscapeAndWriteByte(byte(crc & 0x00FF), writer)) {
    return status;
  }
  return EscapeAndWriteByte(byte((crc & 0xFF00) >> 8), writer);
}

bool NeedsEscaping(byte b) {
  return (b == kHdlcFrameDelimiter || b == kHdlcEscape);
}

}  // namespace

Status EncodeAndWritePayload(ConstByteSpan payload, stream::Writer& writer) {
  uint16_t crc = 0xFFFF;

  if (Status status = WriteFrameDelimiter(writer); !status.ok()) {
    return status;
  }

  auto begin = payload.begin();
  while (true) {
    auto end = std::find_if(begin, payload.end(), NeedsEscaping);

    if (Status status = writer.Write(std::span(begin, end)); !status.ok()) {
      return status;
    }
    crc = checksum::CcittCrc16(std::span(begin, end), crc);

    if (end == payload.end()) {
      break;
    }
    crc = checksum::CcittCrc16(*end, crc);
    if (Status status = EscapeAndWriteByte(*end, writer); !status.ok()) {
      return status;
    }
    begin = end + 1;
  }

  if (Status status = WriteCrc(crc, writer); !status.ok()) {
    return status;
  }
  if (Status status = WriteFrameDelimiter(writer); !status.ok()) {
    return status;
  }
  return Status::OK;
}

}  // namespace pw::hdlc_lite
