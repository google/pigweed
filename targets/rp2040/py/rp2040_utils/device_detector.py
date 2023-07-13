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
"""Detects attached Raspberry Pi Pico boards."""

from dataclasses import dataclass
import logging
from typing import Dict, List

import serial.tools.list_ports  # type: ignore
import usb  # type: ignore

import pw_cli.log

# Vendor/device ID to search for in USB devices.
_RASPBERRY_PI_VENDOR_ID = 0x2E8A
_PICO_USB_SERIAL_DEVICE_ID = 0x000A
_PICO_BOOTLOADER_DEVICE_ID = 0x0003
_PICO_DEVICE_IDS = (
    _PICO_USB_SERIAL_DEVICE_ID,
    _PICO_BOOTLOADER_DEVICE_ID,
)

_LOG = logging.getLogger('pi_pico_detector')


@dataclass
class BoardInfo:
    """Information about a connected Pi Pico board."""

    serial_port: str
    bus: int
    port: int

    # As a board is flashed and reset, the USB address can change. This method
    # uses the USB bus and port to try and find the desired device. Using the
    # serial number sounds appealing, but unfortunately the application's serial
    # number is different from the bootloader's.
    def address(self) -> int:
        devices = usb.core.find(
            find_all=True,
            idVendor=_RASPBERRY_PI_VENDOR_ID,
        )
        for device in devices:
            if device.idProduct not in _PICO_DEVICE_IDS:
                raise ValueError(
                    'Unknown device type on bus %d port %d'
                    % (self.bus, self.port)
                )
            if device.port_number == self.port:
                return device.address
        raise ValueError(
            (
                'No Pico found, it may have been disconnected or flashed with '
                'an incompatible application'
            )
        )


@dataclass
class _BoardSerialInfo:
    """Information that ties a serial number to a serial com port."""

    serial_port: str
    serial_number: str


@dataclass
class _BoardUsbInfo:
    """Information that ties a serial number to a USB information"""

    serial_number: str
    bus: int
    port: int


def _detect_pico_usb_info() -> Dict[str, _BoardUsbInfo]:
    """Finds Raspberry Pi Pico devices and retrieves USB info for each one."""
    boards: Dict[str, _BoardUsbInfo] = {}
    devices = usb.core.find(
        find_all=True,
        idVendor=_RASPBERRY_PI_VENDOR_ID,
    )

    if not devices:
        return boards

    for device in devices:
        if device.idProduct == _PICO_USB_SERIAL_DEVICE_ID:
            boards[device.serial_number] = _BoardUsbInfo(
                serial_number=device.serial_number,
                bus=device.bus,
                port=device.port_number,
            )
        elif device.idProduct == _PICO_BOOTLOADER_DEVICE_ID:
            _LOG.error(
                'Found a Pi Pico in bootloader mode on bus %d address %d',
                device.bus,
                device.address,
            )
            _LOG.error(
                (
                    'Please flash and reboot the Pico into an application '
                    'utilizing USB serial to properly detect it'
                )
            )

        else:
            _LOG.error('Unknown/incompatible Raspberry Pi detected')
            _LOG.error(
                (
                    'Make sure your Pi Pico is running an application '
                    'utilizing USB serial'
                )
            )
    return boards


def _detect_pico_serial_ports() -> Dict[str, _BoardSerialInfo]:
    """Finds the serial com port associated with each Raspberry Pi Pico."""
    boards = {}
    all_devs = serial.tools.list_ports.comports()
    for dev in all_devs:
        if (
            dev.vid == _RASPBERRY_PI_VENDOR_ID
            and dev.pid == _PICO_USB_SERIAL_DEVICE_ID
        ):
            if dev.serial_number is None:
                raise ValueError('Found pico with no serial number')
            boards[dev.serial_number] = _BoardSerialInfo(
                serial_port=dev.device,
                serial_number=dev.serial_number,
            )
    return boards


def detect_boards() -> List[BoardInfo]:
    """Detects attached Raspberry Pi Pico boards in USB serial mode.

    Returns:
      A list of all found boards as BoardInfo objects.
    """
    serial_devices = _detect_pico_serial_ports()
    pico_usb_info = _detect_pico_usb_info()
    boards = []
    for serial_number, usb_info in pico_usb_info.items():
        if serial_number in serial_devices:
            serial_info = serial_devices[serial_number]
            boards.append(
                BoardInfo(
                    serial_port=serial_info.serial_port,
                    bus=usb_info.bus,
                    port=usb_info.port,
                )
            )
    return boards


def main():
    """Detects and then prints all attached Raspberry Pi Picos."""
    pw_cli.log.install()

    boards = detect_boards()
    if not boards:
        _LOG.info('No attached boards detected')
    for idx, board in enumerate(boards):
        _LOG.info('Board %d:', idx)
        _LOG.info('  %s', board)


if __name__ == '__main__':
    main()
