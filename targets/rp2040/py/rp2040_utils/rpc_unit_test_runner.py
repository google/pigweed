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
from rp2040_utils.unit_test_runner import PiPicoTestingDevice, parse_args


_LOG = logging.getLogger()


class PiPicoRpcTestingDevice(PiPicoTestingDevice):
    """An RPC test runner implementation for the Pi Pico."""

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
                _LOG.info('Test run complete')
            except RpcTimeout:
                _LOG.error('Test timed out after %s seconds.', timeout)
                return False
            if not test_results.all_tests_passed():
                return False
        return True


def run_device_test(
    binary: Path,
    test_timeout: float,
    baud_rate: int,
    usb_bus: int,
    usb_port: str,
) -> bool:
    """Flash, run, and check an on-device test binary.

    Returns true on test pass.
    """
    board = device_detector.board_from_usb_port(usb_bus, usb_port)
    return PiPicoRpcTestingDevice(board, baud_rate).run_device_test(
        binary, test_timeout
    )


def detect_and_run_test(
    binary: Path,
    test_timeout: float,
    baud_rate: int,
    include_picos: bool = True,
    include_debug_probes: bool = True,
) -> bool:
    """Detect a dev board and run a test binary on it.

    Returns whether or not the test completed successfully.
    """
    _LOG.debug('Attempting to automatically detect dev board')
    boards = device_detector.detect_boards(
        include_picos=include_picos,
        include_debug_probes=include_debug_probes,
    )
    if not boards:
        _LOG.error('Could not find an attached device')
        return False
    return PiPicoRpcTestingDevice(boards[0], baud_rate).run_device_test(
        binary, test_timeout
    )


def main():
    """Set up runner and then flash/run device test."""
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
