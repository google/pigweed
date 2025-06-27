# Copyright 2025 The Pigweed Authors
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
"""A CLI tool for running tools, builds, and more from a workflows.json file."""

import argparse
from collections.abc import Callable, Sequence
import json
import logging
import os
from pathlib import Path
import sys
from typing import NoReturn

from google.protobuf import json_format, message, text_format
from pw_build.proto import workflows_pb2
from pw_cli import multitool
from pw_config_loader import find_config

_LOG = logging.getLogger(__name__)

_WORKFLOWS_FILE = 'workflows.json'


class _BuiltinPlugin(multitool.MultitoolPlugin):
    def __init__(
        self,
        name: str,
        description: str,
        callback: Callable[[Sequence[str]], int],
    ):
        self._name = name
        self._description = description
        self._callback = callback

    def name(self) -> str:
        return self._name

    def help(self) -> str:
        return self._description

    def run(self, plugin_args: Sequence[str]) -> int:
        return self._callback(plugin_args)


class WorkflowsCli(multitool.MultitoolCli):
    """A CLI entry point for launching project-specific workflows."""

    def __init__(self, config: workflows_pb2.WorkflowSuite | None = None):
        self.config: workflows_pb2.WorkflowSuite | None = config

    @staticmethod
    def _load_proto_json(config: Path) -> workflows_pb2.WorkflowSuite:
        json_msg = json.loads(config.read_text())
        msg = workflows_pb2.WorkflowSuite()
        json_format.ParseDict(json_msg, msg)
        return msg

    @staticmethod
    def _load_config_from(
        search_from: Path = Path.cwd(),
    ) -> workflows_pb2.WorkflowSuite:
        config = next(
            find_config.configs_in_parents(_WORKFLOWS_FILE, search_from),
            None,
        )
        if not config:
            _LOG.critical(
                'No `%s` file found in current directory or its parents',
                _WORKFLOWS_FILE,
            )
            return workflows_pb2.WorkflowSuite()
        return WorkflowsCli._load_proto_json(config)

    def _dump_textproto(self, plugin_args: Sequence[str]) -> int:
        parser = argparse.ArgumentParser()
        parser.add_argument(
            'name',
            nargs='?',
            default=None,
        )
        args = parser.parse_args(plugin_args)
        if self.config is None:
            print('Config is empty')
            return 0
        if not args.name:
            print(self.dump_config())
        else:
            print(self.dump_fragment(args.name))
        return 0

    def dump_config(self) -> str:
        """Dumps the entire config in a human-readable format."""
        if self.config is None:
            return ''

        return text_format.MessageToBytes(self.config).decode()

    def dump_fragment(self, fragment_name: str) -> str:
        """Dumps a fragment of the config in a human-readable format."""
        if not fragment_name:
            raise ValueError('Invalid empty fragment name')
        dump: message.Message | None = None

        if self.config is not None:
            for conf in self.config.configs:
                if conf.name == fragment_name:
                    dump = conf
                    break
            for tool in self.config.tools:
                if tool.name == fragment_name:
                    dump = tool
                    break
                if tool.build_config.name == fragment_name:
                    dump = tool.build_config
                    break
            for build in self.config.builds:
                if build.name == fragment_name:
                    dump = build
                    break
            for group in self.config.groups:
                if group.name == fragment_name:
                    dump = group
                    break
        if dump is None:
            raise ValueError(
                f'Could not find any config fragment named `{fragment_name}`'
            )
        return text_format.MessageToBytes(dump).decode()

    def _builtin_plugins(self) -> list[multitool.MultitoolPlugin]:
        return [
            _BuiltinPlugin(
                name='describe',
                description='Describe a build, tool, or group',
                callback=self._dump_textproto,
            ),
        ]

    def plugins(self) -> Sequence[multitool.MultitoolPlugin]:
        if not self.config:
            self.config = self._load_config_from()

        all_plugins = []
        all_plugins.extend(self._builtin_plugins())
        return all_plugins

    def main(self) -> NoReturn:
        # Small bit of UX magic: The `pw` wrapper entry point emits a loading
        # message while this tool is being bootstrapped. As soon as this tool
        # launches, clear that line so the output isn't polluted.
        if sys.stdout.isatty():
            print('\r\033[2K', end='')
        if 'BUILD_WORKING_DIRECTORY' in os.environ:
            os.chdir(os.environ['BUILD_WORKING_DIRECTORY'])
        super().main()


if __name__ == '__main__':
    WorkflowsCli().main()
