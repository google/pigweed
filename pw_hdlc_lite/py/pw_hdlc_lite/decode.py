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
"""Decoder class for decoding bytes using HDLC-Lite protocol"""

import enum
import logging
from typing import Iterator, NamedTuple, Optional
import zlib

from pw_hdlc_lite import protocol

_LOG = logging.getLogger('pw_hdlc_lite')

NO_ADDRESS = -1
_MIN_FRAME_SIZE = 6  # 1 B address + 1 B control + 4 B CRC-32


class FrameStatus(enum.Enum):
    """Indicates that an error occurred."""
    OK = 'OK'
    FCS_MISMATCH = 'frame check sequence failure'
    FRAMING_ERROR = 'invalid flag or escape characters'


class Frame(NamedTuple):
    """Represents an HDLC frame."""

    # All bytes in the frame (address, control, information, FCS)
    raw: bytes

    # Whether parsing the frame succeeded.
    status: FrameStatus = FrameStatus.OK

    @property
    def address(self) -> int:
        """The frame's address field (assumes only one byte for now)."""
        return self.raw[0] if self.ok() else NO_ADDRESS

    @property
    def control(self) -> bytes:
        """The control byte (assumes only one byte for now)."""
        return self.raw[1:2] if self.ok() else b''

    @property
    def data(self) -> bytes:
        """The information field in the frame."""
        return self.raw[2:-4] if self.ok() else b''

    def ok(self) -> bool:
        """True if this represents a valid frame.

        If false, then parsing failed. The status is set to indicate what type
        of error occurred, and the data field contains all bytes parsed from the
        frame (including bytes parsed as address or control bytes).
        """
        return self.status is FrameStatus.OK

    def __repr__(self) -> str:
        if self.ok():
            body = (f'address={self.address}, control={self.control!r}, '
                    f'data={self.data!r}')
        else:
            body = str(self.status)

        return f'{type(self).__name__}({body})'


class _State(enum.Enum):
    INTERFRAME = 0
    FRAME = 1
    FRAME_ESCAPE = 2


def _check_frame(frame_data: bytes) -> FrameStatus:
    if len(frame_data) < _MIN_FRAME_SIZE:
        return FrameStatus.FRAMING_ERROR

    frame_crc = int.from_bytes(frame_data[-4:], 'little')
    if zlib.crc32(frame_data[:-4]) != frame_crc:
        return FrameStatus.FCS_MISMATCH

    return FrameStatus.OK


class FrameDecoder:
    """Decodes one or more HDLC frames from a stream of data."""
    def __init__(self):
        self._data = bytearray()
        self._state = _State.INTERFRAME

    def process(self, data: bytes) -> Iterator[Frame]:
        """Decodes and yields HDLC frames, including corrupt frames.

        The ok() method on Frame indicates whether it is valid or represents a
        frame parsing error.

        Yields:
          Frames, which may be valid (frame.ok()) or corrupt (!frame.ok())
        """
        for byte in data:
            frame = self._process_byte(byte)
            if frame:
                yield frame

    def process_valid_frames(self, data: bytes) -> Iterator[Frame]:
        """Decodes and yields valid HDLC frames, logging any errors."""
        for frame in self.process(data):
            if frame.ok():
                yield frame
            else:
                _LOG.warning('Failed to decode frame: %s; discarded %d bytes',
                             frame.status.value, len(frame.data))
                _LOG.debug('Discarded data: %s', frame.data)

    def _process_byte(self, byte: int) -> Optional[Frame]:
        frame: Optional[Frame] = None

        if self._state is _State.INTERFRAME:
            if byte == protocol.FLAG:
                if self._data:
                    frame = Frame(bytes(self._data), FrameStatus.FRAMING_ERROR)

                self._state = _State.FRAME
                self._data.clear()
            else:
                self._data.append(byte)
        elif self._state is _State.FRAME:
            if byte == protocol.FLAG:
                if self._data:
                    frame = Frame(bytes(self._data), _check_frame(self._data))

                self._state = _State.FRAME
                self._data.clear()
            elif byte == protocol.ESCAPE:
                self._state = _State.FRAME_ESCAPE
            else:
                self._data.append(byte)
        elif self._state is _State.FRAME_ESCAPE:
            if byte == protocol.FLAG:
                frame = Frame(bytes(self._data), FrameStatus.FRAMING_ERROR)
                self._state = _State.FRAME
                self._data.clear()
            elif byte in (0x5d, 0x5e):  # Valid escape characters
                self._state = _State.FRAME
                self._data.append(protocol.escape(byte))
            else:
                self._state = _State.INTERFRAME
                self._data.append(byte)
        else:
            raise AssertionError(f'Invalid decoder state: {self._state}')

        return frame
