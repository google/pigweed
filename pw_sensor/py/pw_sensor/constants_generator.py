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
from dataclasses import dataclass, fields, is_dataclass
from collections.abc import Sequence
import io
import re
import sys
from typing import Type, List, Any
import typing

import yaml


def kid_from_name(name: str) -> str:
    """
    Generate a const style ID name from a given name string. Example:
      If name is "sample_rate", the ID would be kSampleRate

    Args:
      name: the name to convert to an ID
    Returns:
      C++ style 'k' prefixed camel cased ID
    """
    return "k" + ''.join(ele.title() for ele in re.split(r"[\s_\-\,]+", name))


class Printable:
    """Common printable object"""

    def __init__(
        self, item_id: str, name: str, description: str | None
    ) -> None:
        self.id: str = item_id
        self.name: str = name
        self.description: str | None = description

    def __hash__(self) -> int:
        return hash((self.id, self.name, self.description))

    @property
    def variable_name(self) -> str:
        """Convert the 'id' to a constant variable name in C++."""
        return kid_from_name(self.id)

    def print(self, writer: io.TextIOWrapper) -> None:
        """
        Baseclass for a printable sensor object

        Args:
          writer: IO writer used to print values.
        """
        writer.write(
            f"""
/// @brief {self.name}
"""
        )
        if self.description:
            writer.write(
                f"""///
/// {self.description}
"""
            )


@dataclass
class UnitsSpec:
    """Typing for the Units definition dictionary."""

    name: str
    symbol: str


class Units(Printable):
    """A single unit representation"""

    symbol: str

    def __init__(self, unit_id: str, definition: UnitsSpec) -> None:
        """
        Create a new unit object

        Args:
          unit_id: The ID of the unit
          definition: A dictionary of the unit definition
        """
        super().__init__(
            item_id=unit_id,
            name=definition.name,
            description=definition.name,
        )
        self.symbol: str = definition.symbol

    def __hash__(self) -> int:
        return hash((super().__hash__(), self.symbol))

    def __eq__(self, value: object) -> bool:
        if not isinstance(value, Units):
            return False
        return (
            self.id == value.id
            and self.name == value.name
            and self.description == value.description
            and self.symbol == value.symbol
        )

    def print(self, writer: io.TextIOWrapper) -> None:
        """
        Print header definition for this unit

        Args:
          writer: IO writer used to print values.
        """
        super().print(writer=writer)
        writer.write(
            f"""PW_SENSOR_UNIT_TYPE(
    {super().variable_name},
    "PW_SENSOR_UNITS_TYPE",
    "{self.name}",
    "{self.symbol}"
);
"""
        )


@dataclass
class AttributeSpec:
    """Typing for the Attribute definition dictionary."""

    name: str
    description: str


class Attribute(Printable):
    """A single attribute representation."""

    def __init__(self, attr_id: str, definition: AttributeSpec) -> None:
        super().__init__(
            item_id=attr_id,
            name=definition.name,
            description=definition.description,
        )

    def print(self, writer: io.TextIOWrapper) -> None:
        """
        Print header definition for this attribute

        Args:
          writer: IO writer used to print values.
        """
        super().print(writer=writer)
        writer.write(
            f"""PW_SENSOR_ATTRIBUTE_TYPE(
    {super().variable_name},
    "PW_SENSOR_ATTRIBUTE_TYPE",
    "{self.name}"
);
"""
        )


@dataclass
class ChannelSpec:
    """Typing for the Channel definition dictionary."""

    name: str
    description: str
    units: str


class Channel(Printable):
    """A single channel representation."""

    def __init__(
        self, channel_id: str, definition: ChannelSpec, units: dict[str, Units]
    ) -> None:
        super().__init__(
            item_id=channel_id,
            name=definition.name,
            description=definition.description,
        )
        self.units: Units = units[definition.units]

    def __hash__(self) -> int:
        return hash((super().__hash__(), self.units))

    def __eq__(self, value: object) -> bool:
        if not isinstance(value, Channel):
            return False
        return (
            self.id == value.id
            and self.name == value.name
            and self.description == value.description
            and self.units == value.units
        )

    def print(self, writer: io.TextIOWrapper) -> None:
        """
        Print header definition for this channel

        Args:
          writer: IO writer used to print values.
        """
        super().print(writer=writer)
        writer.write(
            f"""
PW_SENSOR_MEASUREMENT_TYPE({super().variable_name},
                           "PW_SENSOR_MEASUREMENT_TYPE",
                           "{self.name}",
                           ::pw::sensor::units::{self.units.variable_name}
);"""
        )


@dataclass
class TriggerSpec:
    """Typing for the Trigger definition dictionary."""

    name: str
    description: str


class Trigger(Printable):
    """A single trigger representation."""

    def __init__(self, trigger_id: str, definition: TriggerSpec) -> None:
        super().__init__(
            item_id=trigger_id,
            name=definition.name,
            description=definition.description,
        )

    def print(self, writer: io.TextIOWrapper) -> None:
        """
        Print header definition for this trigger

        Args:
          writer: IO writer used to print values.
        """
        super().print(writer=writer)
        writer.write(
            f"""PW_SENSOR_TRIGGER_TYPE(
    {super().variable_name},
    "PW_SENSOR_TRIGGER_TYPE",
    "{self.name}"
);
"""
        )


@dataclass
class SensorAttributeSpec:
    """Typing for the SensorAttribute definition dictionary."""

    channel: str
    attribute: str
    units: str


class SensorAttribute(Printable):
    """An attribute instance belonging to a sensor"""

    @staticmethod
    def id_from_definition(definition: SensorAttributeSpec) -> str:
        """
        Get a unique ID for the channel/attribute pair (not sensor specific)

        Args:
          definition: A dictionary of the attribute definition
        Returns:
          String representation for the channel/attribute pair
        """
        return f"{definition.channel}-{definition.attribute}"

    @staticmethod
    def name_from_definition(definition: SensorAttributeSpec) -> str:
        """
        Get a unique name for the channel/attribute pair (not sensor specific)

        Args:
          definition: A dictionary of the attribute definition
        Returns:
          String representation of the human readable name for the
          channel/attribute pair.
        """
        return f"{definition.channel}'s {definition.attribute} attribute"

    @staticmethod
    def description_from_definition(definition: SensorAttributeSpec) -> str:
        """
        Get the description for the channel/attribute pair (not sensor specific)

        Args:
          definition: A dictionary of the attribute definition
        Returns:
          A description string for the channel/attribute pair.
        """
        return (
            f"Allow the configuration of the {definition.channel}'s "
            + f"{definition.attribute} attribute"
        )

    def __init__(self, definition: SensorAttributeSpec) -> None:
        super().__init__(
            item_id=SensorAttribute.id_from_definition(definition=definition),
            name=SensorAttribute.name_from_definition(definition=definition),
            description=SensorAttribute.description_from_definition(
                definition=definition
            ),
        )
        self.attribute: str = definition.attribute
        self.channel: str = definition.channel
        self.units: str = definition.units

    def __hash__(self) -> int:
        return hash(
            (super().__hash__(), self.attribute, self.channel, self.units)
        )

    def print(self, writer: io.TextIOWrapper) -> None:
        super().print(writer)
        writer.write(
            f"""
PW_SENSOR_ATTRIBUTE_INSTANCE({self.variable_name},
                             channels::{kid_from_name(self.channel)},
                             attributes::{kid_from_name(self.attribute)},
                             units::{kid_from_name(self.units)});
"""
        )


@dataclass
class CompatibleSpec:
    """Typing for the Compatible dictionary."""

    org: str
    part: str


@dataclass
class SensorSpec:
    """Typing for the Sensor definition dictionary."""

    description: str
    compatible: CompatibleSpec
    attributes: List[SensorAttributeSpec]
    channels: dict[str, List[ChannelSpec]]
    triggers: List[Any]


class Sensor(Printable):
    """Represent a single sensor type instance"""

    @staticmethod
    def sensor_id_to_name(sensor_id: str) -> str:
        """
        Convert a sensor ID to a human readable name

        Args:
          sensor_id: The ID of the sensor
        Returns:
          Human readable name based on the ID
        """
        return sensor_id.replace(',', ' ')

    def __init__(self, item_id: str, definition: SensorSpec) -> None:
        super().__init__(
            item_id=item_id,
            name=Sensor.sensor_id_to_name(item_id),
            description=definition.description,
        )
        self.compatible_org: str = definition.compatible.org
        self.compatible_part: str = definition.compatible.part
        self.chan_count: int = len(definition.channels)
        self.attr_count: int = len(definition.attributes)
        self.trig_count: int = len(definition.triggers)
        self.attributes: Sequence[SensorAttribute] = [
            SensorAttribute(definition=spec) for spec in definition.attributes
        ]

    @property
    def namespace(self) -> str:
        """
        The namespace which owns the sensor (the org from the compatible)

        Returns:
          The C++ style namespace name of the org.
        """
        return self.compatible_org.replace("-", "_")

    def __hash__(self) -> int:
        return hash(
            (
                super().__hash__(),
                self.compatible_org,
                self.compatible_part,
                self.chan_count,
                self.attr_count,
                self.trig_count,
            )
        )

    def print(self, writer: io.TextIOWrapper) -> None:
        """
        Print header definition for this trigger

        Args:
          writer: IO writer used to print values.
        """
        writer.write(
            f"""
namespace {self.namespace} {{
class {self.compatible_part.upper()}
  : public pw::sensor::zephyr::ZephyrSensor<{len(self.attributes)}> {{
 public:
  {self.compatible_part.upper()}(const struct device* dev)
      : pw::sensor::zephyr::ZephyrSensor<{len(self.attributes)}>(
            dev,
            {{"""
        )
        for attribute in self.attributes:
            writer.write(
                f"""
             Attribute::Build<{attribute.variable_name}>(),"""
            )
        writer.write(
            f"""
            }}) {{}}
}};
}}  // namespace {self.namespace}
"""
        )


@dataclass
class Args:
    """CLI arguments"""

    package: Sequence[str]
    language: str
    zephyr: bool


class CppHeader:
    """Generator for a C++ header"""

    def __init__(
        self,
        using_zephyr: bool,
        package: Sequence[str],
        attributes: Sequence[Attribute],
        channels: Sequence[Channel],
        triggers: Sequence[Trigger],
        units: Sequence[Units],
        sensors: Sequence[Sensor],
    ) -> None:
        """
        Args:
          package: The package name used in the output. In C++ we'll convert
            this to a namespace.
          attributes: A sequence of attributes which will be exposed in the
            'attributes' namespace
          channels: A sequence of channels which will be exposed in the
            'channels' namespace
          triggers: A sequence of triggers which will be exposed in the
            'triggers' namespace
          units: A sequence of units which should be exposed in the 'units'
            namespace
        """
        self._using_zephyr: bool = using_zephyr
        self._package: str = '::'.join(package)
        self._attributes: Sequence[Attribute] = attributes
        self._channels: Sequence[Channel] = channels
        self._triggers: Sequence[Trigger] = triggers
        self._units: Sequence[Units] = units
        self._sensors: Sequence[Sensor] = sensors
        self._sensor_attributes: set[SensorAttribute] = set()
        for sensor in sensors:
            self._sensor_attributes.update(sensor.attributes)

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

        self._print_in_namespace(
            namespace="units", printables=self._units, writer=writer
        )
        self._print_in_namespace(
            namespace="attributes", printables=self._attributes, writer=writer
        )
        self._print_in_namespace(
            namespace="channels", printables=self._channels, writer=writer
        )
        self._print_in_namespace(
            namespace="triggers", printables=self._triggers, writer=writer
        )
        for sensor_attribute in self._sensor_attributes:
            sensor_attribute.print(writer=writer)

    @staticmethod
    def _print_in_namespace(
        namespace: str,
        printables: Sequence[Printable],
        writer: io.TextIOWrapper,
    ) -> None:
        """
        Print constants definitions wrapped in a namespace
        Args:
          namespace: The namespace to use
          printables: A sequence of printable objects
          writer: Where to write the text
        """
        writer.write(f"\nnamespace {namespace} {{\n")
        for printable in printables:
            printable.print(writer=writer)
        writer.write(f"\n}}  // namespace {namespace}\n")

    def _print_footer(self, writer: io.TextIOWrapper) -> None:
        """
        Write the bottom part of the .h file (closing namespace)

        Args:
            writer: Where to write the text
        """
        if self._package:
            writer.write(f"\n}}  // namespace {self._package}")

        if self._using_zephyr:
            self._print_zephyr_mapping(writer=writer)

    def _print_zephyr_mapping(self, writer: io.TextIOWrapper) -> None:
        """
        Generate Zephyr type maps for channels, attributes, and triggers.

        Args:
            writer: Where to write the text
        """
        writer.write(
            f"""
#include <zephyr/generated/sensor_constants.h>
#include \"pw_containers/flat_map.h\"

namespace pw::sensor::zephyr {{

class ZephyrAttributeMap
    : public pw::containers::FlatMap<uint32_t, uint32_t,
                                     {len(self._attributes)}> {{
 public:
  constexpr ZephyrAttributeMap()
      : pw::containers::FlatMap<uint32_t, uint32_t,
                                {len(self._attributes)}>({{{{"""
        )
        for attribute in self._attributes:
            attribute_type = (
                f"{self._package}::attributes::"
                + f"{attribute.variable_name}::kAttributeType"
            )
            writer.write(
                f"""
            {{{attribute_type},
             SENSOR_ATTR_{attribute.id.upper()}}},"""
            )
        writer.write("\n      }}) {}\n};")
        writer.write(
            f"""

class ZephyrChannelMap
    : public pw::containers::FlatMap<uint32_t, uint32_t,
                                     {len(self._channels)}> {{
 public:
  constexpr ZephyrChannelMap()
      : pw::containers::FlatMap<uint32_t, uint32_t,
                                {len(self._channels)}>({{{{"""
        )
        for channel in self._channels:
            measurement_name = (
                f"{self._package}::channels::"
                + f"{channel.variable_name}::kMeasurementName"
            )
            writer.write(
                f"""
            {{{measurement_name},
             SENSOR_CHAN_{channel.id.upper()}}},"""
            )
        writer.write(
            """
      }}) {}
};

extern ZephyrAttributeMap kAttributeMap;
extern ZephyrChannelMap kChannelMap;

}  // namespace pw::sensor::zephyr

#include "pw_sensor_zephyr/sensor.h"
namespace pw::sensor {
"""
        )
        for sensor in self._sensors:
            sensor.print(writer=writer)
        writer.write(
            """
}  // namespace pw::sensor
"""
        )


@dataclass
class InputSpec:
    """Typing for the InputData spec dictionary"""

    units: dict[str, UnitsSpec]
    attributes: dict[str, AttributeSpec]
    channels: dict[str, ChannelSpec]
    triggers: dict[str, TriggerSpec]
    sensors: dict[str, SensorSpec]


class InputData:
    """
    Wrapper class for all the input data parsed out into: Units, Attribute,
    Channel, Trigger, and Sensor types (or sub-types).
    """

    def __init__(
        self,
        spec: InputSpec,
        units_type: Type[Units] = Units,
        attribute_type: Type[Attribute] = Attribute,
        channel_type: Type[Channel] = Channel,
        trigger_type: Type[Trigger] = Trigger,
        sensor_type: Type[Sensor] = Sensor,
    ) -> None:
        """
        Parse the input spec and create all the input data types.

        Args:
          spec: The input spec dictionary
          units_type: The type to use for units
          attribute_type: The type to use for attributes
          channel_type: The type to use for channels
          trigger_type: The type to use for triggers
          sensor_type: The type to use for sensors
        """
        self.all_attributes: set[Attribute] = set()
        self.all_channels: set[Channel] = set()
        self.all_triggers: set[Trigger] = set()
        self.all_units: dict[str, Units] = {}
        self.all_sensors: set[Sensor] = set()
        for units_id, units_spec in spec.units.items():
            units = units_type(unit_id=units_id, definition=units_spec)
            assert units not in self.all_units.values()
            self.all_units[units_id] = units
        for attribute_id, attribute_spec in spec.attributes.items():
            attribute = attribute_type(
                attr_id=attribute_id, definition=attribute_spec
            )
            assert not attribute in self.all_attributes
            self.all_attributes.add(attribute)
        for channel_id, channel_spec in spec.channels.items():
            channel = channel_type(
                channel_id=channel_id,
                definition=channel_spec,
                units=self.all_units,
            )
            assert not channel in self.all_channels
            self.all_channels.add(channel)
        for trigger_id, trigger_spec in spec.triggers.items():
            trigger = trigger_type(
                trigger_id=trigger_id, definition=trigger_spec
            )
            assert not trigger in self.all_triggers
            self.all_triggers.add(trigger)
        for sensor_id, sensor_spec in spec.sensors.items():
            sensor = sensor_type(item_id=sensor_id, definition=sensor_spec)
            assert not sensor in self.all_sensors
            self.all_sensors.add(sensor)


def is_list_type(t) -> bool:
    """
    Checks if the given type `t` is either a list or typing.List.

    Args:
        t: The type to check.
    Returns:
        True if `t` is a list type, False otherwise.
    """
    origin = typing.get_origin(t)
    return origin is list or (origin is list and typing.get_args(t) == ())


def create_dataclass_from_dict(cls, data, indent: int = 0):
    """Recursively creates a dataclass instance from a nested dictionary."""

    field_values = {}

    if is_list_type(cls):
        result = []
        for item in data:
            result.append(
                create_dataclass_from_dict(
                    typing.get_args(cls)[0], item, indent + 2
                )
            )
        return result

    for field in fields(cls):
        field_value = data[field.name]

        # We need to check if the field is a List, dictionary, or another
        # dataclass. If it is, recurse.
        if is_list_type(field.type):
            item_type = typing.get_args(field.type)[0]
            field_value = [
                create_dataclass_from_dict(item_type, item, indent + 2)
                for item in field_value
            ]
        elif dict in field.type.__mro__:
            value_type = typing.get_args(field.type)[1]
            field_value = {
                key: create_dataclass_from_dict(value_type, val, indent + 2)
                for key, val in field_value.items()
            }
        elif is_dataclass(field.type):
            field_value = create_dataclass_from_dict(
                field.type, field_value, indent + 2
            )

        field_values[field.name] = field_value

    return cls(**field_values)


def main() -> None:
    """
    Main entry point, this function will:
    - Get CLI flags
    - Read YAML from stdin
    - Find all attribute, channel, trigger, and unit definitions
    - Print header
    """
    args = get_args()
    yaml_input = yaml.safe_load(sys.stdin)
    spec: InputSpec = create_dataclass_from_dict(InputSpec, yaml_input)
    data = InputData(spec=spec)

    if args.language == "cpp":
        out = CppHeader(
            using_zephyr=args.zephyr,
            package=args.package,
            attributes=list(data.all_attributes),
            channels=list(data.all_channels),
            triggers=list(data.all_triggers),
            units=list(data.all_units.values()),
            sensors=list(data.all_sensors),
        )
    else:
        raise ValueError(f"Invalid language selected: '{args.language}'")
    print(out)


def validate_package_arg(value: str) -> str:
    """
    Validate that the package argument is a valid string

    Args:
      value: The package name

    Returns:
      The same value after being validated.
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
    parser.add_argument(
        "--zephyr",
        action="store_true",
    )
    args = parser.parse_args()
    return Args(
        package=args.package.split("."),
        language=args.language,
        zephyr=args.zephyr,
    )


if __name__ == "__main__":
    main()
