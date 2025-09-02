# Copyright 2020 The Pigweed Authors
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
"""The Pigweed command line interface."""

import argparse
from typing import Sequence

from pw_cli import multitool, pw_command_plugins, plugins


class BootstrappedPlugin(multitool.MultitoolPlugin):
    def __init__(self, plugin: plugins.Plugin):
        self.plugin = plugin

    def name(self) -> str:
        return self.plugin.name

    def help(self) -> str:
        return self.plugin.help()

    def run(self, plugin_args: Sequence[str]) -> int:
        return pw_command_plugins.run(self.name(), plugin_args)


class PwMultitoolTool(multitool.MultitoolCli):
    """The entry point for the `pw` tool."""

    def plugins(
        self, args: argparse.Namespace
    ) -> Sequence[multitool.MultitoolPlugin]:
        pw_command_plugins.register()
        return [
            BootstrappedPlugin(plugin)
            for name, plugin in pw_command_plugins.plugin_registry.items()
        ]


def main():
    PwMultitoolTool().main()


if __name__ == '__main__':
    main()
