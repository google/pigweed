#!/usr/bin/env python3
# Copyright 2019 The Pigweed Authors
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
"""Detects attached stm32f429i-disc1 boards connected via mini usb."""

import glob
import logging
import subprocess
import sys
import typing

import coloredlogs

# Root for linux USB devices.
_LINUX_USB_ROOT = '/sys/bus/usb/devices'

# Vendor/device ID to search for in USB devices.
_ST_VENDOR_ID = '0483'
_DISCOVERY_MODEL_ID = '374b'

_LOG = logging.getLogger('unit_test_runner')


class BoardInfo(typing.NamedTuple):
    """Information about a connected dev board."""
    dev_name: str
    serial_number: str


def _linux_list_com_ports() -> list:
    """Linux helper to list tty devices."""
    ports = []
    for bus in glob.glob(f'{_LINUX_USB_ROOT}/usb*/'):
        cmd = ['find', bus, '-wholename', '*/tty/tty*/dev']
        output = subprocess.run(cmd,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT)
        if output.returncode:
            _LOG.info(output.stdout.decode('utf-8', errors='replace'))
            _LOG.error('Failed to find com ports')
            continue

        ports.extend(
            output.stdout.decode('utf-8', errors='replace').splitlines())
    for idx, path in enumerate(ports):
        ports[idx] = path[:path.rfind('/dev')]
    return ports


def _linux_get_device_props(devpath) -> dict:
    """Linux helper for fetching detecting device properties as a dictionary."""
    cmd = ['udevadm', 'info', '-p', f'{devpath}', '-q', 'property']
    output = subprocess.run(cmd,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)

    if output.returncode:
        _LOG.info(output.stdout.decode('utf-8', errors='replace'))
        _LOG.error('Failed to get info for devpath=%s', devpath)
        return dict()

    output_lines = output.stdout.decode('utf-8', errors='replace').splitlines()
    return dict((prop.split('=') for prop in output_lines))


def detect_boards_linux() -> list:
    """Linux-specific implementation for detecting stm32f429i-disc1 boards."""
    boards_found = []
    for path in _linux_list_com_ports():
        dev_props = _linux_get_device_props(path)
        if (dev_props['ID_VENDOR_ID'] == _ST_VENDOR_ID
                and dev_props['ID_MODEL_ID'] == _DISCOVERY_MODEL_ID):
            boards_found.append(
                BoardInfo(dev_name=dev_props['DEVNAME'],
                          serial_number=dev_props['ID_SERIAL_SHORT']))
    return boards_found


def detect_boards() -> list:
    """Detect attached boards, returning a list of Board objects."""
    if sys.platform.startswith('linux'):
        return detect_boards_linux()

    _LOG.error('Unsupported OS %s, cannot detect '
               'attached boards', sys.platform)
    return []


def main(args=None):
    """This detects and then displays all attached discovery boards."""

    # Try to use pw_cli logs, else default to something reasonable.
    try:
        import pw_cli.log  # pylint: disable=import-outside-toplevel
        pw_cli.log.install()
    except ImportError:
        coloredlogs.install(level='DEBUG' if args.verbose else 'INFO',
                            level_styles={
                                'debug': {
                                    'color': 244
                                },
                                'error': {
                                    'color': 'red'
                                }
                            },
                            fmt='%(asctime)s %(levelname)s | %(message)s')
    boards = detect_boards()
    if not boards:
        _LOG.info('No attached boards detected')
    for idx, board in enumerate(boards):
        _LOG.info('Board %d:', idx)
        _LOG.info('  - Port: %s', board.dev_name)
        _LOG.info('  - Serial #: %s', board.serial_number)


if __name__ == '__main__':
    main()
