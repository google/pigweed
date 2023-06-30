#!/usr/bin/env python3
# Copyright 2023 The Pigweed Authors
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

r"""Used to parse a data stream containing base64 tokenized messages and
    raw text from Zephyr console.  A tokenized message will be decoded
    and sent to the log handler.  The raw text is just flushed to the log
    handler.
"""

import enum
import os

from pathlib import Path
from typing import Iterable, Optional, Callable, Union, Any

from pw_tokenizer.detokenize import AutoUpdatingDetokenizer
from pw_tokenizer import detokenize, encode


class TokenStatus(enum.Enum):
    """Indicates that an error occurred."""

    OK = 'OK'
    TOKEN_ERROR = 'invalid flag or escape characters'


class _State(enum.Enum):
    RAW_TEXT = 0
    TOKEN_STARTED = 1
    TOKEN_ENDED = 2


PREFIX = ord('$')
EOT = ord('#')

_PathOrStr = Union[Path, str]


class Token:
    """Represents an Zephyr Base64 Token."""

    def __init__(
        self,
        raw_encoded: bytes,
        raw_decoded: bytes,
        status: TokenStatus = TokenStatus.OK,
    ):
        """Parses fields from an Zephyr Token.

        Arguments:
            raw_encoded: The complete base64 message
            raw_decoded: The complete base64 token.
            status: Whether parsing the token succeeded.
        """
        self.raw_encoded = raw_encoded
        self.raw_decoded = raw_decoded
        self.status = status
        self.data: bytes = b''

        if status == TokenStatus.OK:
            self.data = raw_decoded

    def ok(self) -> bool:
        """True if this represents a valid token.

        If false, then parsing failed. The status is set to indicate what type
        of error occurred, and the data field contains all bytes parsed from the
        token.
        """
        return self.status is TokenStatus.OK

    def __repr__(self) -> str:
        if self.ok():
            body = f'data={self.data!r}'
        else:
            body = (
                f'raw_encoded={self.raw_encoded!r}, '
                f'status={str(self.status)}'
            )

        return f'{type(self).__name__}({body})'


class ZephyrTokenDecoder:
    """Decodes one or more Zephyr tokens from a stream of data."""

    def __init__(self) -> None:
        self._decoded_data = bytearray()
        self._raw_data = bytearray()
        self._state = _State.RAW_TEXT

    def process(self, data: bytes) -> Iterable[Token]:
        """Decodes and yields Zephyr tokens, including corrupt tokens.

        The ok() method on Token indicates whether it is valid or represents a
        token parsing error.

        Yields:
          Tokens, which may be valid (token.ok()) or corrupt (!token.ok())
        """
        for byte in data:
            token = self.process_byte(byte)
            if token:
                yield token

    def process_valid_tokens(self, data: bytes) -> Iterable[Token]:
        """Decodes and yields valid Zephyr tokens, logging any errors."""
        for token in self.process(data):
            if token.ok():
                yield token
            else:
                print(
                    'Failed to decode token: %s; discarded %d bytes',
                    token.status.value,
                    len(token.raw_encoded),
                )
                print('Discarded data: %s', token.raw_encoded)

    def _finish_token(self, status: TokenStatus) -> Token:
        token = Token(bytes(self._raw_data), bytes(self._decoded_data), status)
        self._raw_data.clear()
        self._decoded_data.clear()
        return token

    def process_byte(self, byte: int) -> Optional[Token]:
        """Processes a single byte and returns a token if one was completed."""
        token: Optional[Token] = None

        self._raw_data.append(byte)

        if self._state is _State.RAW_TEXT:
            if byte == PREFIX:
                self._state = _State.TOKEN_STARTED
                self._decoded_data.append(byte)
        elif self._state is _State.TOKEN_STARTED:
            if byte == EOT:
                self._state = _State.TOKEN_ENDED
                token = self._finish_token(TokenStatus.OK)
            else:
                self._decoded_data.append(byte)
        elif self._state is _State.TOKEN_ENDED:
            if byte == PREFIX:
                self._state = _State.TOKEN_STARTED
                self._decoded_data.append(byte)
            else:
                self._state = _State.RAW_TEXT
        else:
            raise AssertionError(f'Invalid decoder state: {self._state}')

        return token


class ZephyrDetokenizer:
    """Processes both base64 message and non-token data in a stream."""

    def __init__(
        self, log_handler: Callable[[bytes], Any], *paths_or_files: _PathOrStr
    ) -> None:
        """Yields valid Zephyr tokens and passes non-token data to callback."""
        self._log_handler = log_handler

        self._raw_data = bytearray()
        self._zephyr_decoder = ZephyrTokenDecoder()

        print(f'Loading Token database: {paths_or_files}')
        self.detokenizer = AutoUpdatingDetokenizer(*paths_or_files)
        self.detokenizer.show_errors = True
        self.base64_re = detokenize._base64_message_regex(b'$')

    def flush_non_frame_data(self) -> None:
        """Flushes any data in the buffer as non-token data.

        If a valid token was flushed partway, the data for the first part
        of the frame will be included both in the raw data and in the frame.
        """
        self._flush_non_frame()

    def _flush_non_frame(self, to_index: Optional[int] = None):
        if self._raw_data:
            self._log_handler(bytes(self._raw_data[:to_index]))
            del self._raw_data[:to_index]

    def decode(self, data: bytes):
        for token in self.process(data):
            if token.ok():
                print(f'token: {token.data}')
                result = detokenize.detokenize_base64(
                    self.detokenizer, token.data
                )
                print(f'result: {result.decode()}')
                self._log_handler(result)

    def process(self, data: bytes) -> Iterable[Token]:
        """Processes a stream of mixed Tokens and raw text data.

        Yields OK tokens and calls log_handler with raw text data.
        """
        for byte in data:
            yield from self._process_byte(byte)

        if self._zephyr_decoder._state is _State.RAW_TEXT:
            self._flush_non_frame()

    def _process_byte(self, byte: int) -> Iterable[Token]:
        self._raw_data.append(byte)
        token = self._zephyr_decoder.process_byte(byte)

        if token is None:
            return

        if token.ok():
            # Drop the valid token from the data. Only drop matching bytes in
            # case the token was flushed prematurely.
            for suffix_byte in reversed(token.raw_encoded):
                if not self._raw_data or self._raw_data[-1] != suffix_byte:
                    break
                self._raw_data.pop()

            self._flush_non_frame()  # Flush the raw data before the token.

            yield token
        else:
            # Don't flush a final flag byte yet because it might be the start of
            # an token.
            to_index = -1 if self._raw_data[-1] == PREFIX else None
            self._flush_non_frame(to_index)
