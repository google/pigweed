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
import os
from pathlib import Path
import platform
import shutil
import subprocess
from typing import Dict, Iterable, Optional

from ctypes.util import find_library as ctypes_find_library
import serial.tools.list_ports
import usb  # type: ignore
from usb.backend import libusb1  # type: ignore

import pw_cli.log
from pw_cli.env import pigweed_environment

_LOG = logging.getLogger('pi_pico_detector')

# Vendor/device ID to search for in USB devices.
_RASPBERRY_PI_VENDOR_ID = 0x2E8A
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
    raise RuntimeError(f'Unsupported platform.system(): {platform.system()}')


def custom_find_library(name: str) -> Optional[str]:
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

    serial_port: str
    bus: int
    port: int

    # As a board is flashed and reset, the USB address can change. This method
    # uses the USB bus and port to try and find the desired device. Using the
    # serial number sounds appealing, but unfortunately the application's serial
    # number is different from the bootloader's.
    def address(self) -> int:
        for device in libusb_raspberry_pi_devices():
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
    devices = libusb_raspberry_pi_devices()

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


def detect_boards() -> list[BoardInfo]:
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
