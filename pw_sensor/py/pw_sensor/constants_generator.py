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
class Printable:
    """Common printable object"""

    id: str
    name: str
    description: str | None

    @property
    def variable_name(self) -> str:
        return "k" + ''.join(
            ele.title() for ele in re.split(r"[\s_-]+", self.id)
        )

    def print(self, writer: io.TextIOWrapper) -> None:
        writer.write(
            f"""
/// @var k{self.variable_name}
/// @brief {self.name}
"""
        )
        if self.description:
            writer.write(
                f"""///
/// {self.description}
"""
            )


@dataclass(frozen=True)
class Units:
    """A single unit representation"""

    name: str
    symbol: str


@dataclass(frozen=True)
class Attribute(Printable):
    """A single attribute representation."""

    units: Units

    def print(self, writer: io.TextIOWrapper) -> None:
        """Print header definition for this attribute"""
        super().print(writer=writer)
        writer.write(
            f"""
PW_SENSOR_ATTRIBUTE_TYPE(
    static,
    {super().variable_name},
    "PW_SENSOR_ATTRIBUTE_TYPE",
    "{self.name}",
    "{self.units.symbol}"
);
"""
        )


@dataclass(frozen=True)
class Channel(Printable):
    """A single channel representation."""

    units: Units

    def print(self, writer: io.TextIOWrapper) -> None:
        """Print header definition for this channel"""
        super().print(writer=writer)
        writer.write(
            f"""
PW_SENSOR_MEASUREMENT_TYPE(
    static,
    {super().variable_name},
    "PW_SENSOR_MEASUREMENT_TYPE",
    "{self.name}",
    "{self.units.symbol}"
);
"""
        )


@dataclass(frozen=True)
class Trigger(Printable):
    """A single trigger representation."""

    id: str
    name: str
    description: str

    def print(self, writer: io.TextIOWrapper) -> None:
        """Print header definition for this trigger"""
        super().print(writer=writer)
        writer.write(
            f"""
PW_SENSOR_TRIGGER_TYPE(
    static,
    {super().variable_name},
    "PW_SENSOR_TRIGGER_TYPE",
    "{self.name}"
);
"""
        )


@dataclass
class Args:
    """CLI arguments"""

    package: Sequence[str]
    language: str


def attribute_from_dict(attribute_id: str, definition: dict) -> Attribute:
    """Construct an Attribute from a dictionary entry."""
    return Attribute(
        id=attribute_id,
        name=definition["name"],
        description=definition["description"],
        units=Units(
            name=definition["units"]["name"],
            symbol=definition["units"]["symbol"],
        ),
    )


def channel_from_dict(channel_id: str, definition: dict) -> Channel:
    """Construct a Channel from a dictionary entry."""
    return Channel(
        id=channel_id,
        name=definition["name"],
        description=definition["description"],
        units=Units(
            name=definition["units"]["name"],
            symbol=definition["units"]["symbol"],
        ),
    )


def trigger_from_dict(trigger_id: str, definition: dict) -> Trigger:
    """Construct a Trigger from a dictionary entry."""
    return Trigger(
        id=trigger_id,
        name=definition["name"],
        description=definition["description"],
    )


class CppHeader:
    """Generator for a C++ header"""

    def __init__(
        self,
        package: Sequence[str],
        attributes: Sequence[Attribute],
        channels: Sequence[Channel],
        triggers: Sequence[Trigger],
    ) -> None:
        """
        Args:
          package: The package name used in the output. In C++ we'll convert
            this to a namespace.
          units: A sequence of units which should be exposed as
            ::pw::sensor::MeasurementType.
        """
        self._package: str = '::'.join(package)
        self._attributes: Sequence[Attribute] = attributes
        self._channels: Sequence[Channel] = channels
        self._triggers: Sequence[Trigger] = triggers

    def __str__(self) -> str:
        writer = io.StringIO()
        self._print_header(writer=writer)
        self._print_constants(writer=writer)
        self._print_footer(writer=writer)
        return writer.getvalue()

    def _print_header(self, writer: io.TextIOWrapper) -> None:
        """
        Print the top part of the .h file (pragma, includes, and namespace)

        Args:
          writer: Where to write the text to
        """
        writer.write(
            "/* Auto-generated file, do not edit */\n"
            "#pragma once\n"
            "\n"
            "#include \"pw_sensor/types.h\"\n"
        )
        if self._package:
            writer.write(f"namespace {self._package} {{\n\n")

    def _print_constants(self, writer: io.TextIOWrapper) -> None:
        """
        Print the constants definitions from self._attributes, self._channels,
        and self._trigger

        Args:
            writer: Where to write the text
        """

        writer.write("namespace attributes {\n")
        for attribute in self._attributes:
            attribute.print(writer)
        writer.write("}  // namespace attributes\n")
        writer.write("namespace channels {\n")
        for channel in self._channels:
            channel.print(writer)
        writer.write("}  // namespace channels\n")
        writer.write("namespace triggers {\n")
        for trigger in self._triggers:
            trigger.print(writer)
        writer.write("}  // namespace triggers\n")

    def _print_footer(self, writer: io.TextIOWrapper) -> None:
        """
        Write the bottom part of the .h file (closing namespace)

        Args:
            writer: Where to write the text
        """
        if self._package:
            writer.write(f"\n}}  // namespace {self._package}")


def main() -> None:
    """
    Main entry point, this function will:
    - Get CLI flags
    - Read YAML from stdin
    - Find all channel definitions
    - Print header
    """
    args = get_args()
    spec = yaml.safe_load(sys.stdin)
    all_attributes: set[Attribute] = set()
    all_channels: set[Channel] = set()
    all_triggers: set[Trigger] = set()
    for attribute_id, definition in spec["attributes"].items():
        attribute = attribute_from_dict(
            attribute_id=attribute_id, definition=definition
        )
        assert not attribute in all_attributes
        all_attributes.add(attribute)
    for channel_id, definition in spec["channels"].items():
        channel = channel_from_dict(
            channel_id=channel_id, definition=definition
        )
        assert not channel in all_channels
        all_channels.add(channel)
    for trigger_id, definition in spec["triggers"].items():
        trigger = trigger_from_dict(
            trigger_id=trigger_id, definition=definition
        )
        assert not trigger in all_triggers
        all_triggers.add(trigger)

    if args.language == "cpp":
        out = CppHeader(
            package=args.package,
            attributes=list(all_attributes),
            channels=list(all_channels),
            triggers=list(all_triggers),
        )
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
