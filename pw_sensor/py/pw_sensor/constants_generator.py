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
"""Tooling to generate C++ constants from a yaml sensor definition."""

import argparse
from dataclasses import dataclass
from collections.abc import Sequence
import io
import re
import sys

import yaml


@dataclass(frozen=True)
class Units:
    """A single unit representation"""

    id: str
    name: str
    symbol: str


@dataclass
class Args:
    """CLI arguments"""

    package: Sequence[str]
    language: str


class CppHeader:
    """Generator for a C++ header"""

    def __init__(self, package: Sequence[str], units: Sequence[Units]) -> None:
        """
        Args:
          package: The package name used in the output. In C++ we'll convert
            this to a namespace.
          units: A sequence of units which should be exposed as
            ::pw::sensor::MeasurementType.
        """
        self._package: str = '::'.join(package)
        self._units: Sequence[Units] = units

    def __str__(self) -> str:
        writer = io.StringIO()
        self._print_header(writer=writer)
        self._print_units(writer=writer)
        self._print_footer(writer=writer)
        return writer.getvalue()

    def _print_header(self, writer: io.TextIOWrapper) -> None:
        """
        Print the top part of the .h file (pragma, includes, and namespace)

        Args:
          writer: Where to write the text to
        """
        writer.write("/* Auto-generated file, do not edit */\n")
        writer.write("#pragma once\n")
        writer.write("\n")
        writer.write("#include \"pw_sensor/types.h\"\n")
        writer.write("\n")
        if self._package:
            writer.write(f"namespace {self._package} {{\n\n")

    def _print_units(self, writer: io.TextIOWrapper) -> None:
        """
        Print the unit definitions from self._units as
        ::pw::sensor::MeasurementType

        Args:
            writer: Where to write the text
        """
        for units in self._units:
            variable_name = ''.join(
                ele.title() for ele in re.split(r"[\s_-]+", units.id)
            )
            writer.write(
                f"constexpr ::pw::sensor::MeasurementType k{variable_name} =\n"
            )
            writer.write(
                '    PW_SENSOR_MEASUREMENT_TYPE'
                '("PW_SENSOR_MEASUREMENT_TYPE", '
                f'"{units.name}", "{units.symbol}");\n'
            )

    def _print_footer(self, writer: io.TextIOWrapper) -> None:
        """
        Write the bottom part of the .h file (closing namespace)

        Args:
            writer: Where to write the text
        """
        if self._package:
            writer.write(f"\n}}  // {self._package}")


def main() -> None:
    """
    Main entry point, this function will:
    - Get CLI flags
    - Read YAML from stdin
    - Find all channel definitions
    - Print header
    """
    args = get_args()
    definition = yaml.safe_load(sys.stdin)
    all_units: set[Units] = set()
    for channel_id, definition in definition["channels"].items():
        unit = Units(
            id=channel_id,
            name=definition["units"]["name"],
            symbol=definition["units"]["symbol"],
        )
        assert not unit in all_units
        all_units.add(unit)

    if args.language == "cpp":
        out = CppHeader(package=args.package, units=list(all_units))
    else:
        raise ValueError(f"Invalid language selected: '{args.language}'")
    print(out)


def validate_package_arg(value: str) -> str:
    """
    Validate that the package argument is a valid string
    """
    if value is None or value == "":
        return value
    if not re.match(r"[a-zA-Z_$][\w$]*(\.[a-zA-Z_$][\w$]*)*", value):
        raise argparse.ArgumentError(
            argument=None,
            message=f"Invalid string {value}. Must use alphanumeric values "
            "separated by dots.",
        )
    return value


def get_args() -> Args:
    """
    Get CLI arguments

    Returns:
      Typed arguments class instance
    """
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--package",
        "-pkg",
        default="",
        type=validate_package_arg,
        help="Output package name separated by '.', example: com.google",
    )
    parser.add_argument(
        "--language",
        type=str,
        choices=["cpp"],
        default="cpp",
    )
    args = parser.parse_args()
    return Args(package=args.package.split("."), language=args.language)


if __name__ == "__main__":
    main()
