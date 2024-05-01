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

from __future__ import annotations

from dataclasses import dataclass, field
import functools
import logging
from pathlib import Path
import shlex
from typing import Callable, TYPE_CHECKING

from prompt_toolkit.formatted_text import ANSI, StyleAndTextTuples
from prompt_toolkit.formatted_text.base import OneStyleAndTextTuple

from pw_presubmit.build import write_gn_args_file

if TYPE_CHECKING:
    from pw_build.project_builder import ProjectBuilder
    from pw_build.project_builder_prefs import ProjectBuilderPrefs

_LOG = logging.getLogger('pw_build.watch')


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

    build_dir: Path | None = None
    build_system_command: str | None = None
    build_system_extra_args: list[str] = field(default_factory=list)
    targets: list[str] = field(default_factory=list)
    command: list[str] = field(default_factory=list)
    run_if: Callable[[Path], bool] = lambda _build_dir: True

    def __post_init__(self) -> None:
        # Copy self._expanded_args from the command list.
        self._expanded_args: list[str] = []
        if self.command:
            self._expanded_args = self.command

    def should_run(self) -> bool:
        """Return True if this build command should be run."""
        if self.build_dir:
            return self.run_if(self.build_dir)
        return True

    def _get_starting_build_system_args(self) -> list[str]:
        """Return flags that appear immediately after the build command."""
        assert self.build_system_command
        assert self.build_dir
        return []

    def _get_build_system_args(self) -> list[str]:
        assert self.build_system_command
        assert self.build_dir

        # Both make and ninja use -C for a build directory.
        if self.make_command() or self.ninja_command():
            return ['-C', str(self.build_dir), *self.targets]

        if self.bazel_command():
            # Bazel doesn't use -C for the out directory. Instead we use
            # --symlink_prefix to save some outputs to the desired
            # location. This is the same pattern used by pw_presubmit.
            bazel_args = ['--symlink_prefix', str(self.build_dir / 'bazel-')]
            if self.bazel_clean_command():
                # Targets are unrecognized args for bazel clean
                return bazel_args
            return bazel_args + [*self.targets]

        raise UnknownBuildSystem(
            f'\n\nUnknown build system command "{self.build_system_command}" '
            f'for build directory "{self.build_dir}".\n'
            'Supported commands: ninja, bazel, make'
        )

    def _resolve_expanded_args(self) -> list[str]:
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

    def make_command(self) -> bool:
        return (
            self.build_system_command is not None
            and self.build_system_command.endswith('make')
        )

    def ninja_command(self) -> bool:
        return (
            self.build_system_command is not None
            and self.build_system_command.endswith('ninja')
        )

    def bazel_command(self) -> bool:
        return (
            self.build_system_command is not None
            and self.build_system_command.endswith('bazel')
        )

    def bazel_build_command(self) -> bool:
        return self.bazel_command() and 'build' in self.build_system_extra_args

    def bazel_test_command(self) -> bool:
        return self.bazel_command() and 'test' in self.build_system_extra_args

    def bazel_clean_command(self) -> bool:
        return self.bazel_command() and 'clean' in self.build_system_extra_args

    def get_args(
        self,
        additional_ninja_args: list[str] | None = None,
        additional_bazel_args: list[str] | None = None,
        additional_bazel_build_args: list[str] | None = None,
    ) -> list[str]:
        """Return all args required to launch this BuildCommand."""
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

        build_system_target_args = self._get_build_system_args()

        # Construct the build system command args.
        command = [
            self.build_system_command,
            *self._get_starting_build_system_args(),
            *extra_args,
            *build_system_target_args,
        ]
        return command

    def __str__(self) -> str:
        return ' '.join(shlex.quote(arg) for arg in self.get_args())


@dataclass
class BuildRecipeStatus:
    """Stores the status of a build recipe."""

    recipe: BuildRecipe
    current_step: str = ''
    percent: float = 0.0
    error_count: int = 0
    return_code: int | None = None
    flag_done: bool = False
    flag_started: bool = False
    error_lines: dict[int, list[str]] = field(default_factory=dict)

    def pending(self) -> bool:
        return self.return_code is None

    def failed(self) -> bool:
        if self.return_code is not None:
            return self.return_code != 0
        return False

    def append_failure_line(self, line: str) -> None:
        lines = self.error_lines.get(self.error_count, [])
        lines.append(line)
        self.error_lines[self.error_count] = lines

    def has_empty_ninja_errors(self) -> bool:
        for error_lines in self.error_lines.values():
            # NOTE: There will be at least 2 lines for each ninja failure:
            # - A starting 'FAILED: target' line
            # - An ending line with this format:
            #   'ninja: error: ... cannot make progress due to previous errors'

            # If the total error line count is very short, assume it's an empty
            # ninja error.
            if len(error_lines) <= 3:
                # If there is a failure in the regen step, there will be 3 error
                # lines: The above two and one more with the regen command.
                return True
            # Otherwise, if the line starts with FAILED: build.ninja the failure
            # is likely in the regen step and there will be extra cmake or gn
            # error text that was not captured.
            for line in error_lines:
                if line.startswith(
                    '\033[31mFAILED: \033[0mbuild.ninja'
                ) or line.startswith('FAILED: build.ninja'):
                    return True
        return False

    def increment_error_count(self, count: int = 1) -> None:
        self.error_count += count
        if self.error_count not in self.error_lines:
            self.error_lines[self.error_count] = []

    def should_log_failures(self) -> bool:
        return (
            self.recipe.project_builder is not None
            and self.recipe.project_builder.separate_build_file_logging
            and (not self.recipe.project_builder.send_recipe_logs_to_root)
        )

    def log_last_failure(self) -> None:
        """Log the last ninja error if available."""
        if not self.should_log_failures():
            return

        logger = self.recipe.error_logger
        if not logger:
            return

        _color = self.recipe.project_builder.color  # type: ignore

        lines = self.error_lines.get(self.error_count, [])
        _LOG.error('')
        _LOG.error(' ╔════════════════════════════════════')
        _LOG.error(
            ' ║  START %s Failure #%d:',
            _color.cyan(self.recipe.display_name),
            self.error_count,
        )

        logger.error('')
        for line in lines:
            logger.error(line)
        logger.error('')

        _LOG.error(
            ' ║  END %s Failure #%d',
            _color.cyan(self.recipe.display_name),
            self.error_count,
        )
        _LOG.error(" ╚════════════════════════════════════")
        _LOG.error('')

    def log_entire_recipe_logfile(self) -> None:
        """Log the entire build logfile if no ninja errors available."""
        if not self.should_log_failures():
            return

        recipe_logfile = self.recipe.logfile
        if not recipe_logfile:
            return

        _color = self.recipe.project_builder.color  # type: ignore

        logfile_path = str(recipe_logfile.resolve())

        _LOG.error('')
        _LOG.error(' ╔════════════════════════════════════')
        _LOG.error(
            ' ║  %s Failure; Entire log below:',
            _color.cyan(self.recipe.display_name),
        )
        _LOG.error(' ║  %s %s', _color.yellow('START'), logfile_path)

        logger = self.recipe.error_logger
        if not logger:
            return

        logger.error('')
        for line in recipe_logfile.read_text(
            encoding='utf-8', errors='ignore'
        ).splitlines():
            logger.error(line)
        logger.error('')

        _LOG.error(' ║  %s %s', _color.yellow('END'), logfile_path)
        _LOG.error(" ╚════════════════════════════════════")
        _LOG.error('')

    def status_slug(self, restarting: bool = False) -> OneStyleAndTextTuple:
        status = ('', '')
        if not self.recipe.enabled:
            return ('fg:ansidarkgray', 'Disabled')

        waiting = False
        if self.done:
            if self.passed():
                status = ('fg:ansigreen', 'OK      ')
            elif self.failed():
                status = ('fg:ansired', 'FAIL    ')
        elif self.started:
            status = ('fg:ansiyellow', 'Building')
        else:
            waiting = True
            status = ('default', 'Waiting ')

        # Only show Aborting if the process is building (or has failures).
        if restarting and not waiting and not self.passed():
            status = ('fg:ansiyellow', 'Aborting')
        return status

    def current_step_formatted(self) -> StyleAndTextTuples:
        formatted_text: StyleAndTextTuples = []
        if self.passed():
            return formatted_text

        if self.current_step:
            if '\x1b' in self.current_step:
                formatted_text = ANSI(self.current_step).__pt_formatted_text__()
            else:
                formatted_text = [('', self.current_step)]

        return formatted_text

    @property
    def done(self) -> bool:
        return self.flag_done

    @property
    def started(self) -> bool:
        return self.flag_started

    def mark_done(self) -> None:
        self.flag_done = True

    def mark_started(self) -> None:
        self.flag_started = True

    def set_failed(self) -> None:
        self.flag_done = True
        self.return_code = -1

    def set_passed(self) -> None:
        self.flag_done = True
        self.return_code = 0

    def passed(self) -> bool:
        if self.done and self.return_code is not None:
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
        auto_create_build_dir: Auto create the build directory and all necessary
            parent directories before running any build commands.
    """

    build_dir: Path
    steps: list[BuildCommand] = field(default_factory=list)
    title: str | None = None
    enabled: bool = True
    auto_create_build_dir: bool = True

    def __hash__(self):
        return hash((self.build_dir, self.title, len(self.steps)))

    def __post_init__(self) -> None:
        # Update all included steps to use this recipe's build_dir.
        for step in self.steps:
            if self.build_dir:
                step.build_dir = self.build_dir

        # Set logging variables
        self._logger: logging.Logger | None = None
        self.error_logger: logging.Logger | None = None
        self._logfile: Path | None = None
        self._status: BuildRecipeStatus = BuildRecipeStatus(self)
        self.project_builder: ProjectBuilder | None = None

    def toggle_enabled(self) -> None:
        self.enabled = not self.enabled

    def set_project_builder(self, project_builder) -> None:
        self.project_builder = project_builder

    def set_targets(self, new_targets: list[str]) -> None:
        """Reset all build step targets."""
        for step in self.steps:
            step.targets = new_targets

    def set_logger(self, logger: logging.Logger) -> None:
        self._logger = logger

    def set_error_logger(self, logger: logging.Logger) -> None:
        self.error_logger = logger

    def set_logfile(self, log_file: Path) -> None:
        self._logfile = log_file

    def reset_status(self) -> None:
        self._status = BuildRecipeStatus(self)

    @property
    def status(self) -> BuildRecipeStatus:
        return self._status

    @property
    def log(self) -> logging.Logger:
        if self._logger:
            return self._logger
        return logging.getLogger()

    @property
    def logfile(self) -> Path | None:
        return self._logfile

    @property
    def display_name(self) -> str:
        if self.title:
            return self.title
        return str(self.build_dir)

    def targets(self) -> list[str]:
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


def create_build_recipes(prefs: ProjectBuilderPrefs) -> list[BuildRecipe]:
    """Create a list of BuildRecipes from ProjectBuilderPrefs."""
    build_recipes: list[BuildRecipe] = []

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
        steps: list[BuildCommand] = []
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


def should_gn_gen(out: Path) -> bool:
    """Returns True if the gn gen command should be run.

    Returns True if ``build.ninja`` or ``args.gn`` files are missing from the
    build directory.
    """
    # gn gen only needs to run if build.ninja or args.gn files are missing.
    expected_files = [
        out / 'build.ninja',
        out / 'args.gn',
    ]
    return any(not gen_file.is_file() for gen_file in expected_files)


def should_gn_gen_with_args(
    gn_arg_dict: dict[str, bool | str | list | tuple]
) -> Callable:
    """Returns a callable which writes an args.gn file prior to checks.

    Args:
      gn_arg_dict: Dictionary of key value pairs to use as gn args.

    Returns:
      Callable which takes a single Path argument and returns a bool
      for True if the gn gen command should be run.

    The returned function will:

    1. Always re-write the ``args.gn`` file.
    2. Return True if ``build.ninja`` or ``args.gn`` files are missing.
    """

    def _write_args_and_check(out: Path) -> bool:
        # Always re-write the args.gn file.
        write_gn_args_file(out / 'args.gn', **gn_arg_dict)

        return should_gn_gen(out)

    return _write_args_and_check


def _should_regenerate_cmake(
    cmake_generate_command: list[str], out: Path
) -> bool:
    """Save the full cmake command to a file.

    Returns True if cmake files should be regenerated.
    """
    _should_regenerate = True
    cmake_command = ' '.join(cmake_generate_command)
    cmake_command_filepath = out / 'cmake_cfg_command.txt'
    if (out / 'build.ninja').is_file() and cmake_command_filepath.is_file():
        if cmake_command == cmake_command_filepath.read_text():
            _should_regenerate = False

    if _should_regenerate:
        out.mkdir(parents=True, exist_ok=True)
        cmake_command_filepath.write_text(cmake_command)

    return _should_regenerate


def should_regenerate_cmake(
    cmake_generate_command: list[str],
) -> Callable[[Path], bool]:
    """Return a callable to determine if cmake should be regenerated.

    Args:
      cmake_generate_command: Full list of args to run cmake.

    The returned function will return True signaling CMake should be re-run if:

    1. The provided CMake command does not match an existing args in the
       ``cmake_cfg_command.txt`` file in the build dir.
    2. ``build.ninja`` is missing or ``cmake_cfg_command.txt`` is missing.

    When the function is run it will create the build directory if needed and
    write the cmake_generate_command args to the ``cmake_cfg_command.txt`` file.
    """
    return functools.partial(_should_regenerate_cmake, cmake_generate_command)
