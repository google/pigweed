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
"""Decoder class for decoding bytes using HDLC protocol"""

import enum
import logging
from typing import Iterator, Optional
import zlib

from pw_hdlc import protocol

_LOG = logging.getLogger('pw_hdlc')

NO_ADDRESS = -1
_MIN_FRAME_SIZE = 6  # 1 B address + 1 B control + 4 B CRC-32


class FrameStatus(enum.Enum):
    """Indicates that an error occurred."""
    OK = 'OK'
    FCS_MISMATCH = 'frame check sequence failure'
    FRAMING_ERROR = 'invalid flag or escape characters'
    BAD_ADDRESS = 'address field too long'


class Frame:
    """Represents an HDLC frame."""
    def __init__(self,
                 raw_encoded: bytes,
                 raw_decoded: bytes,
                 status: FrameStatus = FrameStatus.OK):
        """Parses fields from an HDLC frame.

        Arguments:
            raw_encoded: The complete HDLC-encoded frame, excluding HDLC flag
                characters.
            raw_decoded: The complete decoded frame (address, control,
                information, FCS).
            status: Whether parsing the frame succeeded.
        """
        self.raw_encoded = raw_encoded
        self.raw_decoded = raw_decoded
        self.status = status

        self.address: int = NO_ADDRESS
        self.control: bytes = b''
        self.data: bytes = b''

        if status == FrameStatus.OK:
            address, address_length = protocol.decode_address(raw_decoded)
            if address_length == 0:
                self.status = FrameStatus.BAD_ADDRESS
                return

            self.address = address
            self.control = raw_decoded[address_length:address_length + 1]
            self.data = raw_decoded[address_length + 1:-4]

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
        self._decoded_data = bytearray()
        self._raw_data = bytearray()
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
                             frame.status.value, len(frame.raw_encoded))
                _LOG.debug('Discarded data: %s', frame.raw_encoded)

    def _finish_frame(self, status: FrameStatus) -> Frame:
        frame = Frame(bytes(self._raw_data), bytes(self._decoded_data), status)
        self._raw_data.clear()
        self._decoded_data.clear()
        return frame

    def _process_byte(self, byte: int) -> Optional[Frame]:
        frame: Optional[Frame] = None

        # Record every byte except the flag character.
        if byte != protocol.FLAG:
            self._raw_data.append(byte)

        if self._state is _State.INTERFRAME:
            if byte == protocol.FLAG:
                if self._raw_data:
                    frame = self._finish_frame(FrameStatus.FRAMING_ERROR)

                self._state = _State.FRAME
        elif self._state is _State.FRAME:
            if byte == protocol.FLAG:
                if self._raw_data:
                    frame = self._finish_frame(_check_frame(
                        self._decoded_data))

                self._state = _State.FRAME
            elif byte == protocol.ESCAPE:
                self._state = _State.FRAME_ESCAPE
            else:
                self._decoded_data.append(byte)
        elif self._state is _State.FRAME_ESCAPE:
            if byte == protocol.FLAG:
                frame = self._finish_frame(FrameStatus.FRAMING_ERROR)
                self._state = _State.FRAME
            elif byte in protocol.VALID_ESCAPED_BYTES:
                self._state = _State.FRAME
                self._decoded_data.append(protocol.escape(byte))
            else:
                self._state = _State.INTERFRAME
        else:
            raise AssertionError(f'Invalid decoder state: {self._state}')

        return frame
