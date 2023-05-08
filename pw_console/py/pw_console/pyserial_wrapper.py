# Copyright 2021 The Pigweed Authors
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
"""Wrapers for pyserial classes to log read and write data."""
from __future__ import annotations

from typing import TYPE_CHECKING

import serial

from pw_console.plugins.bandwidth_toolbar import SerialBandwidthTracker

if TYPE_CHECKING:
    from _typeshed import ReadableBuffer


class SerialWithLogging(serial.Serial):  # pylint: disable=too-many-ancestors
    """pyserial with read and write wrappers for logging."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._bandwidth_tracker = SerialBandwidthTracker()

    def read(self, size: int = 1) -> bytes:
        data = super().read(size)
        self._bandwidth_tracker.track_read_data(data)
        return data

    def write(self, data: ReadableBuffer) -> None:
        self._bandwidth_tracker.track_write_data(data)
        super().write(data)
