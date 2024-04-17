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

import logging
import importlib.resources
from pathlib import Path
from collections.abc import Sequence
import importlib
import jsonschema
import jsonschema.exceptions
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
        self._logger = logging.getLogger(__class__.__name__)
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
            "sensors": {},
        }
        metadata = metadata.copy()
        try:
            jsonschema.validate(instance=metadata, schema=_METADATA_SCHEMA)
        except jsonschema.exceptions.ValidationError as e:
            raise RuntimeError(
                "ERROR: Malformed sensor metadata YAML: "
                f"{yaml.safe_dump(metadata, indent=2)}"
            ) from e

        # Resolve all the dependencies, after this, 'resolved' will have a
        # list of channel and attribute specifiers
        self._resolve_dependencies(metadata=metadata, out=result)

        # Resolve all channel entries
        self._resolve_channels(metadata=metadata, out=result)

        compatible = metadata.pop("compatible")
        channels = metadata.pop("channels")
        result["sensors"][f"{compatible['org']},{compatible['part']}"] = {
            "compatible": compatible,
            "channels": channels,
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
            return

        resolved_deps: dict = {}
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
                self._backfill_declarations(declarations=dep_yaml)
                for chan_id, channel in dep_yaml.get("channels", {}).items():
                    if channel.get("name") is None:
                        channel["name"] = chan_id
                    if channel.get("description") is None:
                        channel["description"] = ""
                    if channel["units"].get("name") is None:
                        channel["units"]["name"] = channel["units"]["symbol"]
                    resolved_deps[chan_id] = channel
                    for sub, sub_channel in channel.get(
                        "sub-channels", {}
                    ).items():
                        subchan_id = f"{chan_id}_{sub}"
                        if sub_channel.get("name") is None:
                            sub_channel["name"] = subchan_id
                        if sub_channel.get("description") is None:
                            sub_channel["description"] = channel.get(
                                "description"
                            )
                        sub_channel["units"] = channel["units"]
                        resolved_deps[subchan_id] = sub_channel
                    channel.pop("sub-channels", None)

        out["channels"] = resolved_deps

    def _backfill_declarations(self, declarations: dict) -> None:
        """
        Add any missing properties of a declaration object.

        Args:
          declarations: The top level declarations dictionary loaded from the
            dependency file.
        """
        if not declarations.get("channels"):
            return

        for chan_id, channel in declarations["channels"].items():
            if not channel.get("name"):
                channel["name"] = chan_id
            if not channel.get("description"):
                channel["description"] = ""
            units = channel["units"]
            if not units.get("name"):
                units["name"] = units["symbol"]

    def _resolve_channels(self, metadata: dict, out: dict) -> None:
        """
        For each channel in the metadata, find the matching definition in the
        'out/channels' entry and use the data to fill any missing information.
        For example, if an entry exists that looks like:
            acceleration: {}

        We would then try and find the 'acceleration' key in the out/channels
        list (which was already validated by _resolve_dependencies). Since the
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
            # Check if the channel_name exists in 'deps', we can assume 'deps'
            # exists because _resolve_dependencies() is required to have been
            # called first.
            channel = self._check_channel_name(
                channel_name=channel_name, channels=out["channels"]
            )
            # The content of 'channel' came from the 'deps' list which was
            # already validated and every field added if missing. At this point
            # it's safe to access the channel's name, description, and units.
            if not channel_values.get("name"):
                channel_values["name"] = channel["name"]
            if not channel_values.get("description"):
                channel_values["description"] = channel["description"]
            channel_values["units"] = channel["units"]

            if not channel_values.get("indicies"):
                channel_values["indicies"] = [{}]
            for index in channel_values["indicies"]:
                if not index.get("name"):
                    index["name"] = channel_values["name"]
                if not index.get("description"):
                    index["description"] = channel_values["description"]

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

    def _check_channel_name(self, channel_name: str, channels: dict) -> dict:
        """
        Given a channel name and the resolved list of dependencies, try to find
        the full definition of the channel with the name, description, and
        units.

        Args:
          channel_name: The name of the channel to search for in the
            dependencies
          channels: The dictionary of resolved channels which define the
            available channels

        Returns:
          A dictionary with the following structure:
            name: string
            description: string
            units:
              name: string
              symbol: string

        Raises:
          RuntimeError: If the channel name isn't in the dependency list
        """
        # Check if we can find 'channel_name' in the 'channels' dictionary
        if not channels.get(channel_name):
            raise RuntimeError(
                f"Failed to find a channel defenition for {channel_name}, "
                "did you forget a dependency?"
            )

        channel = channels[channel_name]
        name = channel.get("name", channel_name)
        description = channel.get("description", "")
        units = {
            "name": channel["units"].get("name", channel["units"]["symbol"]),
            "symbol": channel["units"]["symbol"],
        }
        return {
            "name": name,
            "description": description,
            "units": units,
        }
