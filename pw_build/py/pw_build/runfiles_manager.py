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
"""A library that bridges runfiles management between Bazel and bootstrap."""

import importlib
import os
from pathlib import Path
import re
import shutil
import subprocess
from typing import List

from pw_cli.tool_runner import ToolRunner

try:
    from python.runfiles import runfiles  # type: ignore

    _IS_BOOTSTRAP = False
except ImportError:
    _IS_BOOTSTRAP = True

_ENV_VAR_PATTERN = re.compile(r'\${([^}]*)}')


class RunfilesManager(ToolRunner):
    """A class that helps manage runtime file resources.

    Projects that use bootstrap get their files from the currently
    activated environment. In contrast, Bazel-based projects will get their
    files from Bazel's runfiles mechanism.

    This class bridges those differences, and simplifies the process of
    writing tools that work in both environments. Every resource is associated
    with a key used to retrieve the resource, and the resources must be
    registered twice: once for Bazel-based projects, and once for other
    projects that run from a shell environment bootstrapped by pw_env_setup.
    To ensure consistency, if a resource is used by one environment, an error
    will be emitted if it was never registered for the other environment.

    If a file is exclusive to one of the two environments, it may be tagged
    as ``exclusive=True`` to suppress the error emitted if a resource isn't
    properly registered for the other environment. Attempting to access a
    resource ``exclusive`` to a different environment will still raise an error.

    This class is also a :py:class:`pw_cli.tool_runner.ToolRunner`, and may be
    used to launch subprocess actions.
    """

    def __init__(self):
        self._runfiles: dict[str, Path] = {}
        self._tools: dict[str, str] = {}
        self._bazel_resources: set[str] = set()
        self._bootstrap_resources: set[str] = set()
        if _IS_BOOTSTRAP:
            self._r = None
        else:
            self._r = runfiles.Create()

    def add_bazel_tool(
        self, tool_name: str, import_path: str, exclusive: bool = False
    ) -> None:
        """Maps a runnable tool to the provided tool name.

        Files added through this mechanism will be available when running from
        the Bazel build. Unless you specify ``exclusive=True``, you must also
        register this file with
        :py:meth:`pw_build.runfiles_manager.RunfilesManager.add_bootstrapped_tool()`
        before attempting to use it.

        The ``import_path`` is the import path of the
        ``pw_py_importable_runfile`` rule that provides the desired file.
        """  # pylint: disable=line-too-long
        self._map_bazel_runfile(
            tool_name, import_path, executable=True, exclusive=exclusive
        )

    def add_bootstrapped_tool(
        self,
        tool_name: str,
        path_or_env: str,
        from_shell_path: bool = False,
        exclusive: bool = False,
    ) -> None:
        """Maps a runnable tool to the provided tool name.

        Files added through this mechanism will be available from an
        activated environment constructed via pw_env_setup. Unless you specify
        ``exclusive=True``, you must also register this tool with
        :py:meth:`pw_build.runfiles_manager.RunfilesManager.add_bazel_tool()`
        before attempting to use it.

        Environment variables may be expanded using ``${PW_FOO}`` within the
        path expression. If ``from_shell_path=True`` is enabled, the active
        shell ``PATH`` is searched for the requested tool, and environment
        variables will **not** be expanded.
        """
        self._map_bootstrap_runfile(
            tool_name,
            path_or_env,
            executable=True,
            from_shell_path=from_shell_path,
            exclusive=exclusive,
        )

    def add_bazel_file(
        self, key: str, import_path: str, exclusive: bool = False
    ) -> None:
        """Maps a non-executable file resource to the provided key.

        Files added through this mechanism will be available when running from
        the Bazel build. Unless you specify ``exclusive=True``, you must also
        register this file with
        :py:meth:`pw_build.runfiles_manager.RunfilesManager.add_bootstrapped_file()`
        before attempting to use it.

        The ``import_path`` is the import path of the
        ``pw_py_importable_runfile`` rule that provides the desired file.
        """  # pylint: disable=line-too-long
        self._map_bazel_runfile(
            key, import_path, executable=False, exclusive=exclusive
        )

    def add_bootstrapped_file(
        self, key: str, path_or_env: str, exclusive: bool = False
    ) -> None:
        """Maps a non-executable file resource to the provided key.

        Files added through this mechanism will be available from an activated
        environment constructed via pw_env_setup. Unless you specify
        ``exclusive=True``, you must also register this file with
        :py:meth:`pw_build.runfiles_manager.RunfilesManager.add_bazel_file()`
        before attempting to use it.

        Environment variables may be expanded using ``${PW_FOO}`` within the
        path expression.
        """
        self._map_bootstrap_runfile(
            key, path_or_env, executable=False, exclusive=exclusive
        )

    def _map_bazel_runfile(
        self, key: str, import_path: str, executable: bool, exclusive: bool
    ) -> None:
        if _IS_BOOTSTRAP:
            self._register_bazel_resource(key, exclusive)
            return
        try:
            module = importlib.import_module(import_path)
        except ImportError:
            raise ValueError(
                f'Failed to load runfiles import `{key}={import_path}`. Did '
                'you forget to add a dependency on the appropriate '
                'pw_py_importable_runfile?'
            )
        file_path = self._r.Rlocation(*module.RLOCATION)
        self._check_path(file_path, key)
        self._runfiles[key] = Path(file_path)
        if executable:
            self._tools[key] = file_path
        self._register_bazel_resource(key, exclusive)

    def _map_bootstrap_runfile(
        self,
        key: str,
        path: str,
        executable: bool,
        exclusive: bool,
        from_shell_path: bool = False,
    ) -> None:
        if not _IS_BOOTSTRAP:
            self._register_bootstrap_resource(key, exclusive)
            return
        if from_shell_path:
            actual_path = shutil.which(path)
            if actual_path is None:
                raise ValueError(f'Tool `{key}={path}` not found on PATH')
            path = actual_path
        unknown_vars: List[str] = []
        known_vars: dict[str, str] = {}
        for fmt_var in _ENV_VAR_PATTERN.findall(path):
            if not fmt_var:
                raise ValueError(
                    f'Runfiles entry `{key}={path}` has a format expansion '
                    'with no environment variable name'
                )
            if fmt_var not in os.environ:
                unknown_vars.append(fmt_var)
            else:
                if os.environ[fmt_var] is None:
                    raise ValueError(
                        f'Runfiles entry `{key}={path}` requested the '
                        '{fmt_var} environment variable, which is set but empty'
                    )
                known_vars[fmt_var] = os.environ[fmt_var]
        if unknown_vars:
            raise ValueError(
                'Failed to expand the following environment variables for '
                f'runfile entry `{key}={path}`: {", ".join(unknown_vars)}'
            )
        file_path = _ENV_VAR_PATTERN.sub(
            lambda m: known_vars[m.group(1)],
            path,
        )
        self._check_path(file_path, key)

        self._runfiles[key] = Path(file_path)
        if executable:
            self._tools[key] = file_path
        self._register_bootstrap_resource(key, exclusive)

    def _register_bootstrap_resource(self, key: str, exclusive: bool):
        self._bootstrap_resources.add(key)
        if exclusive:
            self._bazel_resources.add(key)

    def _register_bazel_resource(self, key: str, exclusive: bool):
        self._bazel_resources.add(key)
        if exclusive:
            self._bootstrap_resources.add(key)

    @staticmethod
    def _check_path(path: str, key: str):
        if not Path(path).is_file():
            raise ValueError(f'Runfile `{key}={path}` does not exist')

    def get(self, key: str) -> Path:
        """Retrieves the ``Path`` to the resource at the requested key."""
        not_known_by = []
        if not key in self._bazel_resources:
            not_known_by.append('Bazel')
        if not key in self._bootstrap_resources:
            not_known_by.append('bootstrap')
        if not_known_by:
            if len(not_known_by) == 1:
                which = not_known_by[0]
                other = lambda e: 'Bazel' if e == 'bootstrap' else 'bootstrap'
                raise ValueError(
                    f'`{key}` was registered for {other(which)} environments, '
                    f'but not for {which} environments. Either register in '
                    f'{which} or mark as `exclusive=True`'
                )
            raise ValueError(
                f'`{key}` is not a registered tool or runfile resource'
            )
        if not key in self._runfiles:
            this_environment_kind = 'bootstrap' if _IS_BOOTSTRAP else 'Bazel'
            other_environment_kind = 'Bazel' if _IS_BOOTSTRAP else 'bootstrap'
            raise ValueError(
                f'`{key}` was marked as `exclusive=True` to '
                f'{other_environment_kind} environments, but was used '
                f'in a {this_environment_kind} environment'
            )

        # Note that this is intentionally designed not to return `None`, as
        # that makes it too easy to mask issues due to missing resources. Often,
        # this causes bugs to occur later during execution of a script in ways
        # that are less clearly spelled out.
        return self._runfiles[key]

    def __getitem__(self, key):
        return self.get(key)

    def _run_tool(
        self, tool: str, args, **kwargs
    ) -> subprocess.CompletedProcess:
        tool_path = self.get(tool)
        if tool not in self._tools:
            raise ValueError(
                f'`{tool}` was registered as a file rather than a runnable '
                'tool. Register it with add_bazel_tool() and/or '
                'add_bootstrapped_tool() to make it runnable.'
            )
        return subprocess.run([str(tool_path), *args], **kwargs)
