# Copyright 2022 The Pigweed Authors
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
"""Tests for pw_build.project_builder_prefs"""

import argparse
import copy
from pathlib import Path
import shutil
import tempfile
from typing import Any
import unittest

from parameterized import parameterized  # type: ignore

from pw_build.project_builder_argparse import add_project_builder_arguments
from pw_build.project_builder_prefs import (
    ProjectBuilderPrefs,
    default_config,
    load_defaults_from_argparse,
)

_BAZEL_COMMAND = 'bazel'
# Prefer bazelisk if available.
if shutil.which('bazelisk'):
    _BAZEL_COMMAND = 'bazelisk'


def _create_tempfile(content: str) -> Path:
    with tempfile.NamedTemporaryFile(
        prefix=f'{__package__}', delete=False
    ) as output_file:
        output_file.write(content.encode('UTF-8'))
        return Path(output_file.name)


class TestProjectBuilderPrefs(unittest.TestCase):
    """Tests for ProjectBuilderPrefs."""

    maxDiff = None

    def test_load_no_existing_files(self) -> None:
        # Create a prefs instance with no loaded config.
        prefs = ProjectBuilderPrefs(
            load_argparse_arguments=add_project_builder_arguments,
            project_file=False,
            project_user_file=False,
            user_file=False,
        )
        # Construct an expected result config.
        expected_config: dict[Any, Any] = {}
        # Apply default build_system_commands and build_directories.
        expected_config.update(default_config())
        expected_config.update(
            load_defaults_from_argparse(add_project_builder_arguments)
        )

        self.assertEqual(
            prefs._config, expected_config  # pylint: disable=protected-access
        )

    @parameterized.expand(
        [
            (
                'Manual --build-system-commands on the command line',
                # Argparse input
                [
                    '--build-system-command',
                    'out',
                    'bazel build',
                    '--build-system-command',
                    'out',
                    'bazel test',
                ],
                # Expected changed config
                {
                    'default_build_targets': [],
                    'build_directories': [],
                    'build_system_commands': {
                        'out': {
                            'commands': [
                                {
                                    'command': 'bazel',
                                    'extra_args': ['build'],
                                },
                                {
                                    'command': 'bazel',
                                    'extra_args': ['test'],
                                },
                            ],
                        },
                    },
                },
            ),
            (
                'Empty build directory (no -C) with targets. '
                'Ninja manually specified.',
                # Argparse input
                '--default-build-system ninja docs python.lint'.split(),
                # Expected changed config
                {
                    'default_build_system': 'ninja',
                    'default_build_targets': ['docs', 'python.lint'],
                    'build_directories': [],
                    'build_system_commands': {
                        'default': {
                            'commands': [{'command': 'ninja', 'extra_args': []}]
                        }
                    },
                },
            ),
            (
                'Empty build directory (no -C) with targets. '
                'Ninja not specified.',
                # Argparse input
                'docs python.lint'.split(),
                # Expected changed config
                {
                    'default_build_targets': ['docs', 'python.lint'],
                    'build_directories': [],
                    'build_system_commands': {
                        'default': {
                            'commands': [{'command': 'ninja', 'extra_args': []}]
                        }
                    },
                },
            ),
            (
                'Empty build directory (no -C) with targets (bazel).',
                # Argparse input
                (
                    '--default-build-system bazel //pw_watch/... //pw_build/...'
                ).split(),
                # Expected changed config
                {
                    'default_build_system': 'bazel',
                    'default_build_targets': [
                        '//pw_watch/...',
                        '//pw_build/...',
                    ],
                    'build_directories': [],
                    'build_system_commands': {
                        'default': {
                            'commands': [
                                {
                                    'command': _BAZEL_COMMAND,
                                    'extra_args': ['build'],
                                },
                                {
                                    'command': _BAZEL_COMMAND,
                                    'extra_args': ['test'],
                                },
                            ]
                        }
                    },
                },
            ),
            (
                'Targets with no build directory and an additional build '
                'directory with targets.',
                # Argparse input
                'docs python.lint -C out2 python.tests'.split(),
                # Expected changed config
                {
                    'default_build_targets': ['docs', 'python.lint'],
                    'build_directories': [['out2', 'python.tests']],
                    'build_system_commands': {
                        'default': {
                            'commands': [{'command': 'ninja', 'extra_args': []}]
                        }
                    },
                },
            ),
            (
                '',
                # Argparse input
                'docs python.lint -C out2 python.tests'.split(),
                # Expected changed config
                {
                    'default_build_targets': ['docs', 'python.lint'],
                    'build_directories': [['out2', 'python.tests']],
                    'build_system_commands': {
                        'default': {
                            'commands': [{'command': 'ninja', 'extra_args': []}]
                        }
                    },
                },
            ),
            (
                'Two build directories; one with build system commands the '
                'other with none defined.',
                # Argparse input
                [
                    '-C',
                    'out/gn',
                    'python.lint',
                    '-C',
                    'out/bazel',
                    '//...',
                    '--build-system-command',
                    'out/bazel',
                    'bazel build',
                    '--build-system-command',
                    'out/bazel',
                    'bazel test',
                    '--logfile',
                    'out/build.txt',
                ],
                # Expected changed config
                {
                    'default_build_targets': [],
                    'build_directories': [
                        ['out/gn', 'python.lint'],
                        ['out/bazel', '//...'],
                    ],
                    'build_system_commands': {
                        'default': {
                            'commands': [{'command': 'ninja', 'extra_args': []}]
                        },
                        'out/bazel': {
                            'commands': [
                                {
                                    'command': 'bazel',
                                    'extra_args': ['build'],
                                },
                                {
                                    'command': 'bazel',
                                    'extra_args': ['test'],
                                },
                            ],
                        },
                    },
                    'logfile': Path('out/build.txt'),
                },
            ),
        ]
    )
    def test_apply_command_line_args(
        self,
        _name,
        argparse_args,
        expected_config_changes,
    ):
        """Check command line args are applied to ProjectBuilderPrefs."""
        # Load default command line arg values.
        parser = argparse.ArgumentParser(
            formatter_class=argparse.RawDescriptionHelpFormatter,
        )
        parser = add_project_builder_arguments(parser)
        argparse_output = parser.parse_args(argparse_args)

        # Create a prefs instance with the test config file.
        prefs = ProjectBuilderPrefs(
            load_argparse_arguments=add_project_builder_arguments,
            project_file=False,
            project_user_file=False,
            user_file=False,
        )

        # Save config before apply_command_line_args
        # pylint: disable=protected-access
        expected_config = copy.deepcopy(prefs._config)

        # Run apply_command_line_args
        prefs.apply_command_line_args(argparse_output)

        # Add the expected changes to the base
        expected_config.update(expected_config_changes)

        # Check equality
        self.assertEqual(prefs._config, expected_config)
        # pylint: enable=protected-access


if __name__ == '__main__':
    unittest.main()
