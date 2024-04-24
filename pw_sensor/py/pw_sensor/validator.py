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
import importlib
import logging
from pathlib import Path

import jsonschema  # type: ignore
import jsonschema.exceptions  # type: ignore
import yaml

_METADATA_SCHEMA = yaml.safe_load(
    importlib.resources.read_text("pw_sensor", "metadata_schema.json")
)

_DEPENDENCY_SCHEMA = yaml.safe_load(
    importlib.resources.read_text("pw_sensor", "dependency_schema.json")
)

_RESOLVED_SCHEMA = yaml.safe_load(
    importlib.resources.read_text("pw_sensor", "resolved_schema.json")
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
          channels:
            acceleration: {}
            die_temperature: {}

        Args:
          metadata: Structured sensor data, this will NOT be modified

        Returns:
          A set of channels and a single sensor which match the schema in
          resolved_schema.json.

        Raises:
          RuntimeError: An error in the schema validation or a missing
            definition.
          FileNotFoundError: One of the dependencies was not found.
        """
        result: dict = {
            "attributes": {},
            "channels": {},
            "triggers": {},
            "sensors": {},
        }
        metadata = metadata.copy()
        try:
            jsonschema.validate(instance=metadata, schema=_METADATA_SCHEMA)
        except jsonschema.exceptions.ValidationError as e:
            raise RuntimeError(
                "ERROR: Malformed sensor metadata YAML:\n"
                f"{yaml.safe_dump(metadata, indent=2)}"
            ) from e

        # Resolve all the dependencies, after this, 'resolved' will have a
        # list of channel and attribute specifiers
        self._resolve_dependencies(metadata=metadata, out=result)

        self._logger.debug(yaml.safe_dump(result, indent=2))

        # Resolve all channel entries
        self._resolve_channels(metadata=metadata, out=result)

        # Resolve all attribute entries
        self._resolve_attributes(metadata=metadata, out=result)

        # Resolve all trigger entries
        self._resolve_triggers(metadata=metadata, out=result)

        compatible = metadata.pop("compatible")
        channels = metadata.pop("channels")
        attributes = metadata.pop("attributes")
        triggers = metadata.pop("triggers")
        result["sensors"][f"{compatible['org']},{compatible['part']}"] = {
            "compatible": compatible,
            "channels": channels,
            "attributes": attributes,
            "triggers": triggers,
        }

        try:
            jsonschema.validate(instance=result, schema=_RESOLVED_SCHEMA)
        except jsonschema.exceptions.ValidationError as e:
            raise RuntimeError(
                "ERROR: Malformed output YAML: "
                f"{yaml.safe_dump(result, indent=2)}"
            ) from e

        return result

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
            out["channels"] = {}
            out["attributes"] = {}
            out["triggers"] = {}
            return

        for dep in deps:
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
                self._backfill_declarations(declarations=dep_yaml, out=out)

    def _backfill_declarations(self, declarations: dict, out: dict) -> None:
        """
        Add any missing properties of a declaration object.

        Args:
          declarations: The top level declarations dictionary loaded from the
            dependency file.
        """
        self._backfill_channels(declarations=declarations, out=out)
        self._backfill_attributes(declarations=declarations, out=out)
        self._backfill_triggers(declarations=declarations, out=out)

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
            assert resolved_attributes.get(attr_id) is None
            resolved_attributes[attr_id] = attribute
            if not attribute.get("name"):
                attribute["name"] = attr_id
            if not attribute.get("description"):
                attribute["description"] = ""
            if not attribute["units"].get("name"):
                attribute["units"]["name"] = attribute["units"]["symbol"]

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
            assert resolved_channels.get(chan_id) is None
            resolved_channels[chan_id] = channel
            if not channel.get("name"):
                channel["name"] = chan_id
            if not channel.get("description"):
                channel["description"] = ""
            units = channel["units"]
            if not units.get("name"):
                units["name"] = units["symbol"]
            # Resolve sub-channels
            for sub, sub_channel in channel.get("sub-channels", {}).items():
                subchan_id = f"{chan_id}_{sub}"
                if sub_channel.get("name") is None:
                    sub_channel["name"] = subchan_id
                if sub_channel.get("description") is None:
                    sub_channel["description"] = channel.get("description")
                sub_channel["units"] = channel["units"]
                resolved_channels[subchan_id] = sub_channel
            channel.pop("sub-channels", None)

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
            assert resolved_triggers.get(trigger_id) is None
            resolved_triggers[trigger_id] = trigger
            if not trigger.get("name"):
                trigger["name"] = trigger_id
            if not trigger.get("description"):
                trigger["description"] = ""

    def _resolve_attributes(self, metadata: dict, out: dict) -> None:
        """
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
        attributes: dict | None = metadata.get("attributes")
        if not attributes:
            metadata["attributes"] = {}
            self._logger.debug("No attributes found, skipping")
            return

        for attribute_name, attribute_value in attributes.items():
            # Check if the attribute_name exists in 'out/attributes', we can
            # assume 'out/attributes' exists because _resolve_dependencies() is
            # required to have been called first.
            attribute = self._check_scalar_name(
                name=attribute_name,
                haystack=out["attributes"],
                overrides=attribute_value,
            )
            # The content of 'attribute' came from the 'out/attributes' list
            # which was already validated and every field added if missing. At
            # this point it's safe to access the attribute's name, description,
            # and units.
            attribute_value["name"] = attribute["name"]
            attribute_value["description"] = attribute["description"]
            attribute_value["units"] = attribute["units"]

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

        for channel_name, channel_values in channels.items():
            # Check if the channel_name exists in 'out/channels', we can assume
            # 'out/channels' exists because _resolve_dependencies() is required
            # to have been called first.
            channel = self._check_scalar_name(
                name=channel_name,
                haystack=out["channels"],
                overrides=channel_values,
            )
            # The content of 'channel' came from the 'out/channels' dict which
            # was already validated and every field added if missing. At this
            # point it's safe to access the channel's name, description, and
            # units.
            channel_values["name"] = channel["name"]
            channel_values["description"] = channel["description"]
            channel_values["units"] = channel["units"]

            if not channel_values.get("indicies"):
                channel_values["indicies"] = [{}]
            for index in channel_values["indicies"]:
                if not index.get("name"):
                    index["name"] = channel_values["name"]
                if not index.get("description"):
                    index["description"] = channel_values["description"]

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
        triggers: dict | None = metadata.get("triggers")
        if not triggers:
            metadata["triggers"] = {}
            self._logger.debug("No triggers found, skipping")
            return

        for trigger_name, trigger_value in triggers.items():
            # Check if the trigger_name exists in 'out/triggers', we can
            # assume 'out/triggers' exists because _resolve_dependencies() is
            # required to have been called first.
            trigger = self._check_scalar_name(
                name=trigger_name,
                haystack=out["triggers"],
                overrides=trigger_value,
            )
            # The content of 'trigger' came from the 'out/triggers' dict
            # which was already validated and every field added if missing. At
            # this point it's safe to access the trigger's name and description.
            trigger_value["name"] = trigger["name"]
            trigger_value["description"] = trigger["description"]

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

    @staticmethod
    def _check_scalar_name(name: str, haystack: dict, overrides: dict) -> dict:
        """
        Given a name and the resolved list of dependencies, try to find
        the full definition of a scalar (channel or attribute) with the name,
        description, and units OR trigger with just a name/description.

        We rely on the schema to ensure that channels and attributes have units
        so if we can't find units, then we must be looking for a trigger.

        Args:
          name: The name of the channel/attribute/trigger to search for in the
            dependencies
          haystack: The dictionary of resolved properties which define the
            available channels/attributes/triggers

        Returns:
          A dictionary with the following structure:
            name: string
            description: string
            (optional)
            units:
              name: string
              symbol: string

        Raises:
          RuntimeError: If the 'name' isn't in the dependency list
        """
        # Check if we can find 'name' in the 'haystack' dictionary
        if not haystack.get(name):
            raise RuntimeError(
                f"Failed to find a definition for '{name}', did you forget a "
                "dependency?"
            )

        item = haystack[name]
        name = overrides.get("name", item.get("name", name))
        description = overrides.get("description", item.get("description", ""))
        if item.get("units") is None:
            return {
                "name": name,
                "description": description,
            }

        units = {
            "name": item["units"].get("name", item["units"]["symbol"]),
            "symbol": item["units"]["symbol"],
        }
        return {
            "name": name,
            "description": description,
            "units": units,
        }
