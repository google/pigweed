#!/usr/bin/env python3
# Copyright 2024 The Pigweed Authors
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
"""This script flashes and runs RPC unit tests on Raspberry Pi Pico boards."""

import argparse
import logging
import sys
from pathlib import Path

import serial  # type: ignore

import pw_cli.log
from pw_hdlc import rpc
from pw_log.proto import log_pb2
from pw_rpc.callback_client.errors import RpcTimeout
from pw_system import device
from pw_tokenizer import detokenize
from pw_unit_test_proto import unit_test_pb2

from rp2040_utils import device_detector
from rp2040_utils.device_detector import BoardInfo
from rp2040_utils.unit_test_runner import PiPicoTestingDevice


_LOG = logging.getLogger("unit_test_runner")


def parse_args():
    """Parse and return command-line arguments."""

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
        type=int,
        help='The port of this Pi Pico on the specified USB bus',
    )
    parser.add_argument(
        '--serial-port',
        type=str,
        help='The name of the serial port to connect to when running tests',
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
        '--verbose',
        '-v',
        dest='verbose',
        action='store_true',
        help='Output additional logs as the script runs',
    )

    return parser.parse_args()


class PiPicoRpcTestingDevice(PiPicoTestingDevice):
    """An RPC test runner implementation for the Pi Pico."""

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
            if choice.exists():
                return choice

        _LOG.error(
            'Cannot find ELF file to use as a token database for binary: %s',
            binary,
        )
        return None

    def run_device_test(self, binary: Path, timeout: float) -> bool:
        """Run an RPC unit test on this device.

        Returns whether it succeeded.
        """
        if not self.load_binary(binary):
            return False
        serial_device = serial.Serial(
            self.serial_port(), self.baud_rate(), timeout=0.1
        )
        reader = rpc.SerialReader(serial_device, 8192)
        elf_path = self._find_elf(binary)
        if not elf_path:
            return False
        with device.Device(
            channel_id=rpc.DEFAULT_CHANNEL_ID,
            reader=reader,
            write=serial_device.write,
            proto_library=[log_pb2, unit_test_pb2],
            detokenizer=detokenize.Detokenizer(elf_path),
        ) as dev:
            try:
                test_results = dev.run_tests(timeout)
            except RpcTimeout:
                _LOG.error('Test timed out after %s seconds.', timeout)
                return False
            if not test_results.all_tests_passed():
                return False
        return True


def run_device_test(
    binary: Path,
    test_timeout: float,
    serial_port: str,
    baud_rate: int,
    usb_bus: int,
    usb_port: int,
) -> bool:
    """Flash, run, and check an on-device test binary.

    Returns true on test pass.
    """
    board = BoardInfo(
        bus=usb_bus,
        port=usb_port,
        serial_port=serial_port,
    )
    return PiPicoRpcTestingDevice(board, baud_rate).run_device_test(
        binary, test_timeout
    )


def detect_and_run_test(
    binary: Path, test_timeout: float, baud_rate: int
) -> bool:
    """Detect a dev board and run a test binary on it.

    Returns whether or not the test completed successfully.
    """
    _LOG.debug('Attempting to automatically detect dev board')
    boards = device_detector.detect_boards()
    if not boards:
        _LOG.error('Could not find an attached device')
        return False
    return PiPicoRpcTestingDevice(boards[0], baud_rate).run_device_test(
        binary, test_timeout
    )


def main():
    """Set up runner and then flash/run device test."""
    args = parse_args()
    log_level = logging.DEBUG if args.verbose else logging.INFO
    pw_cli.log.install(level=log_level)

    test_passed = False
    if not args.serial_port:
        test_passed = detect_and_run_test(
            args.binary, args.test_timeout, args.baud
        )
    else:
        test_passed = run_device_test(
            args.binary,
            args.test_timeout,
            args.serial_port,
            args.baud,
            args.usb_bus,
            args.usb_port,
        )
    sys.exit(0 if test_passed else 1)


if __name__ == '__main__':
    main()
