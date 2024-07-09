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
"""Find serial ports."""

import argparse
import logging
import operator
import sys
from typing import Optional, Sequence

from serial.tools.list_ports import comports
from serial.tools.list_ports_common import ListPortInfo

from pw_cli.interactive_prompts import interactive_index_select

_LOG = logging.getLogger(__package__)


def _parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '-i',
        '--interactive',
        action='store_true',
        help='Show an interactive prompt to select a port.',
    )
    parser.add_argument(
        '-l',
        '--list-ports',
        action='store_true',
        help='List all port info.',
    )
    parser.add_argument(
        '-p',
        '--product',
        help='Print ports matching this product name.',
    )
    parser.add_argument(
        '-m',
        '--manufacturer',
        help='Print ports matching this manufacturer name.',
    )
    parser.add_argument(
        '-s',
        '--serial-number',
        help='Print ports matching this serial number.',
    )
    parser.add_argument(
        '-1',
        '--print-first-match',
        action='store_true',
        help='Print the first port found sorted by device path.',
    )
    return parser.parse_args()


def _print_ports(ports: Sequence[ListPortInfo]):
    for cp in ports:
        for line in [
            f"device        = {cp.device}",
            f"name          = {cp.name}",
            f"description   = {cp.description}",
            f"vid           = {cp.vid}",
            f"pid           = {cp.pid}",
            f"serial_number = {cp.serial_number}",
            f"location      = {cp.location}",
            f"manufacturer  = {cp.manufacturer}",
            f"product       = {cp.product}",
            f"interface     = {cp.interface}",
        ]:
            print(line)
        print()


def main(
    list_ports: bool = False,
    product: Optional[str] = None,
    manufacturer: Optional[str] = None,
    serial_number: Optional[str] = None,
    print_first_match: bool = False,
    interactive: bool = False,
) -> int:
    """List device info or print matches."""
    ports = sorted(comports(), key=operator.attrgetter('device'))

    if list_ports:
        _print_ports(ports)
        return 0

    if interactive:
        selected_port = interactive_serial_port_select()
        if selected_port:
            print(selected_port)
            return 0
        return 1

    any_match_found = False

    # Print matching devices
    for port in ports:
        if (
            product is not None
            and port.product is not None
            and product in port.product
        ):
            any_match_found = True
            print(port.device)

        if (
            manufacturer is not None
            and port.manufacturer is not None
            and manufacturer in port.manufacturer
        ):
            any_match_found = True
            print(port.device)

        if (
            serial_number is not None
            and port.serial_number is not None
            and serial_number in port.serial_number
        ):
            any_match_found = True
            print(port.device)

        if any_match_found and print_first_match:
            return 0

    if not any_match_found:
        return 1

    return 0


def interactive_serial_port_select(auto_select_only_one=True) -> str:
    """Prompt the user to select a detected serial port.

    Returns: String containing the path to the tty device.
    """
    ports = sorted(comports(), key=operator.attrgetter('device'))

    if not ports:
        print()
        print(
            '\033[31mERROR:\033[0m No serial ports detected.',
            file=sys.stderr,
        )
        sys.exit(1)

    if auto_select_only_one and len(ports) == 1:
        # Auto select the first port.
        return ports[0].device

    # Create valid entry list
    port_lines = list(
        f'{port.device} - {port.manufacturer} - {port.description}'
        for i, port in enumerate(ports)
    )

    print()
    print('Please select a serial port device.')
    print('Available ports:')
    selected_index, _selected_text = interactive_index_select(
        selection_lines=port_lines,
        prompt_text=(
            'Enter a port index or press up/down (Ctrl-C to cancel)\n> '
        ),
    )

    selected_port = ports[selected_index]

    return selected_port.device


if __name__ == '__main__':
    sys.exit(main(**vars(_parse_args())))
