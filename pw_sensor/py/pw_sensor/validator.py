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
"""Sensor schema validation tooling."""

from collections.abc import Sequence
import importlib.resources
import logging
from pathlib import Path
import re

import jsonschema  # type: ignore
import jsonschema.exceptions  # type: ignore
import yaml

_METADATA_SCHEMA = yaml.safe_load(
    importlib.resources.files("pw_sensor")
    .joinpath("metadata_schema.json")
    .read_text()
)

_DEPENDENCY_SCHEMA = yaml.safe_load(
    importlib.resources.files("pw_sensor")
    .joinpath("dependency_schema.json")
    .read_text()
)

_RESOLVED_SCHEMA = yaml.safe_load(
    importlib.resources.files("pw_sensor")
    .joinpath("resolved_schema.json")
    .read_text()
)


class Validator:
    """
    Context used for validating metadata dictionaries.

    What the validator is:
    - A system to resolve and verify that declared sensor metadata is well
      defined and formatted
    - A utility to resolve any and all dependencies when using a specified
      metadata file.

    What the validator is NOT:
    - Code generator
    """

    def __init__(
        self,
        include_paths: Sequence[Path] | None = None,
        log_level: int = logging.WARNING,
    ) -> None:
        """
        Construct a Validator with some context of the current run.

        Args:
          include_paths: An optional list of directories in which to resolve
            dependencies
          log_level: A desired logging level (defaults to logging.WARNING)
        """
        self._include_paths = include_paths if include_paths else []
        self._logger = logging.getLogger(self.__class__.__name__)
        self._logger.setLevel(log_level)

    def validate(self, metadata: dict) -> dict:
        """
        Accept a structured metadata description. This dictionary should first
        pass the schema provided in metadata_schema.yaml. Then, every channel
        used by the sensor should be defined in exactly one of the dependencies.
        Example YAML:

          deps:
            - "pw_sensor/channels.yaml"
          compatible:
            org: "Bosch"
            part: "BMA4xx
          supported-buses:
            - i2c
          channels:
            acceleration: []
            die_temperature: []

        Args:
          metadata: Structured sensor data, this will NOT be modified

        Returns:
          A set of attributes, channels, triggers, and units along with a single
          sensor which match the schema in resolved_schema.json.

        Raises:
          RuntimeError: An error in the schema validation or a missing
            definition.
          FileNotFoundError: One of the dependencies was not found.
        """
        result: dict = {
            "attributes": {},
            "channels": {},
            "triggers": {},
            "units": {},
            "sensors": {},
        }
        metadata = metadata.copy()

        # Validate the incoming schema
        try:
            jsonschema.validate(instance=metadata, schema=_METADATA_SCHEMA)
        except jsonschema.exceptions.ValidationError as e:
            raise RuntimeError(
                "ERROR: Malformed sensor metadata YAML:\n"
                f"{yaml.safe_dump(metadata, indent=2)}"
            ) from e

        # Resolve all the dependencies, after this, 'result' will have all the
        # missing properties for which defaults can be provided
        self._resolve_dependencies(metadata=metadata, out=result)

        self._logger.debug(
            "Resolved dependencies:\n%s", yaml.safe_dump(result, indent=2)
        )

        # Resolve all channel entries (must be done before attributes)
        self._resolve_channels(metadata=metadata, out=result)

        # Resolve all trigger entries (must be done before attributes)
        self._resolve_triggers(metadata=metadata, out=result)

        # Resolve all attribute entries
        self._resolve_attributes(metadata=metadata, out=result)

        compatible, compatible_str = Validator._get_compatible_string_and_dict(
            metadata.pop("compatible")
        )
        supported_buses = metadata.pop("supported-buses")
        channels = metadata.pop("channels")
        attributes = metadata.pop("attributes")
        triggers = metadata.pop("triggers")

        result["sensors"][compatible_str] = {
            "compatible": compatible,
            "supported-buses": self._normalize_supported_buses(supported_buses),
            "channels": channels,
            "attributes": attributes,
            "triggers": triggers,
            "description": metadata.get("description", ""),
            "extras": metadata.get("extras", {}),
        }

        # Validate the final output before returning
        try:
            jsonschema.validate(instance=result, schema=_RESOLVED_SCHEMA)
        except jsonschema.exceptions.ValidationError as e:
            msg = (
                "ERROR: Malformed output YAML: "
                f"{yaml.safe_dump(result, indent=2)}"
            )
            raise RuntimeError(msg) from e

        return result

    @staticmethod
    def _normalize_supported_buses(buses: list[str]) -> list[str]:
        """Resolve a list of supported buses

        Each bus string will be converted to lowercase and all sequential
        whitespace & '-' characters will be replaced by a single '_'.

        Args:
            buses: A list of the supported sensor buses

        Returns:
            Normalized list of buses

        """
        filtered_list = list(
            {re.sub(r"[\s\-]+", "_", s.lower()) for s in buses}
        )
        if len(buses) != len(filtered_list):
            error = (
                "ERROR: bus list contains duplicates when converted to "
                f"lowercase and concatenated with '_': {sorted(buses)} -> "
                f"{sorted(filtered_list)}"
            )
            raise RuntimeError(error)
        return filtered_list

    @staticmethod
    def _get_compatible_string_and_dict(
        compatible: dict[str, str],
    ) -> tuple[dict[str, str], str]:
        """
        Normalize compatible info

        This function processes a 'compatible' dictionary with a 'part' key and
        an optional 'org' key. It returns a new dictionary with the 'org' key
        removed if it was empty or missing, and a formatted string based on the
        'org' key's presence and value.

        Args:
            compatible (dict[str, str]): A dictionary with a 'part' key and an
            optional 'org' key.

        Returns:
            Tuple[dict[str, str], str]: A tuple containing:
            - A new dictionary with the 'org' key removed if it was empty or
              missing.
            - A formatted string:
              - "{org},{part}" if 'org' exists and is not empty (after trimming)
              - "part" otherwise.

        """
        part = compatible["part"].lower()
        org = compatible.get("org", "").strip().lower()

        new_compatible = {"part": part}
        if org:
            new_compatible["org"] = org
            return new_compatible, f"{org},{part}"
        return new_compatible, part

    def _resolve_dependencies(self, metadata: dict, out: dict) -> None:
        """
        Given a list of dependencies, ensure that each of them exists and
        matches the schema provided in dependency_schema.yaml. Once loaded, the
        content of the definition file will be resolved (filling in any missing
        fields that can be inherited) and the final result will be placed in the
        'out' dictionary.

        Args:
          metadata: The full sensor metadata passed to the validate function
          out: Output dictionary where the resolved dependencies should be
            stored

        Raises:
          RuntimeError: An error in the schema validation or a missing
            definition.
          FileNotFoundError: One of the dependencies was not found.
        """
        deps: None | list[str] = metadata.get("deps")
        if not deps:
            self._logger.debug("No dependencies found, skipping imports")
            return

        merged_deps: dict = {
            "attributes": {},
            "channels": {},
            "triggers": {},
            "units": {},
        }
        for dep in deps:
            # Load each of the dependencies, then merge them. This avoids any
            # include dependency order issues.
            dep_file = self._get_dependency_file(dep)
            with open(dep_file, mode="r", encoding="utf-8") as dep_yaml_file:
                dep_yaml = yaml.safe_load(dep_yaml_file)
                try:
                    jsonschema.validate(
                        instance=dep_yaml, schema=_DEPENDENCY_SCHEMA
                    )
                except jsonschema.exceptions.ValidationError as e:
                    raise RuntimeError(
                        "ERROR: Malformed dependency YAML: "
                        f"{yaml.safe_dump(dep_yaml, indent=2)}"
                    ) from e
                # Merge all the loaded values into 'merged_deps'
                for category in merged_deps:
                    self._merge_deps(
                        category=category,
                        dep_yaml=dep_yaml,
                        merged_deps=merged_deps,
                    )
        # Backfill any default values from the merged dependencies and put them
        # into 'out'
        self._backfill_declarations(declarations=merged_deps, out=out)

    @staticmethod
    def _merge_deps(category: str, dep_yaml: dict, merged_deps: dict) -> None:
        """
        Pull all properties from dep_yaml[category] and put them into
        merged_deps after validating that no key duplicates exist.

        Args:
          category: The index of dep_yaml and merged_deps to merge
          dep_yaml: The newly loaded dependency YAML
          merged_deps: The accumulated dependency map
        """
        for key, value in dep_yaml.get(category, {}).items():
            assert (
                key not in merged_deps[category]
            ), f"'{key}' was already found under '{category}'"
            merged_deps[category][key] = value

    def _backfill_declarations(self, declarations: dict, out: dict) -> None:
        """
        Add any missing properties of a declaration object.

        Args:
          declarations: The top level declarations dictionary loaded from the
            dependency file.
          out: The already resolved map of all defined dependencies
        """
        self._backfill_units(declarations=declarations, out=out)
        self._backfill_channels(declarations=declarations, out=out)
        self._backfill_attributes(declarations=declarations, out=out)
        self._backfill_triggers(declarations=declarations, out=out)

    @staticmethod
    def _backfill_units(declarations: dict, out: dict) -> None:
        """
        Move units from 'declarations' to 'out' while also filling in any
        default values.

        Args:
          declarations: The original YAML declaring units.
          out: Output dictionary where we'll add the key "units" wit the result.
        """
        if out.get("units") is None:
            out["units"] = {}
        resolved_units: dict = out["units"]
        if not declarations.get("units"):
            return

        for units_id, unit in declarations["units"].items():
            # Copy unit to resolved_units and fill any default values
            assert resolved_units.get(units_id) is None
            resolved_units[units_id] = unit
            if not unit.get("name"):
                unit["name"] = unit["symbol"]
            if unit.get("description") is None:
                unit["description"] = ""

    @staticmethod
    def _backfill_attributes(declarations: dict, out: dict) -> None:
        """
        Move attributes from 'delcarations' to 'out' while also filling in any
        default values.

        Args:
          declarations: The original YAML declaring attributes.
          out: Output dictionary where we'll add the key "attributes" with the
            result.
        """
        if out.get("attributes") is None:
            out["attributes"] = {}
        resolved_attributes: dict = out["attributes"]
        if not declarations.get("attributes"):
            return

        for attr_id, attribute in declarations["attributes"].items():
            # Copy attribute to resolved_attributes and fill any default values
            assert resolved_attributes.get(attr_id) is None
            resolved_attributes[attr_id] = attribute
            if not attribute.get("name"):
                attribute["name"] = attr_id
            if not attribute.get("description"):
                attribute["description"] = ""

    @staticmethod
    def _backfill_channels(declarations: dict, out: dict) -> None:
        """
        Move channels from 'declarations' to 'out' while also filling in any
        default values.

        Args:
          declarations: The original YAML declaring channels.
          out: Output dictionary where we'll add the key "channels" with the
            result.
        """
        if out.get("channels") is None:
            out["channels"] = {}
        resolved_channels: dict = out["channels"]
        if not declarations.get("channels"):
            return

        for chan_id, channel in declarations["channels"].items():
            # Copy channel to resolved_channels and fill any default values
            assert resolved_channels.get(chan_id) is None
            resolved_channels[chan_id] = channel
            if not channel.get("name"):
                channel["name"] = chan_id
            if not channel.get("description"):
                channel["description"] = ""
            assert channel["units"] in out["units"], (
                f"'{channel['units']}' not found in\n"
                + f"{yaml.safe_dump(out.get('units', {}), indent=2)}"
            )

    @staticmethod
    def _backfill_triggers(declarations: dict, out: dict) -> None:
        """
        Move triggers from 'delcarations' to 'out' while also filling in any
        default values.

        Args:
          declarations: The original YAML declaring triggers.
          out: Output dictionary where we'll add the key "triggers" with the
            result.
        """
        if out.get("triggers") is None:
            out["triggers"] = {}
        resolved_triggers: dict = out["triggers"]
        if not declarations.get("triggers"):
            return

        for trigger_id, trigger in declarations["triggers"].items():
            # Copy trigger to resolved_triggers and fill any default values
            assert resolved_triggers.get(trigger_id) is None
            resolved_triggers[trigger_id] = trigger
            if not trigger.get("name"):
                trigger["name"] = trigger_id
            if not trigger.get("description"):
                trigger["description"] = ""

    def _resolve_attributes(self, metadata: dict, out: dict) -> None:
        """Resolve and validate any default values in Attributes

        For each attribute in the metadta, find the matching definition in the
        'out/attributes' entry and use the data to fill any missing information.
        For example, if an entry exists that looks like:
            sample_rate: {}

        We would then try and find the 'sample_rate' key in the out/attributes
        list (which was already validated by _resolve_dependencies). Since the
        example above does not override any fields, we would copy the 'name',
        'description', and 'units' from the definition into the attribute entry.

        Args:
          metadata: The full sensor metadata passed to the validate function
          out: The current output, used to get channel definitions

        Raises:
          RuntimeError: An error in the schema validation or a missing
            definition.

        """
        attributes: list | None = metadata.get("attributes")
        if not attributes:
            metadata["attributes"] = []
            self._logger.debug("No attributes found, skipping")
            return

        attribute: dict
        for attribute in attributes:
            assert attribute["attribute"] in out["attributes"]
            assert attribute["units"] in out["units"]

            has_channel_name = "channel" in attribute
            has_trigger_name = "trigger" in attribute

            if has_channel_name and has_trigger_name:
                error = (
                    "Attribute instances cannot specify both channel AND "
                    f"trigger:\n{yaml.safe_dump(attribute, indent=2)}"
                )
                raise RuntimeError(error)
            if has_channel_name:
                assert attribute["channel"] in out["channels"]
            if has_trigger_name:
                assert attribute["trigger"] in out["triggers"]

    def _resolve_channels(self, metadata: dict, out: dict) -> None:
        """
        For each channel in the metadata, find the matching definition in the
        'out/channels' entry and use the data to fill any missing information.
        For example, if an entry exists that looks like:
            acceleration: {}

        We would then try and find the 'acceleration' key in the out/channels
        dict (which was already validated by _resolve_dependencies). Since the
        example above does not override any fields, we would copy the 'name',
        'description', and 'units' from the definition into the channel entry.

        Args:
          metadata: The full sensor metadata passed to the validate function
          out: The current output, used to get channel definitions

        Raises:
          RuntimeError: An error in the schema validation or a missing
            definition.
        """
        channels: dict | None = metadata.get("channels")
        if not channels:
            self._logger.debug("No channels found, skipping")
            metadata["channels"] = {}
            return

        channel_name: str
        indices: list[dict]
        for channel_name, indices in channels.items():
            # channel_name must have been resolved by now.
            if out["channels"].get(channel_name) is None:
                raise RuntimeError(
                    f"Failed to find a definition for '{channel_name}', did you"
                    " forget a dependency?"
                )
            channel = out["channels"][channel_name]
            # The content of 'channel' came from the 'out/channels' dict which
            # was already validated and every field added if missing. At this
            # point it's safe to access the channel's name, description, and
            # units.

            if not indices:
                indices.append({})

            index: dict
            for index in indices:
                if not index.get("name"):
                    index["name"] = channel["name"]
                if not index.get("description"):
                    index["description"] = channel["description"]
                # Always use the same units
                index["units"] = channel["units"]

    def _resolve_triggers(self, metadata: dict, out: dict) -> None:
        """
        For each trigger in the metadata, find the matching definition in the
        'out/triggers' entry and use the data to fill any missing information.
        For example, if an entry exists that looks like:
            data_ready: {}

        We would then try and find the 'data_ready' key in the out/triggers
        dict (which was already validated by _resolve_dependencies). Since the
        example above does not override any fields, we would copy the 'name' and
        'description' from the definition into the trigger entry.

        Args:
          metadata: The full sensor metadata passed to the validate function
          out: The current output, used to get trigger definitions

        Raises:
          RuntimeError: An error in the schema validation or a missing
            definition.
        """
        triggers: list | None = metadata.get("triggers")
        if not triggers:
            metadata["triggers"] = []
            self._logger.debug("No triggers found, skipping")
            return

        for trigger_name in triggers:
            assert trigger_name in out["triggers"]

    def _get_dependency_file(self, dep: str) -> Path:
        """
        Search for a dependency file and return the full path to it if found.

        Args:
          dep: The dependency string as provided by the metadata yaml.

        Returns:
          The dependency file as a Path object if found.

        Raises:
          FileNotFoundError: One of the dependencies was not found.
        """
        error_string = f"Failed to find {dep} using search paths:"
        # Check if a full path was used
        if Path(dep).is_file():
            return Path(dep)

        # Search all the include paths
        for path in self._include_paths:
            if (path / dep).is_file():
                return path / dep
            error_string += f"\n- {path}"

        raise FileNotFoundError(error_string)
