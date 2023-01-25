#!/usr/bin/env python
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
"""Watch build config dataclasses."""

from dataclasses import dataclass, field
import logging
from pathlib import Path
import shlex
from typing import Callable, List, Optional, TYPE_CHECKING

if TYPE_CHECKING:
    from pw_build.project_builder_prefs import ProjectBuilderPrefs


class UnknownBuildSystem(Exception):
    """Exception for requesting unsupported build systems."""


class UnknownBuildDir(Exception):
    """Exception for an unknown build dir before command running."""


@dataclass
class BuildCommand:
    """Store details of a single build step.

    Example usage:

    .. code-block:: python

        from pw_build.build_recipe import BuildCommand, BuildRecipe

        def should_gen_gn(out: Path):
            return not (out / 'build.ninja').is_file()

        cmd1 = BuildCommand(build_dir='out',
                            command=['gn', 'gen', '{build_dir}'],
                            run_if=should_gen_gn)

        cmd2 = BuildCommand(build_dir='out',
                            build_system_command='ninja',
                            build_system_extra_args=['-k', '0'],
                            targets=['default']),

    Args:
        build_dir: Output directory for this build command. This can be omitted
            if the BuildCommand is included in the steps of a BuildRecipe.
        build_system_command: This command should end with ``ninja``, ``make``,
            or ``bazel``.
        build_system_extra_args: A list of extra arguments passed to the
            build_system_command. If running ``bazel test`` include ``test`` as
            an extra arg here.
        targets: Optional list of targets to build in the build_dir.
        command: List of strings to run as a command. These are passed to
            subprocess.run(). Any instances of the ``'{build_dir}'`` string
            literal will be replaced at run time with the out directory.
        run_if: A callable function to run before executing this
            BuildCommand. The callable takes one Path arg for the build_dir. If
            the callable returns true this command is executed. All
            BuildCommands are run by default.
    """

    build_dir: Optional[Path] = None
    build_system_command: Optional[str] = None
    build_system_extra_args: List[str] = field(default_factory=list)
    targets: List[str] = field(default_factory=list)
    command: List[str] = field(default_factory=list)
    run_if: Callable[[Path], bool] = lambda _build_dir: True

    def __post_init__(self) -> None:
        # Copy self._expanded_args from the command list.
        self._expanded_args: List[str] = []
        if self.command:
            self._expanded_args = self.command

    def should_run(self) -> bool:
        """Return True if this build command should be run."""
        if self.build_dir:
            return self.run_if(self.build_dir)
        return True

    def _get_starting_build_system_args(self) -> List[str]:
        """Return flags that appear immediately after the build command."""
        assert self.build_system_command
        assert self.build_dir
        if self.build_system_command.endswith('bazel'):
            return ['--output_base', str(self.build_dir)]
        return []

    def _get_build_system_args(self) -> List[str]:
        assert self.build_system_command
        assert self.build_dir

        # Both make and ninja use -C for a build directory.
        if self.build_system_command.endswith(
            'make'
        ) or self.build_system_command.endswith('ninja'):
            return ['-C', str(self.build_dir), *self.targets]

        # Bazel relies on --output_base which is handled by the
        # _get_starting_build_system_args() function.
        if self.build_system_command.endswith('bazel'):
            return [*self.targets]

        raise UnknownBuildSystem(
            f'\n\nUnknown build system command "{self.build_system_command}" '
            f'for build directory "{self.build_dir}".\n'
            'Supported commands: ninja, bazel, make'
        )

    def _resolve_expanded_args(self) -> List[str]:
        """Replace instances of '{build_dir}' with the self.build_dir."""
        resolved_args = []
        for arg in self._expanded_args:
            if arg == "{build_dir}":
                if not self.build_dir:
                    raise UnknownBuildDir(
                        '\n\nUnknown "{build_dir}" value for command:\n'
                        f'  {self._expanded_args}\n'
                        f'In BuildCommand: {repr(self)}\n\n'
                        'Check build_dir is set for the above BuildCommand'
                        'or included as a step to a BuildRecipe.'
                    )
                resolved_args.append(str(self.build_dir.resolve()))
            else:
                resolved_args.append(arg)
        return resolved_args

    def ninja_command(self) -> bool:
        if self.build_system_command and self.build_system_command.endswith(
            'ninja'
        ):
            return True
        return False

    def bazel_command(self) -> bool:
        if self.build_system_command and self.build_system_command.endswith(
            'bazel'
        ):
            return True
        return False

    def bazel_build_command(self) -> bool:
        if self.bazel_command() and 'build' in self.build_system_extra_args:
            return True
        return False

    def get_args(
        self,
        additional_ninja_args: Optional[List[str]] = None,
        additional_bazel_args: Optional[List[str]] = None,
        additional_bazel_build_args: Optional[List[str]] = None,
    ) -> List[str]:
        # If this is a plain command step, return self._expanded_args as-is.
        if not self.build_system_command:
            return self._resolve_expanded_args()

        # Assmemble user-defined extra args.
        extra_args = []
        extra_args.extend(self.build_system_extra_args)
        if additional_ninja_args and self.ninja_command():
            extra_args.extend(additional_ninja_args)

        if additional_bazel_build_args and self.bazel_build_command():
            extra_args.extend(additional_bazel_build_args)

        if additional_bazel_args and self.bazel_command():
            extra_args.extend(additional_bazel_args)

        # Construct the build system command args.
        command = [
            self.build_system_command,
            *self._get_starting_build_system_args(),
            *extra_args,
            *self._get_build_system_args(),
        ]
        return command

    def __str__(self) -> str:
        return ' '.join(shlex.quote(arg) for arg in self.get_args())


@dataclass
class BuildRecipeStatus:
    current_step: str = ''
    percent: float = 0.0
    error_count: int = 0
    return_code: Optional[int] = None

    def pending(self) -> bool:
        return self.return_code is None

    def failed(self) -> bool:
        if self.return_code is not None:
            return self.return_code != 0
        return False

    def set_passed(self) -> None:
        self.return_code = 0

    def passed(self) -> bool:
        if self.return_code is not None:
            return self.return_code == 0
        return False


@dataclass
class BuildRecipe:
    """Dataclass to store a list of BuildCommands.

    Example usage:

    .. code-block:: python

        from pw_build.build_recipe import BuildCommand, BuildRecipe

        def should_gen_gn(out: Path) -> bool:
            return not (out / 'build.ninja').is_file()

        recipe = BuildRecipe(
            build_dir='out',
            title='Vanilla Ninja Build',
            steps=[
                BuildCommand(command=['gn', 'gen', '{build_dir}'],
                             run_if=should_gen_gn),
                BuildCommand(build_system_command='ninja',
                             build_system_extra_args=['-k', '0'],
                             targets=['default']),
            ],
        )

    Args:
        build_dir: Output directory for this BuildRecipe. On init this out dir
            is set for all included steps.
        steps: List of BuildCommands to run.
        title: Custom title. The build_dir is used if this is ommited.
    """

    build_dir: Path
    steps: List[BuildCommand] = field(default_factory=list)
    title: Optional[str] = None

    def __hash__(self):
        return hash((self.build_dir, self.title, len(self.steps)))

    def __post_init__(self) -> None:
        # Update all included steps to use this recipe's build_dir.
        for step in self.steps:
            if self.build_dir:
                step.build_dir = self.build_dir

        # Set logging variables
        self._logger: Optional[logging.Logger] = None
        self._logfile: Optional[Path] = None
        self._status: BuildRecipeStatus = BuildRecipeStatus()

    def set_targets(self, new_targets: List[str]) -> None:
        """Reset all build step targets."""
        for step in self.steps:
            step.targets = new_targets

    def set_logger(self, logger: logging.Logger) -> None:
        self._logger = logger

    def set_logfile(self, log_file: Path) -> None:
        self._logfile = log_file

    def reset_status(self) -> None:
        self._status = BuildRecipeStatus()

    @property
    def status(self) -> BuildRecipeStatus:
        return self._status

    @property
    def log(self) -> logging.Logger:
        if self._logger:
            return self._logger
        return logging.getLogger()

    @property
    def logfile(self) -> Optional[Path]:
        return self._logfile

    @property
    def display_name(self) -> str:
        if self.title:
            return self.title
        return str(self.build_dir)

    def targets(self) -> List[str]:
        return list(
            set(target for step in self.steps for target in step.targets)
        )

    def __str__(self) -> str:
        message = self.display_name
        targets = self.targets()
        if targets:
            target_list = ' '.join(self.targets())
            message = f'{message} -- {target_list}'
        return message


def create_build_recipes(prefs: 'ProjectBuilderPrefs') -> List[BuildRecipe]:
    """Create a list of BuildRecipes from ProjectBuilderPrefs."""
    build_recipes: List[BuildRecipe] = []

    if prefs.run_commands:
        for command_str in prefs.run_commands:
            build_recipes.append(
                BuildRecipe(
                    build_dir=Path.cwd(),
                    steps=[BuildCommand(command=shlex.split(command_str))],
                    title=command_str,
                )
            )

    for build_dir, targets in prefs.build_directories.items():
        steps: List[BuildCommand] = []
        build_path = Path(build_dir)
        if not targets:
            targets = []

        for (
            build_system_command,
            build_system_extra_args,
        ) in prefs.build_system_commands(build_dir):
            steps.append(
                BuildCommand(
                    build_system_command=build_system_command,
                    build_system_extra_args=build_system_extra_args,
                    targets=targets,
                )
            )

        build_recipes.append(
            BuildRecipe(
                build_dir=build_path,
                steps=steps,
            )
        )

    return build_recipes
