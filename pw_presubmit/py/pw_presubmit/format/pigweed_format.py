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
"""A CLI utility that checks and fixes formatting for source files."""

import os
from pathlib import Path
import subprocess
import sys

from pw_cli.tool_runner import ToolRunner, BasicSubprocessRunner
from pw_presubmit.format.private.cli import FormattingSuite
from pw_presubmit.format.python import BlackFormatter

try:
    from python.runfiles import runfiles  # type: ignore

    _IS_GN = False
except ImportError:
    _IS_GN = True


# This is a stopgap. Ideally, we have something like:
#
#  from pw_presubmit.format.black_runfile_wrapper import BLACK
#
#  _r = RunfilesRunner()
#  _r.add_tool(BLACK)
class _RunfilesRunner(ToolRunner):
    def __init__(self):
        self._runfiles = runfiles.Create()
        self.tools = {}

    def add_tool(self, tool_name: str, runfiles_path: str, source_repo: str):
        actual_path = self._runfiles.Rlocation(
            runfiles_path,
            source_repo,
        )
        if actual_path is None or not Path(actual_path).is_file():
            raise ValueError(f'{runfiles_path} is not a valid runfiles path')
        self.tools[tool_name] = actual_path

    def _run_tool(
        self, tool: str, args, **kwargs
    ) -> subprocess.CompletedProcess:
        if tool not in self.tools:
            raise ValueError(
                f'This tool runner does not have a registered tool for `{tool}`'
            )
        return subprocess.run([self.tools[tool], *args], **kwargs)


def _pigweed_formatting_suite() -> FormattingSuite:
    if _IS_GN:
        # GN Relies on getting stuff from the environment, and assumes all
        # binaries are on the system PATH. DO NOT LET THESE ASSUMPTIONS LEAK
        # INTO BAZEL!
        pw_root = os.environ.get('PW_ROOT')
        if pw_root is None:
            raise ValueError(
                'This tool cannot be run from outside an activated environment '
                'when being built without Bazel.'
            )
        tool_runfiles = {'.black.toml': Path(pw_root) / '.black.toml'}
        default_runner = BasicSubprocessRunner()  # type: ignore[assignment]
    else:
        r = runfiles.Create()
        tool_runfiles = {
            '.black.toml': r.Rlocation(
                'pigweed/.black.toml', r.CurrentRepository()
            ),
        }
        default_runner = _RunfilesRunner()  # type: ignore[assignment]
        default_runner.add_tool(  # type: ignore[attr-defined]
            'black', 'pigweed/pw_presubmit/py/black', r.CurrentRepository()
        )

    # This is deliberately not public since there's some work to do to make
    # things more extensible/maintainable before downstream projects try to
    # depend on this.
    pigweed_formatters = [
        BlackFormatter(
            config_file=tool_runfiles['.black.toml'],
            tool_runner=default_runner,
        ),
    ]
    return FormattingSuite(pigweed_formatters)


if __name__ == '__main__':
    sys.exit(_pigweed_formatting_suite().main())
