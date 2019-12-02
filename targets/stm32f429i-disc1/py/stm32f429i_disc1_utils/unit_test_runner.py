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

import argparse
import logging
import os
import serial
import subprocess
import sys

import coloredlogs

# Path used to access non-python resources in this python module.
_DIR = os.path.dirname(__file__)

# Path to default openocd configuration file.
_OPENOCD_CONFIG = os.path.join(_DIR, 'openocd_stm32f4xx.cfg')

_LOG = logging.getLogger('unit_test_runner')

# Verification of test pass/failure depends on these strings. If the formatting
# or output of the simple_printing_event_handler changes, this may need to be
# updated.
_TESTS_STARTING_STRING = b'[==========] Running all tests.'
_TESTS_DONE_STRING = b'[==========] Done running all tests.'
_TEST_FAILURE_STRING = b'[  FAILED  ]'


class TestingFailure(Exception):
    """A simple exception to be raised when a testing step fails."""


def parse_args():
    """Parses command-line arguments."""

    parser = argparse.ArgumentParser(
        'Flashes and then runs on-device unit tests')
    parser.add_argument('binary', help='The target test binary to run')
    parser.add_argument('--openocd-config',
                        default=_OPENOCD_CONFIG,
                        help='Path to openocd configuration file')
    parser.add_argument('--port',
                        required=True,
                        help='The name of the serial port to connect to when '
                        'running tests')
    parser.add_argument('--baud',
                        type=int,
                        default=115200,
                        help='Target baud rate to use for serial communication'
                        ' with target device')
    parser.add_argument('--test-timeout',
                        type=float,
                        default=2.0,
                        help='Maximum communication delay in seconds before a '
                        'test is considered unresponsive and aborted')
    parser.add_argument('--verbose',
                        '-v',
                        dest='verbose',
                        action="store_true",
                        help='Output additional logs as the script runs')

    return parser.parse_args()


def reset_device(openocd_config):
    """Uses openocd to reset the attached device."""

    # Name/path of openocd.
    default_flasher = 'openocd'
    flash_tool = os.getenv('OPENOCD_PATH', default_flasher)

    cmd = [
        flash_tool, '-f', openocd_config, '-c', 'init', '-c', 'reset run',
        '-c', 'exit'
    ]
    _LOG.debug('Resetting device...')

    process = subprocess.run(cmd,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT)
    if process.returncode:
        _LOG.error(f'\n{process.stdout.decode("utf-8", errors="replace")}')
        raise TestingFailure('Failed to reset target device.')
    else:
        _LOG.debug(f'\n{process.stdout.decode("utf-8", errors="replace")}')

    _LOG.debug('Successfully reset device!')


def read_serial(openocd_config, port, baud_rate, test_timeout) -> bytes:
    """Reads lines from a serial port until a line read times out.

    Returns bytes object containing the read serial data.
    """

    serial_data = bytearray()
    device = serial.Serial(baudrate=baud_rate, port=port, timeout=test_timeout)
    if not device.is_open:
        raise TestingFailure('Failed to open device.')

    # Flush input buffer and reset the device to begin the test.
    device.reset_input_buffer()
    reset_device(openocd_config)

    # Block and wait for the first byte.
    serial_data += device.read()
    if not serial_data:
        raise TestingFailure('Device not producing output. :(')

    # Read with a reasonable timeout until we stop getting characters.
    while True:
        bytes_read = device.readline()
        if not bytes_read:
            break
        serial_data += bytes_read
        if serial_data.rfind(_TESTS_DONE_STRING) != -1:
            # Set to much more aggressive timeout since the last one or two
            # lines should print out immediately. (one line if all fails or all
            # passes, two lines if mixed.)
            device.timeout = 0.01

    # Remove carriage returns.
    serial_data = serial_data.replace(b'\r', b'')

    # Try to trim captured results to only contain most recent test run.
    test_start_index = serial_data.rfind(_TESTS_STARTING_STRING)
    return serial_data if test_start_index == -1 else serial_data[
        test_start_index:]


def flash_device(binary, openocd_config):
    """Flash binary to a connected device using the provided configuration."""

    # Name/path of openocd.
    default_flasher = 'openocd'
    flash_tool = os.getenv('OPENOCD_PATH', default_flasher)

    openocd_command = ' '.join(['program', binary, 'reset', 'exit'])
    cmd = [flash_tool, '-f', openocd_config, '-c', openocd_command]
    _LOG.info('Flashing firmware to device...')

    process = subprocess.run(cmd,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT)
    if process.returncode:
        _LOG.error(f'\n{process.stdout.decode("utf-8", errors="replace")}')
        raise TestingFailure('Failed to flash target device.')
    else:
        _LOG.debug(f'\n{process.stdout.decode("utf-8", errors="replace")}')

    _LOG.info('Successfully flashed firmware to device!')


def handle_test_results(test_output):
    """Parses test output to determine whether tests passed or failed."""

    if test_output.find(_TESTS_STARTING_STRING) == -1:
        raise TestingFailure('Failed to find test start.')

    if test_output.rfind(_TESTS_DONE_STRING) == -1:
        _LOG.info(f'\n{test_output.decode("utf-8", errors="replace")}')
        raise TestingFailure('Tests did not complete.')

    if test_output.rfind(_TEST_FAILURE_STRING) != -1:
        _LOG.info(f'\n{test_output.decode("utf-8", errors="replace")}')
        raise TestingFailure('Test suite had one or more failures.')

    # TODO(amontanez): Do line-by-line logging of captured output.
    _LOG.debug(f'\n{test_output.decode("utf-8", errors="replace")}')

    _LOG.info('Test passed!')


def run_device_test(binary, test_timeout, openocd_config, baud,
                    port=None) -> bool:
    """Flashes, runs, and checks an on-device test binary.

    Returns true on test pass.
    """

    _LOG.debug('Launching test binary {}'.format(binary))
    try:
        flash_device(binary, openocd_config)
        serial_data = read_serial(openocd_config, port, baud, test_timeout)
        handle_test_results(serial_data)
    except TestingFailure as err:
        _LOG.error(err)
        return False

    return True


def main(args=None):
    """Set up runner, and then flash/run device test."""
    args = parse_args()

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

    if run_device_test(args.binary, args.test_timeout, args.openocd_config,
                       args.baud, args.port):
        sys.exit(0)
    else:
        sys.exit(1)


if __name__ == '__main__':
    main()
