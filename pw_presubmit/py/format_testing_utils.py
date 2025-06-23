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
"""Utilities common to format tests."""

import subprocess
from pathlib import Path
from typing import List, Mapping

from pw_cli.tool_runner import ToolRunner


class CapturingToolRunner(ToolRunner):
    """A ToolRunner that captures executed commands and their arguments."""

    def __init__(self, tool_map: Mapping[str, Path] | None = None):
        self.tool_map: Mapping[str, Path] = tool_map if tool_map else {}
        self.command_history: List[str] = []

    def _run_tool(
        self, tool: str, args, **kwargs
    ) -> subprocess.CompletedProcess:
        """Runs the requested tool with the provided arguments.

        The full command is appended to ``command_history``.
        """
        if self.tool_map:
            if tool not in self.tool_map:
                raise ValueError(f'Attempted to run unmapped tool `{tool}`')
            tool = str(self.tool_map[tool])
        cmd = [tool] + args
        self.command_history.append(' '.join([str(arg) for arg in cmd]))
        return subprocess.run(cmd, **kwargs)
