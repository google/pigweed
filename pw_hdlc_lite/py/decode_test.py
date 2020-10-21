#!/usr/bin/env python
# Copyright 2020 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Contains the Python decoder tests and generates C++ decoder tests."""

from typing import Iterator, List, NamedTuple, Tuple
import unittest

from pw_build.generated_tests import Context, PyTest, TestGenerator, GroupOrTest
from pw_build.generated_tests import parse_test_generation_args
from pw_hdlc_lite.decode import Frame, FrameDecoder, FrameStatus, NO_ADDRESS
from pw_hdlc_lite.protocol import frame_check_sequence as fcs


def _encode(address: int, control: int, data: bytes) -> bytes:
    frame = bytearray([address, control]) + data
    frame += fcs(frame)
    frame = frame.replace(b'\x7d', b'\x7d\x5d')
    frame = frame.replace(b'\x7e', b'\x7d\x5e')
    return b''.join([b'\x7e', frame, b'\x7e'])


class Expected(NamedTuple):
    address: int
    control: bytes
    data: bytes
    status: FrameStatus = FrameStatus.OK

    def __eq__(self, other) -> bool:
        """Define == so an Expected and a Frame can be compared."""
        return (self.address == other.address and self.control == other.control
                and self.data == other.data and self.status is other.status)


_PARTIAL = fcs(b'\x0ACmsg\x5e')
_ESCAPED_FLAG_TEST_CASE = (
    b'\x7e\x0ACmsg\x7d\x7e' + _PARTIAL + b'\x7e',
    [
        Expected(0xA, b'C', b'', FrameStatus.INCOMPLETE),
        Expected(_PARTIAL[0], _PARTIAL[1:2], b'', FrameStatus.INCOMPLETE),
    ],
)

TEST_CASES: Tuple[GroupOrTest[Tuple[bytes, List[Expected]]], ...] = (
    'Empty payload',
    (_encode(0, 0, b''), [Expected(0, b'\0', b'')]),
    (_encode(55, 0x99, b''), [Expected(55, b'\x99', b'')]),
    (_encode(55, 0x99, b'') * 3, [Expected(55, b'\x99', b'')] * 3),
    'Simple one-byte payload',
    (_encode(0, 0, b'\0'), [Expected(0, b'\0', b'\0')]),
    (_encode(123, 0, b'A'), [Expected(123, b'\0', b'A')]),
    'Simple multi-byte payload',
    (_encode(0, 0, b'Hello, world!'), [Expected(0, b'\0', b'Hello, world!')]),
    (_encode(123, 0, b'\0\0\1\0\0'), [Expected(123, b'\0', b'\0\0\1\0\0')]),
    'Escaped one-byte payload',
    (_encode(1, 2, b'\x7e'), [Expected(1, b'\2', b'\x7e')]),
    (_encode(1, 2, b'\x7d'), [Expected(1, b'\2', b'\x7d')]),
    (_encode(1, 2, b'\x7e') + _encode(1, 2, b'\x7d'),
     [Expected(1, b'\2', b'\x7e'),
      Expected(1, b'\2', b'\x7d')]),
    'Escaped address',
    (_encode(0x7e, 0, b'A'), [Expected(0x7e, b'\0', b'A')]),
    (_encode(0x7d, 0, b'B'), [Expected(0x7d, b'\0', b'B')]),
    'Escaped control',
    (_encode(0, 0x7e, b'C'), [Expected(0, b'\x7e', b'C')]),
    (_encode(0, 0x7d, b'D'), [Expected(0, b'\x7d', b'D')]),
    'Escaped address and control',
    (_encode(0x7e, 0x7d, b'E'), [Expected(0x7e, b'\x7d', b'E')]),
    (_encode(0x7d, 0x7e, b'F'), [Expected(0x7d, b'\x7e', b'F')]),
    (_encode(0x7e, 0x7e, b'\x7e'), [Expected(0x7e, b'\x7e', b'\x7e')]),
    'Multiple frames separated by single flag',
    (_encode(0, 0, b'A')[:-1] + _encode(1, 2, b'123'),
     [Expected(0, b'\0', b'A'),
      Expected(1, b'\2', b'123')]),
    (_encode(0xff, 0, b'Yo')[:-1] * 3 + b'\x7e',
     [Expected(0xff, b'\0', b'Yo')] * 3),
    'Ignore empty frames',
    (b'\x7e\x7e', []),
    (b'\x7e' * 10, []),
    (b'\x7e\x7e' + _encode(1, 2, b'3') + b'\x7e' * 5,
     [Expected(1, b'\2', b'3')]),
    (b'\x7e' * 10 + _encode(1, 2, b':O') + b'\x7e' * 3 + _encode(3, 4, b':P'),
     [Expected(1, b'\2', b':O'),
      Expected(3, b'\4', b':P')]),
    'Cannot escape flag',
    (b'\x7e\xAA\x7d\x7e\xab\x00Hello' + fcs(b'\xab\0Hello') + b'\x7e', [
        Expected(0xAA, b'', b'', FrameStatus.INCOMPLETE),
        Expected(0xab, b'\0', b'Hello'),
    ]),
    _ESCAPED_FLAG_TEST_CASE,
    'Frame too short',
    (b'\x7e1\x7e', [Expected(ord('1'), b'', b'', FrameStatus.INCOMPLETE)]),
    (b'\x7e12\x7e', [Expected(ord('1'), b'2', b'', FrameStatus.INCOMPLETE)]),
    (b'\x7e12345\x7e', [Expected(ord('1'), b'2', b'',
                                 FrameStatus.INCOMPLETE)]),
    'Incorrect frame check sequence',
    (b'\x7e123456\x7e',
     [Expected(ord('1'), b'2', b'', FrameStatus.FCS_MISMATCH)]),
    (b'\x7e\1\2msg\xff\xff\xff\xff\x7e',
     [Expected(0x1, b'\2', b'msg', FrameStatus.FCS_MISMATCH)]),
    (_encode(0xA, 0xB, b'???')[:-2] + _encode(1, 2, b'def'), [
        Expected(0xA, b'\x0B', b'??', FrameStatus.FCS_MISMATCH),
        Expected(1, b'\2', b'def'),
    ]),
    'Invalid escape in address',
    (b'\x7e\x7d\x7d\0' + fcs(b'\x5d\0') + b'\x7e',
     [Expected(0,
               fcs(b'\x5d\0')[0:1], b'', FrameStatus.INVALID_ESCAPE)]),
    'Invalid escape in control',
    (b'\x7e\0\x7d\x7d' + fcs(b'\0\x5d') + b'\x7e',
     [Expected(0,
               fcs(b'\0\x5d')[0:1], b'', FrameStatus.INVALID_ESCAPE)]),
    'Invalid escape in data',
    (b'\x7e\0\1\x7d\x7d' + fcs(b'\0\1\x5d') + b'\x7e',
     [Expected(0, b'\1', b'', FrameStatus.INVALID_ESCAPE)]),
    'Frame ends with escape',
    (b'\x7e\x7d\x7e', [Expected(NO_ADDRESS, b'', b'',
                                FrameStatus.INCOMPLETE)]),
    (b'\x7e\1\x7d\x7e', [Expected(1, b'', b'', FrameStatus.INCOMPLETE)]),
    (b'\x7e\1\2abc\x7d\x7e', [Expected(1, b'\2', b'',
                                       FrameStatus.INCOMPLETE)]),
    (b'\x7e\1\2abcd\x7d\x7e',
     [Expected(1, b'\2', b'', FrameStatus.INCOMPLETE)]),
    (b'\x7e\1\2abcd1234\x7d\x7e',
     [Expected(1, b'\2', b'abcd', FrameStatus.INCOMPLETE)]),
    'Inter-frame data is only escapes',
    (b'\x7e\x7d\x7e\x7d\x7e', [
        Expected(NO_ADDRESS, b'', b'', FrameStatus.INCOMPLETE),
        Expected(NO_ADDRESS, b'', b'', FrameStatus.INCOMPLETE),
    ]),
    (b'\x7e\x7d\x7d\x7e\x7d\x7d\x7e', [
        Expected(NO_ADDRESS, b'', b'', FrameStatus.INVALID_ESCAPE),
        Expected(NO_ADDRESS, b'', b'', FrameStatus.INVALID_ESCAPE),
    ]),
    'Data before first flag',
    (b'\0\1' + fcs(b'\0\1'), []),
    (b'\0\1' + fcs(b'\0\1') + b'\x7e',
     [Expected(0, b'\1', b'', FrameStatus.INCOMPLETE)]),
    'No frames emitted until flag',
    (_encode(1, 2, b'3')[:-1], []),
    (b'\x7e' + _encode(1, 2, b'3')[1:-1] * 2, []),
)  # yapf: disable
# Formatting for the above tuple is very slow, so disable yapf.

_TESTS = TestGenerator(TEST_CASES)


def _expected(frames: List[Frame]) -> Iterator[str]:
    for i, frame in enumerate(frames, 1):
        if frame.ok():
            yield f'      Frame(kDecodedFrame{i:02}),'
        else:
            yield f'      Status::DATA_LOSS,  // Frame {i}'


_CPP_HEADER = """\
#include "pw_hdlc_lite/decoder.h"

#include <array>
#include <cstddef>
#include <variant>

#include "gtest/gtest.h"
#include "pw_bytes/array.h"

namespace pw::hdlc_lite {
namespace {
"""

_CPP_FOOTER = """\
}  // namespace
}  // namespace pw::hdlc_lite"""


def _cpp_test(ctx: Context) -> Iterator[str]:
    """Generates a C++ test for the provided test data."""
    data, _ = ctx.test_case
    frames = list(FrameDecoder().process(data))
    data_bytes = ''.join(rf'\x{byte:02x}' for byte in data)

    yield f'TEST(Decoder, {ctx.cc_name()}) {{'
    yield f'  static constexpr auto kData = bytes::String("{data_bytes}");\n'

    for i, frame in enumerate(frames, 1):
        if frame.status is FrameStatus.OK:
            frame_bytes = ''.join(rf'\x{byte:02x}' for byte in frame.raw)
            yield (f'  static constexpr auto kDecodedFrame{i:02} = '
                   f'bytes::String("{frame_bytes}");')
        else:
            yield f'  // Frame {i}: {frame.status.value}'

    yield ''

    expected = '\n'.join(_expected(frames)) or '      // No frames'
    decoder_size = max(len(data), 8)  # Make sure large enough for a frame

    yield f"""\
  DecoderBuffer<{decoder_size}> decoder;

  static constexpr std::array<std::variant<Frame, Status>, {len(frames)}> kExpected = {{
{expected}
  }};

  size_t decoded_frames = 0;

  decoder.Process(kData, [&](const Result<Frame>& result) {{
    ASSERT_LT(decoded_frames++, kExpected.size());
    auto& expected = kExpected[decoded_frames - 1];

    if (std::holds_alternative<Status>(expected)) {{
      EXPECT_EQ(Status::DATA_LOSS, result.status());
    }} else {{
      ASSERT_EQ(Status::OK, result.status());

      const Frame& decoded_frame = result.value();
      const Frame& expected_frame = std::get<Frame>(expected);
      EXPECT_EQ(expected_frame.address(), decoded_frame.address());
      EXPECT_EQ(expected_frame.control(), decoded_frame.control());
      ASSERT_EQ(expected_frame.data().size(), decoded_frame.data().size());
      EXPECT_EQ(std::memcmp(expected_frame.data().data(),
                            decoded_frame.data().data(),
                            expected_frame.data().size()),
                0);
    }}
  }});

  EXPECT_EQ(decoded_frames, kExpected.size());
}}"""


def _define_py_test(ctx: Context) -> PyTest:
    data, expected_frames = ctx.test_case

    def test(self) -> None:
        # Decode in one call
        self.assertEqual(expected_frames,
                         list(FrameDecoder().process(data)),
                         msg=f'{ctx.group}: {data!r}')

        # Decode byte-by-byte
        decoder = FrameDecoder()
        decoded_frames: List[Frame] = []
        for i in range(len(data)):
            decoded_frames += decoder.process(data[i:i + 1])

        self.assertEqual(expected_frames,
                         decoded_frames,
                         msg=f'{ctx.group} (byte-by-byte): {data!r}')

    return test


# Class that tests all cases in TEST_CASES.
DecoderTest = _TESTS.python_tests('DecoderTest', _define_py_test)

if __name__ == '__main__':
    args = parse_test_generation_args()
    if args.generate_cc_test:
        _TESTS.cc_tests(args.generate_cc_test, _cpp_test, _CPP_HEADER,
                        _CPP_FOOTER)
    else:
        unittest.main()
