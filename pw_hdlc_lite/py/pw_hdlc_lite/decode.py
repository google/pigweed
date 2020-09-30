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
from typing import Iterator, NamedTuple, Optional, Tuple
import zlib

from pw_hdlc_lite import protocol

_LOG = logging.getLogger('pw_hdlc_lite')


class FrameStatus(enum.Enum):
    """Indicates that an error occurred."""
    OK = 'OK'
    FCS_MISMATCH = 'frame check sequence failure'
    INCOMPLETE = 'incomplete frame'
    INVALID_ESCAPE = 'invalid escape character'


_MIN_FRAME_SIZE = 6  # 1 B address + 1 B control + 4 B CRC-32

NO_ADDRESS = -1


class Frame(NamedTuple):
    """Represents an HDLC frame."""

    # All bytes in the frame (address, control, information, FCS)
    raw: bytes

    # Whether parsing the frame succeeded.
    status: FrameStatus = FrameStatus.OK

    @property
    def address(self) -> int:
        """The frame's address field (assumes only one byte for now)."""
        return self.raw[0] if self.raw else NO_ADDRESS

    @property
    def control(self) -> bytes:
        """The control byte (assumes only one byte for now)."""
        return self.raw[1:2] if len(self.raw) >= 2 else b''

    @property
    def data(self) -> bytes:
        """The information field in the frame."""
        return self.raw[2:-4] if len(self.raw) >= _MIN_FRAME_SIZE else b''

    def ok(self) -> bool:
        """True if this represents a valid frame.

        If false, then parsing failed. The status is set to indicate what type
        of error occurred, and the data field contains all bytes parsed from the
        frame (including bytes parsed as address or control bytes).
        """
        return self.status is FrameStatus.OK


class _BaseFrameState:
    """Base class for all frame parsing states."""
    def __init__(self, data: bytearray):
        self._data = data  # All data seen in the current frame
        self._escape_next = False

    def handle_flag(self) -> Tuple['_BaseFrameState', Optional[Frame]]:
        """Handles an HDLC flag character (0x7e).

        The HDLC flag is always interpreted as the start of a new frame.

        Returns:
            (next state, optional frame or error)
        """
        # If there is data or an escape character, the frame is incomplete.
        if self._escape_next or self._data:
            return _AddressState(), Frame(bytes(self._data),
                                          FrameStatus.INCOMPLETE)

        return _AddressState(), None

    def handle_escape(self) -> '_BaseFrameState':
        """Handles an HDLC escape character (0x7d); returns the next state."""
        if self._escape_next:
            # If two escapes occur in a row, the frame is invalid.
            return _InterframeState(self._data, FrameStatus.INVALID_ESCAPE)

        self._escape_next = True
        return self

    def handle_byte(self, byte: int) -> '_BaseFrameState':
        """Handles a byte, which may have been escaped; returns next state."""
        self._data.append(protocol.escape(byte) if self._escape_next else byte)
        self._escape_next = False
        return self


class _InterframeState(_BaseFrameState):
    """Not currently in a frame; any data is discarded."""
    def __init__(self, data: bytearray, error: FrameStatus):
        super().__init__(data)
        self._error = error

    def handle_flag(self) -> Tuple[_BaseFrameState, Optional[Frame]]:
        # If this state was entered due to an error, report that error before
        # starting a new frame.
        if self._error is not FrameStatus.OK:
            return _AddressState(), Frame(bytes(self._data), self._error)

        return super().handle_flag()


class _AddressState(_BaseFrameState):
    """First field in a frame: the address."""
    def __init__(self):
        super().__init__(bytearray())

    def handle_byte(self, byte: int) -> _BaseFrameState:
        super().handle_byte(byte)
        # Only handle single-byte addresses for now.
        return _ControlState(self._data)


class _ControlState(_BaseFrameState):
    """Second field in a frame: control."""
    def handle_byte(self, byte: int) -> _BaseFrameState:
        super().handle_byte(byte)
        # Only handle a single control byte for now.
        return _DataState(self._data)


class _DataState(_BaseFrameState):
    """The information field in a frame."""
    def handle_flag(self) -> Tuple[_BaseFrameState, Frame]:
        return _AddressState(), Frame(bytes(self._data), self._check_frame())

    def _check_frame(self) -> FrameStatus:
        # If the last character was an escape, assume bytes are missing.
        if self._escape_next or len(self._data) < _MIN_FRAME_SIZE:
            return FrameStatus.INCOMPLETE

        frame_crc = int.from_bytes(self._data[-4:], 'little')
        if zlib.crc32(self._data[:-4]) != frame_crc:
            return FrameStatus.FCS_MISMATCH

        return FrameStatus.OK


class FrameDecoder:
    """Decodes one or more HDLC frames from a stream of data."""
    def __init__(self):
        self._data = bytearray()
        self._unescape_next_byte_flag = False
        self._state = _InterframeState(bytearray(), FrameStatus.OK)

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
        if byte == protocol.FLAG:
            self._state, frame = self._state.handle_flag()
            return frame

        if byte == protocol.ESCAPE:
            self._state = self._state.handle_escape()
        else:
            self._state = self._state.handle_byte(byte)

        return None
