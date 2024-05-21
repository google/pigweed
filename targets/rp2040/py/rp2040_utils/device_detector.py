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

from dataclasses import asdict, dataclass
import logging
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
from typing import Iterable, Optional

from ctypes.util import find_library as ctypes_find_library
import serial.tools.list_ports
import usb  # type: ignore
from usb.backend import libusb1  # type: ignore

import pw_cli.log
from pw_cli.env import pigweed_environment

_LOG = logging.getLogger('pi_pico_detector')

# Vendor/device ID to search for in USB devices.
_RASPBERRY_PI_VENDOR_ID = 0x2E8A

# RP2040 based debug probes:
_RASPBERRY_PI_DEBUGPROBE_DEVICE_IDS = (
    # RaspberryPi Debug probe: https://github.com/raspberrypi/debugprobe
    0x000C,
    # RaspberryPi Legacy Picoprobe (early Debug probe version)
    0x0004,
)

_PICO_USB_SERIAL_DEVICE_ID = 0x000A
_PICO_BOOTLOADER_DEVICE_ID = 0x0003
_PICO_DEVICE_IDS = (
    _PICO_USB_SERIAL_DEVICE_ID,
    _PICO_BOOTLOADER_DEVICE_ID,
)

_LIBUSB_CIPD_INSTALL_ENV_VAR = 'PW_PIGWEED_CIPD_INSTALL_DIR'
_LIBUSB_CIPD_SUBDIR = 'libexec'

if platform.system() == 'Linux':
    _LIB_SUFFIX = '.so'
elif platform.system() == 'Darwin':
    _LIB_SUFFIX = '.dylib'
elif platform.system() == 'Windows':
    _LIB_SUFFIX = '.dll'
else:
    _LOG.error('Unsupported platform.system(): %s', platform.system())
    sys.exit(1)


def custom_find_library(name: str) -> str | None:
    """Search for shared libraries in non-standard locations."""
    search_paths: list[Path] = []

    # Add to search_paths starting with lowest priority locations.

    if platform.system() == 'Darwin':
        # libusb from homebrew
        homebrew_prefix = os.environ.get('HOMEBREW_PREFIX', '')
        if homebrew_prefix:
            homebrew_lib = Path(homebrew_prefix) / 'lib'
            homebrew_lib = homebrew_lib.expanduser().resolve()
            if homebrew_lib.is_dir():
                search_paths.append(homebrew_lib)

    # libusb from pkg-config
    pkg_config_bin = shutil.which('pkg-config')
    if pkg_config_bin:
        # pkg-config often prefixes libraries with 'lib', check both.
        for pkg_name in [f'lib{name}', name]:
            pkg_config_command = [pkg_config_bin, '--variable=libdir', pkg_name]
            process = subprocess.run(
                pkg_config_command,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
            if process.returncode == 0:
                pkg_config_libdir = Path(
                    process.stdout.decode('utf-8', errors='ignore').strip()
                )
                if pkg_config_libdir.is_dir():
                    search_paths.append(pkg_config_libdir)
                    break

    # libusb provided by CIPD:
    pw_env = pigweed_environment()
    if _LIBUSB_CIPD_INSTALL_ENV_VAR in pw_env:
        cipd_lib = (
            Path(getattr(pw_env, _LIBUSB_CIPD_INSTALL_ENV_VAR))
            / _LIBUSB_CIPD_SUBDIR
        )
        if cipd_lib.is_dir():
            search_paths.append(cipd_lib)

    _LOG.debug('Potential shared library search paths:')
    for path in search_paths:
        _LOG.debug(path)

    # Search for shared libraries in search_paths
    for libdir in reversed(search_paths):
        lib_results = sorted(
            str(lib.resolve())
            for lib in libdir.iterdir()
            if name in lib.name and _LIB_SUFFIX in lib.suffixes
        )
        if lib_results:
            _LOG.info('Using %s located at: %s', name, lib_results[-1])
            # Return the highest lexigraphically sorted lib version
            return lib_results[-1]

    # Fallback to pyusb default of calling ctypes.util.find_library.
    return ctypes_find_library(name)


def libusb_raspberry_pi_devices() -> Iterable[usb.core.Device]:
    return usb.core.find(
        find_all=True,
        idVendor=_RASPBERRY_PI_VENDOR_ID,
        backend=libusb1.get_backend(find_library=custom_find_library),
    )


@dataclass
class BoardInfo:
    """Information about a connected Pi Pico board."""

    bus: int
    port: int
    serial_port: Optional[str] = None

    # As a board is flashed and reset, the USB address can change. This method
    # uses the USB bus and port to try and find the desired device. Using the
    # serial number sounds appealing, but unfortunately the application's serial
    # number is different from the bootloader's.
    def address(self) -> int:
        for device in libusb_raspberry_pi_devices():
            if device.idProduct in _RASPBERRY_PI_DEBUGPROBE_DEVICE_IDS:
                continue
            if device.idProduct not in _PICO_DEVICE_IDS:
                _LOG.error(
                    'Unknown device type on bus %s port %s', self.bus, self.port
                )
            if device.port_number == self.port:
                return device.address
        _LOG.error(
            'No Pico found, it may have been disconnected or flashed with '
            'an incompatible application'
        )
        sys.exit(1)


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
    product: str
    manufacturer: str
    vendor_id: int
    product_id: int

    @property
    def in_bootloader_mode(self) -> bool:
        return self.product_id == _PICO_BOOTLOADER_DEVICE_ID

    @property
    def in_usb_serial_mode(self) -> bool:
        return self.product_id == _PICO_USB_SERIAL_DEVICE_ID

    @property
    def is_debug_probe(self) -> bool:
        return self.product_id in _RASPBERRY_PI_DEBUGPROBE_DEVICE_IDS

    def __repr__(self) -> str:
        return repr(asdict(self))


def _detect_pico_usb_info() -> dict[str, _BoardUsbInfo]:
    """Finds Raspberry Pi Pico devices and retrieves USB info for each one."""
    boards: dict[str, _BoardUsbInfo] = {}
    devices = libusb_raspberry_pi_devices()

    if not devices:
        return boards

    _LOG.debug('==> Detecting Raspberry Pi devices')
    for device in devices:
        try:
            serial_number = device.serial_number
        except ValueError as e:
            _LOG.warning(
                '  --> A connected device has an inaccessible '
                'serial number: %s',
                e,
            )
            continue

        board_usb_info = _BoardUsbInfo(
            serial_number=serial_number,
            bus=device.bus,
            port=device.port_number,
            product=device.product,
            manufacturer=device.manufacturer,
            vendor_id=device.idVendor,
            product_id=device.idProduct,
        )

        if board_usb_info.in_usb_serial_mode:
            boards[serial_number] = board_usb_info
            _LOG.debug(
                '  --> Found a Pi Pico in USB serial mode: %s', board_usb_info
            )

        elif board_usb_info.in_bootloader_mode:
            boards[serial_number] = board_usb_info
            _LOG.debug(
                '  --> Found a Pi Pico in bootloader mode: %s', board_usb_info
            )

        elif board_usb_info.is_debug_probe:
            _LOG.debug(
                '  --> Found Raspberry Pi debug probe: %s', board_usb_info
            )

        else:
            _LOG.warning(
                '  --> Found unknown/incompatible Raspberry Pi: %s',
                board_usb_info,
            )

    return boards


def _detect_pico_serial_ports() -> dict[str, _BoardSerialInfo]:
    """Finds the serial com port associated with each Raspberry Pi Pico."""
    boards = {}
    all_devs = serial.tools.list_ports.comports()
    for dev in all_devs:
        if (
            dev.vid == _RASPBERRY_PI_VENDOR_ID
            and dev.pid == _PICO_USB_SERIAL_DEVICE_ID
        ):
            serial_number = dev.serial_number
            if serial_number is None:
                _LOG.error('Found pico with no serial number')
                continue
            boards[serial_number] = _BoardSerialInfo(
                serial_port=dev.device,
                serial_number=serial_number,
            )
    return boards


def detect_boards() -> list[BoardInfo]:
    """Detects attached Raspberry Pi Pico boards in USB serial mode.

    Returns:
      A list of all found boards as BoardInfo objects.
    """
    serial_devices = _detect_pico_serial_ports()
    pico_usb_info = _detect_pico_usb_info()
    boards = []
    for serial_number, usb_info in pico_usb_info.items():
        serial_port = None
        if serial_number in serial_devices:
            serial_port = serial_devices[serial_number].serial_port

        if serial_port or usb_info.in_bootloader_mode:
            boards.append(
                BoardInfo(
                    bus=usb_info.bus,
                    port=usb_info.port,
                    serial_port=serial_port,
                )
            )

    return boards


def main():
    """Detects and then prints all attached Raspberry Pi Picos."""
    pw_cli.log.install(
        level=logging.DEBUG, use_color=True, hide_timestamp=False
    )

    boards = detect_boards()
    if not boards:
        _LOG.info('No attached boards detected')
    for idx, board in enumerate(boards):
        _LOG.info('Board %d:', idx)
        _LOG.info('  %s', board)


if __name__ == '__main__':
    main()
