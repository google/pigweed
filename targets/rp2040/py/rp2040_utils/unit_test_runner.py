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
"""This script flashes and runs unit tests on Raspberry Pi Pico boards."""

import argparse
import logging
import subprocess
import sys
import time
from pathlib import Path

import serial  # type: ignore

import pw_cli.log
from pw_unit_test import serial_test_runner
from pw_unit_test.serial_test_runner import (
    SerialTestingDevice,
    DeviceNotFound,
)
from rp2040_utils import device_detector
from rp2040_utils.device_detector import PicoBoardInfo, PicoDebugProbeBoardInfo


_LOG = logging.getLogger()


def parse_args():
    """Parses command-line arguments."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        'binary', type=Path, help='The target test binary to run'
    )
    parser.add_argument(
        '--usb-bus',
        type=int,
        help='The bus this Pi Pico is on',
    )
    parser.add_argument(
        '--usb-port',
        type=str,
        help=(
            'The port chain as a colon-separated list of integers of this Pi '
            'Pico on the specified USB bus (e.g. 1:4:2:2)'
        ),
    )
    parser.add_argument(
        '-b',
        '--baud',
        type=int,
        default=115200,
        help='Baud rate to use for serial communication with target device',
    )
    parser.add_argument(
        '--test-timeout',
        type=float,
        default=5.0,
        help='Maximum communication delay in seconds before a '
        'test is considered unresponsive and aborted',
    )
    parser.add_argument(
        '--debug-probe-only',
        action='store_true',
        help='Only run tests on detected Pi Pico debug probes',
    )
    parser.add_argument(
        '--pico-only',
        action='store_true',
        help='Only run tests on detected Pi Pico boards',
    )
    parser.add_argument(
        '--verbose',
        '-v',
        dest='verbose',
        action='store_true',
        help='Output additional logs as the script runs',
    )

    return parser.parse_args()


class PiPicoTestingDevice(SerialTestingDevice):
    """A SerialTestingDevice implementation for the Pi Pico."""

    def __init__(
        self,
        board_info: PicoBoardInfo | PicoDebugProbeBoardInfo,
        baud_rate=115200,
    ):
        self._board_info = board_info
        self._baud_rate = baud_rate

    @staticmethod
    def _find_elf(binary: Path) -> Path | None:
        """Attempt to find and return the path to an ELF file for a binary.

        Args:
          binary: A relative path to a binary.

        Returns the path to the associated ELF file, or None if none was found.
        """
        if binary.suffix == '.elf' or not binary.suffix:
            return binary
        choices = (
            binary.parent / f'{binary.stem}.elf',
            binary.parent / 'bin' / f'{binary.stem}.elf',
            binary.parent / 'test' / f'{binary.stem}.elf',
        )
        for choice in choices:
            if choice.is_file():
                return choice

        _LOG.error(
            'Cannot find ELF file to use as a token database for binary: %s',
            binary,
        )
        return None

    def load_binary(self, binary: Path) -> bool:
        """Flash a binary to this device, returning success or failure."""
        if self._board_info.is_debug_probe():
            return self.load_debugprobe_binary(binary)
        return self.load_picotool_binary(binary)

    def load_debugprobe_binary(self, binary: Path) -> bool:
        """Flash a binary to this device using a debug probe, returning success
        or failure."""
        elf_path = self._find_elf(binary)
        if not elf_path:
            return False

        if not isinstance(self._board_info, PicoDebugProbeBoardInfo):
            return False

        # `probe-rs` takes a `--probe` argument of the form:
        #  <vendor_id>:<product_id>:<serial_number>
        probe = "{:04x}:{:04x}:{}".format(
            self._board_info.vendor_id(),
            self._board_info.device_id(),
            self._board_info.serial_number,
        )

        download_cmd = (
            'probe-rs',
            'download',
            '--probe',
            probe,
            '--chip',
            'RP2040',
            '--speed',
            '10000',
            str(elf_path),
        )
        _LOG.debug('Flashing ==> %s', ' '.join(download_cmd))
        process = subprocess.run(
            download_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT
        )
        if process.returncode:
            err = (
                'Flashing command failed: ' + ' '.join(download_cmd),
                str(self._board_info),
                process.stdout.decode('utf-8', errors='replace'),
            )
            _LOG.error('\n\n'.join(err))
            return False

        reset_cmd = (
            'probe-rs',
            'reset',
            '--probe',
            probe,
            '--chip',
            'RP2040',
        )
        _LOG.debug('Resetting ==> %s', ' '.join(reset_cmd))
        process = subprocess.run(
            reset_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT
        )
        if process.returncode:
            err = (
                'Resetting command failed: ' + ' '.join(reset_cmd),
                str(self._board_info),
                process.stdout.decode('utf-8', errors='replace'),
            )
            _LOG.error('\n\n'.join(err))
            return False

        # Give time for the device to reset.  Ideally the common unit test
        # runner would wait for input but this is not the case.
        time.sleep(0.5)

        return True

    def load_picotool_binary(self, binary: Path) -> bool:
        """Flash a binary to this device using picotool, returning success or
        failure."""
        cmd = (
            'picotool',
            'load',
            '-x',
            str(binary),
            '--bus',
            str(self._board_info.bus),
            '--address',
            str(self._board_info.address()),
            '-F',
        )
        _LOG.debug('Flashing ==> %s', ' '.join(cmd))
        process = subprocess.run(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT
        )
        if process.returncode:
            err = (
                'Flashing command failed: ' + ' '.join(cmd),
                str(self._board_info),
                process.stdout.decode('utf-8', errors='replace'),
            )
            _LOG.error('\n\n'.join(err))
            return False

        start_time = time.monotonic()
        load_binary_timeout = 10.0
        # Wait for serial port to enumerate. This will retry forever.
        while time.monotonic() - start_time < load_binary_timeout:
            # If the serial port path isn't known, watch for a newly
            # enmerated path.
            if not self.serial_port() or self.serial_port() == 'None':
                # Wait a bit before checking for a new port.
                time.sleep(0.3)
                # Check for updated serial port path.
                for device in device_detector.detect_boards():
                    if (
                        device.bus == self._board_info.bus
                        and device.port == self._board_info.port
                    ):
                        self._board_info.serial_port = device.serial_port
                        # Serial port found, break out of device for loop.
                        break

            # Serial port known try to connect to it.
            if self.serial_port():
                # Connect to the serial port.
                try:
                    serial.Serial(
                        baudrate=self.baud_rate(), port=self.serial_port()
                    )
                    return True
                except serial.serialutil.SerialException:
                    # Unable to connect, try again.
                    _LOG.debug(
                        'Unable to connect to %s, retrying', self.serial_port()
                    )
                    time.sleep(0.1)

        _LOG.error(
            'Binary flashed but unable to connect to the serial port: %s',
            self.serial_port(),
        )
        return False

    def serial_port(self) -> str:
        if not self._board_info.serial_port:
            return 'None'
        return self._board_info.serial_port

    def baud_rate(self) -> int:
        return self._baud_rate


def _run_test(
    device: PiPicoTestingDevice, binary: Path, test_timeout: float
) -> bool:
    return serial_test_runner.run_device_test(device, binary, test_timeout)


def run_device_test(
    binary: Path,
    test_timeout: float,
    baud_rate: int,
    usb_bus: int,
    usb_port: str,
) -> bool:
    """Flashes, runs, and checks an on-device test binary.

    Returns true on test pass.
    """
    board = device_detector.board_from_usb_port(usb_bus, usb_port)
    return _run_test(
        PiPicoTestingDevice(board, baud_rate), binary, test_timeout
    )


def detect_and_run_test(
    binary: Path,
    test_timeout: float,
    baud_rate: int,
    include_picos: bool = True,
    include_debug_probes: bool = True,
):
    _LOG.debug('Attempting to automatically detect dev board')
    boards = device_detector.detect_boards(
        include_picos=include_picos,
        include_debug_probes=include_debug_probes,
    )
    if not boards:
        error = 'Could not find an attached device'
        _LOG.error(error)
        raise DeviceNotFound(error)
    return _run_test(
        PiPicoTestingDevice(boards[0], baud_rate), binary, test_timeout
    )


def main():
    """Set up runner, and then flash/run device test."""
    args = parse_args()
    test_logfile = args.binary.with_suffix(args.binary.suffix + '.test_log.txt')
    # Truncate existing logfile.
    test_logfile.write_text('', encoding='utf-8')
    pw_cli.log.install(
        level=logging.DEBUG if args.verbose else logging.INFO,
        debug_log=test_logfile,
    )
    _LOG.debug('Logging results to %s', test_logfile)

    if args.pico_only and args.debug_probe_only:
        _LOG.critical('Cannot specify both --pico-only and --debug-probe-only')
        sys.exit(1)

    # For now, require manual configurations to be fully specified.
    if (args.usb_port is not None or args.usb_bus is not None) and not (
        args.usb_port is not None and args.usb_bus is not None
    ):
        _LOG.critical(
            'Must specify BOTH --usb-bus and --usb-port when manually '
            'specifying a device'
        )
        sys.exit(1)

    test_passed = False
    if not args.usb_bus:
        test_passed = detect_and_run_test(
            args.binary,
            args.test_timeout,
            args.baud,
            not args.debug_probe_only,
            not args.pico_only,
        )
    else:
        test_passed = run_device_test(
            args.binary,
            args.test_timeout,
            args.baud,
            args.usb_bus,
            args.usb_port,
        )

        sys.exit(0 if test_passed else 1)


if __name__ == '__main__':
    main()
