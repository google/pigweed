# Copyright 2023 The Pigweed Authors
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
"""Default pw build script for upstream Pigweed.

Usage:

   pw build --list
   pw build --recipe default_ninja
   pw build --recipe default_* --watch
   pw build --step gn_combined_build_check --step gn_python_*
   pw build -C out/gn --watch
"""

import logging
from pathlib import Path
import shutil
import sys

import pw_presubmit.pigweed_presubmit
from pw_presubmit.build import gn_args

from pw_build.build_recipe import (
    BuildCommand,
    BuildRecipe,
)
from pw_build.project_builder_presubmit_runner import (
    should_gn_gen,
    main,
)


_LOG = logging.getLogger('pw_build')


def pigweed_upstream_main() -> int:
    """Entry point for Pigweed upstream ``pw build`` command.

    Defines one or more BuildRecipes and passes that along with all of Pigweed
    upstream presubmit programs to the project_builder_presubmit_runner.main to
    start a pw build invocation.

    Returns:
      An int representing the success or failure status of the build; 0 if
      successful, 1 if failed.

    Command line usage examples:

    .. code-block:: bash

       pw build --list
       pw build --recipe default_ninja
       pw build --recipe default_* --watch
       pw build --step gn_combined_build_check --step gn_python_*
       pw build -C out/gn --watch
    """
    default_bazel_targets = ['//...:all']

    default_gn_gen_command = [
        'gn',
        'gen',
        '{build_dir}',
        '--export-compile-commands',
    ]
    if shutil.which('ccache'):
        default_gn_gen_command.append(gn_args(pw_command_launcher='ccache'))

    build_recipes = [
        # Ninja build
        BuildRecipe(
            build_dir=Path('out/gn'),
            title='default_ninja',
            steps=[
                BuildCommand(
                    run_if=should_gn_gen,
                    command=default_gn_gen_command,
                ),
                BuildCommand(
                    build_system_command='ninja',
                    targets=['default'],
                ),
            ],
        ),
        # Bazel build
        BuildRecipe(
            build_dir=Path('out/bazel'),
            title='default_bazel',
            steps=[
                BuildCommand(
                    build_system_command='bazel',
                    build_system_extra_args=[
                        'build',
                        '--verbose_failures',
                        '--worker_verbose',
                    ],
                    targets=default_bazel_targets,
                ),
                BuildCommand(
                    build_system_command='bazel',
                    build_system_extra_args=[
                        'test',
                        '--test_output=errors',
                    ],
                    targets=default_bazel_targets,
                ),
            ],
        ),
    ]

    return main(
        presubmit_programs=pw_presubmit.pigweed_presubmit.PROGRAMS,
        build_recipes=build_recipes,
        default_root_logfile=Path('out/build.txt'),
    )


if __name__ == '__main__':
    sys.exit(pigweed_upstream_main())
