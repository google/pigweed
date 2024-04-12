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
import jsonschema
import jsonschema.exceptions
import yaml
import importlib

_METADATA_SCHEMA = yaml.safe_load(
    importlib.resources.read_text("pw_sensor", "metadata_schema.json")
)

_DEPENDENCY_SCHEMA = yaml.safe_load(
    importlib.resources.read_text("pw_sensor", "dependency_schema.json")
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
          metadata: Structured sensor data, this will be modified

        Returns:
          The same modified metadata that was passed into the function. Used
          primarily as a convinience such that metadata can be defined inline
          and the result captured.

        Raises:
          RuntimeError: An error in the schema validation or a missing
            definition.
          FileNotFoundError: One of the dependencies was not found.
        """
        try:
            jsonschema.validate(instance=metadata, schema=_METADATA_SCHEMA)
        except jsonschema.exceptions.ValidationError as e:
            raise RuntimeError(
                "ERROR: Malformed sensor metadata YAML: "
                f"{yaml.safe_dump(metadata, indent=2)}"
            ) from e

        # Resolve all the dependencies, after this, 'resolved' will have a
        # list of channel and attribute specifiers
        self._resolve_dependencies(metadata=metadata)

        # Resolve all channel entries
        self._resolve_channels(metadata=metadata)
        return metadata

    def _resolve_dependencies(self, metadata: dict) -> None:
        """
        Given a list of dependencies, ensure that each of them exists and
        matches the schema provided in dependency_schema.yaml. Once loaded, the
        content of the definition file replace the entry in the dependency list.
        Additionally, any default values are filled in such that after the
        function returns, the dictionary is as verbose as possible.

        Args:
          metadata: The full sensor metadata passed to the validate function

        Raises:
          RuntimeError: An error in the schema validation or a missing
            definition.
          FileNotFoundError: One of the dependencies was not found.
        """
        deps: None | list[str] = metadata.get("deps")
        if not deps:
            self._logger.debug("No dependencies found, skipping imports")
            metadata["deps"] = []
            return

        resolved_deps: list[dict] = []
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
                resolved_deps.append(dep_yaml)

        metadata["deps"] = resolved_deps

    def _resolve_channels(self, metadata: dict) -> None:
        """
        For each channel in the metadata, find the matching definition in the
        'deps' entry and use the data to fill any missing information. For
        example, if an entry exists that looks like:
            acceleration: {}

        We would then try and find the 'acceleration' key in the dependencies
        list (which was already validated by _resolve_dependencies). Since the
        example above does not override any fields, we would copy the 'name',
        'description', and 'units' from the definition into the channel entry.

        Args:
          metadata: The full sensor metadata passed to the validate function

        Raises:
          RuntimeError: An error in the schema validation or a missing
            definition.
          FileNotFoundError: One of the dependencies was not found.
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
                channel_name=channel_name, deps=metadata["deps"]
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

    def _check_channel_name(self, channel_name: str, deps: list[dict]) -> dict:
        """
        Given a channel name and the resolved list of dependencies, try to find
        the full definition of the channel with the name, description, and
        units.

        Args:
          channel_name: The name of the channel to search for in the
            dependencies
          deps: The list of resolved dependencies which define the available
            channels

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
        for dep in deps:
            channels: dict | None = dep.get("channels")
            if not channels:
                continue
            # Check if we can find 'channel_name' in the 'channels' dictionary
            if channels.get(channel_name):
                channel = channels[channel_name]
                name = channel.get("name", channel_name)
                description = channel.get("description", "")
                units = {
                    "name": channel["units"].get(
                        "name", channel["units"]["symbol"]
                    ),
                    "symbol": channel["units"]["symbol"],
                }
                return {
                    "name": name,
                    "description": description,
                    "units": units,
                }
            # Check if we have sub-channels
            for key in channels:
                channel = channels[key]
                if not (
                    channel_name.startswith(key + "_")
                    and channel_name != key
                    and channel.get("sub-channels")
                ):
                    # The channel cannot be a sub-channel, skip it
                    continue

                # It's possible we have a match, check the sub-channels
                sub_channel_name = channel_name.removeprefix(key + "_")
                sub_channel = channel["sub-channels"].get(sub_channel_name)
                if not sub_channel:
                    # Couldn't find the right sub-channel, skip it
                    continue

                # Resolve the name, description, and units. Fill in the defaults
                # according to the override chain.
                name = sub_channel.get(
                    "name", channel.get("name", channel_name)
                )
                description = sub_channel.get(
                    "description", channel.get("description", "")
                )
                units = {
                    "name": channel["units"].get(
                        "name", channel["units"]["symbol"]
                    ),
                    "symbol": channel["units"]["symbol"],
                }
                return {
                    "name": name,
                    "description": description,
                    "units": units,
                }

        raise RuntimeError(
            f"Failed to find a channel defenition for {channel_name}, "
            "did you forget a dependency?"
        )
