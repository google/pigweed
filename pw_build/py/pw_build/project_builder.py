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
"""Build a Pigweed Project.

Run arbitrary commands or invoke build systems (Ninja, Bazel and make) on one or
more build directories.

Examples:

  # Build the default target in out/ using ninja.
  python -m pw_build.project_builder -C out

  # Build pw_run_tests.modules in the out/cmake directory
  python -m pw_build.project_builder -C out/cmake pw_run_tests.modules

  # Build the default target in out/ and pw_apps in out/cmake
  python -m pw_build.project_builder -C out -C out/cmake pw_apps

  # Build python.tests in out/ and pw_apps in out/cmake/
  python -m pw_build.project_builder python.tests -C out/cmake pw_apps

  # Run 'bazel build' and 'bazel test' on the target '//...' in outbazel/
  python -m pw_build.project_builder --run-command 'mkdir -p outbazel'
  -C outbazel '//...'
  --build-system-command outbazel 'bazel build'
  --build-system-command outbazel 'bazel test'
"""

from __future__ import annotations

import argparse
import concurrent.futures
import os
import logging
from pathlib import Path
import re
import shlex
import sys
import subprocess
import time
from typing import (
    Callable,
    Generator,
    NoReturn,
    Sequence,
    NamedTuple,
)

from prompt_toolkit.patch_stdout import StdoutProxy

import pw_cli.env
import pw_cli.log

from pw_build.build_recipe import BuildRecipe, create_build_recipes
from pw_build.project_builder_argparse import add_project_builder_arguments
from pw_build.project_builder_context import get_project_builder_context
from pw_build.project_builder_prefs import ProjectBuilderPrefs

_COLOR = pw_cli.color.colors()
_LOG = logging.getLogger('pw_build')

BUILDER_CONTEXT = get_project_builder_context()

PASS_MESSAGE = """
  ██████╗  █████╗ ███████╗███████╗██╗
  ██╔══██╗██╔══██╗██╔════╝██╔════╝██║
  ██████╔╝███████║███████╗███████╗██║
  ██╔═══╝ ██╔══██║╚════██║╚════██║╚═╝
  ██║     ██║  ██║███████║███████║██╗
  ╚═╝     ╚═╝  ╚═╝╚══════╝╚══════╝╚═╝
"""

# Pick a visually-distinct font from "PASS" to ensure that readers can't
# possibly mistake the difference between the two states.
FAIL_MESSAGE = """
   ▄██████▒░▄▄▄       ██▓  ░██▓
  ▓█▓     ░▒████▄    ▓██▒  ░▓██▒
  ▒████▒   ░▒█▀  ▀█▄  ▒██▒ ▒██░
  ░▓█▒    ░░██▄▄▄▄██ ░██░  ▒██░
  ░▒█░      ▓█   ▓██▒░██░░ ████████▒
   ▒█░      ▒▒   ▓▒█░░▓  ░  ▒░▓  ░
   ░▒        ▒   ▒▒ ░ ▒ ░░  ░ ▒  ░
   ░ ░       ░   ▒    ▒ ░   ░ ░
                 ░  ░ ░       ░  ░
"""


class ProjectBuilderCharset(NamedTuple):
    slug_ok: str
    slug_fail: str
    slug_building: str


ASCII_CHARSET = ProjectBuilderCharset(
    _COLOR.green('OK  '),
    _COLOR.red('FAIL'),
    _COLOR.yellow('... '),
)
EMOJI_CHARSET = ProjectBuilderCharset('✔️ ', '❌', '⏱️ ')


def _exit(*args) -> NoReturn:
    _LOG.critical(*args)
    sys.exit(1)


def _exit_due_to_interrupt() -> None:
    """Abort function called when not using progress bars."""
    # To keep the log lines aligned with each other in the presence of
    # a '^C' from the keyboard interrupt, add a newline before the log.
    print()
    _LOG.info('Got Ctrl-C; exiting...')
    BUILDER_CONTEXT.ctrl_c_interrupt()


_NINJA_BUILD_STEP = re.compile(
    # Start of line
    r'^'
    # Step count: [1234/5678]
    r'\[(?P<step>[0-9]+)/(?P<total_steps>[0-9]+)\]'
    # whitespace
    r' *'
    # Build step text
    r'(?P<action>.*)$'
)

_BAZEL_BUILD_STEP = re.compile(
    # Start of line
    r'^'
    # Optional starting green color
    r'(?:\x1b\[32m)?'
    # Step count: [1,234 / 5,678]
    r'\[(?P<step>[0-9,]+) */ *(?P<total_steps>[0-9,]+)\]'
    # Optional ending clear color and space
    r'(?:\x1b\[0m)? *'
    # Build step text
    r'(?P<action>.*)$'
)

_NINJA_FAILURE = re.compile(
    # Start of line
    r'^'
    # Optional red color
    r'(?:\x1b\[31m)?'
    r'FAILED:'
    r' '
    # Optional color reset
    r'(?:\x1b\[0m)?'
)

_BAZEL_FAILURE = re.compile(
    # Start of line
    r'^'
    # Optional red color
    r'(?:\x1b\[31m)?'
    # Optional bold color
    r'(?:\x1b\[1m)?'
    r'FAIL:'
    # Optional color reset
    r'(?:\x1b\[0m)?'
    # whitespace
    r' *'
    r'.*bazel-out.*'
)

_BAZEL_ELAPSED_TIME = re.compile(
    # Start of line
    r'^'
    # Optional green color
    r'(?:\x1b\[32m)?'
    r'INFO:'
    # single space
    r' '
    # Optional color reset
    r'(?:\x1b\[0m)?'
    r'Elapsed time:'
)


def execute_command_no_logging(
    command: list,
    env: dict,
    recipe: BuildRecipe,
    # pylint: disable=unused-argument
    logger: logging.Logger = _LOG,
    line_processed_callback: Callable[[BuildRecipe], None] | None = None,
    # pylint: enable=unused-argument
) -> bool:
    print()
    proc = subprocess.Popen(command, env=env, errors='replace')
    BUILDER_CONTEXT.register_process(recipe, proc)
    returncode = None
    while returncode is None:
        if BUILDER_CONTEXT.build_stopping():
            try:
                proc.terminate()
            except ProcessLookupError:
                # Process already stopped.
                pass
        returncode = proc.poll()
        time.sleep(0.05)
    print()
    recipe.status.return_code = returncode

    return proc.returncode == 0


def execute_command_with_logging(
    command: list,
    env: dict,
    recipe: BuildRecipe,
    logger: logging.Logger = _LOG,
    line_processed_callback: Callable[[BuildRecipe], None] | None = None,
) -> bool:
    """Run a command in a subprocess and log all output."""
    current_stdout = ''
    returncode = None

    starting_failure_regex = _NINJA_FAILURE
    ending_failure_regex = _NINJA_BUILD_STEP
    build_step_regex = _NINJA_BUILD_STEP

    if command[0].endswith('ninja'):
        build_step_regex = _NINJA_BUILD_STEP
        starting_failure_regex = _NINJA_FAILURE
        ending_failure_regex = _NINJA_BUILD_STEP
    elif command[0].endswith('bazel'):
        build_step_regex = _BAZEL_BUILD_STEP
        starting_failure_regex = _BAZEL_FAILURE
        ending_failure_regex = _BAZEL_ELAPSED_TIME

    with subprocess.Popen(
        command,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        errors='replace',
    ) as proc:
        BUILDER_CONTEXT.register_process(recipe, proc)
        should_log_build_steps = BUILDER_CONTEXT.log_build_steps
        # Empty line at the start.
        logger.info('')

        failure_line = False
        while returncode is None:
            output = ''
            error_output = ''

            if proc.stdout:
                output = proc.stdout.readline()
                current_stdout += output
            if proc.stderr:
                error_output += proc.stderr.readline()
                current_stdout += error_output

            if not output and not error_output:
                returncode = proc.poll()
                continue

            line_match_result = build_step_regex.match(output)
            if line_match_result:
                if failure_line and not BUILDER_CONTEXT.build_stopping():
                    recipe.status.log_last_failure()
                failure_line = False
                matches = line_match_result.groupdict()
                recipe.status.current_step = line_match_result.group(0)
                recipe.status.percent = 0.0
                # Remove commas from step and total_steps strings
                step = int(matches.get('step', '0').replace(',', ''))
                total_steps = int(
                    matches.get('total_steps', '1').replace(',', '')
                )
                if total_steps > 0:
                    recipe.status.percent = float(step / total_steps)

            logger_method = logger.info
            if starting_failure_regex.match(output):
                logger_method = logger.error
                if failure_line and not BUILDER_CONTEXT.build_stopping():
                    recipe.status.log_last_failure()
                recipe.status.increment_error_count()
                failure_line = True
            elif ending_failure_regex.match(output):
                if failure_line and not BUILDER_CONTEXT.build_stopping():
                    recipe.status.log_last_failure()
                failure_line = False

            # Mypy output mixes character encoding in color coded output
            # and uses the 'sgr0' (or exit_attribute_mode) capability from the
            # host machine's terminfo database.
            #
            # This can result in this sequence ending up in STDOUT as
            # b'\x1b(B\x1b[m'. (B tells terminals to interpret text as
            # USASCII encoding but will appear in prompt_toolkit as a B
            # character.
            #
            # The following replace calls will strip out those
            # sequences.
            stripped_output = output.replace('\x1b(B', '').strip()

            # If this isn't a build step.
            if not line_match_result or (
                # Or if this is a build step and logging build steps is
                # requested:
                line_match_result
                and should_log_build_steps
            ):
                # Log this line.
                logger_method(stripped_output)
            recipe.status.current_step = stripped_output

            if failure_line:
                recipe.status.append_failure_line(stripped_output)

            BUILDER_CONTEXT.redraw_progress()

            if line_processed_callback:
                line_processed_callback(recipe)

            if BUILDER_CONTEXT.build_stopping():
                try:
                    proc.terminate()
                except ProcessLookupError:
                    # Process already stopped.
                    pass

        recipe.status.return_code = returncode

        # Log the last failure if not done already
        if failure_line and not BUILDER_CONTEXT.build_stopping():
            recipe.status.log_last_failure()

        # Empty line at the end.
        logger.info('')

    return returncode == 0


def log_build_recipe_start(
    index_message: str,
    project_builder: ProjectBuilder,
    cfg: BuildRecipe,
    logger: logging.Logger = _LOG,
) -> None:
    """Log recipe start and truncate the build logfile."""
    if project_builder.separate_build_file_logging and cfg.logfile:
        # Truncate the file
        with open(cfg.logfile, 'w'):
            pass

    BUILDER_CONTEXT.mark_progress_started(cfg)

    build_start_msg = [
        index_message,
        project_builder.color.cyan('Starting ==>'),
        project_builder.color.blue('Recipe:'),
        str(cfg.display_name),
        project_builder.color.blue('Targets:'),
        str(' '.join(cfg.targets())),
    ]

    if cfg.logfile:
        build_start_msg.extend(
            [
                project_builder.color.blue('Logfile:'),
                str(cfg.logfile.resolve()),
            ]
        )
    build_start_str = ' '.join(build_start_msg)

    # Log start to the root log if recipe logs are not sent.
    if not project_builder.send_recipe_logs_to_root:
        logger.info(build_start_str)
    if cfg.logfile:
        cfg.log.info(build_start_str)


def log_build_recipe_finish(
    index_message: str,
    project_builder: ProjectBuilder,
    cfg: BuildRecipe,
    logger: logging.Logger = _LOG,
) -> None:
    """Log recipe finish and any build errors."""

    BUILDER_CONTEXT.mark_progress_done(cfg)

    if BUILDER_CONTEXT.interrupted():
        level = logging.WARNING
        tag = project_builder.color.yellow('(ABORT)')
    elif cfg.status.failed():
        level = logging.ERROR
        tag = project_builder.color.red('(FAIL)')
    else:
        level = logging.INFO
        tag = project_builder.color.green('(OK)')

    build_finish_msg = [
        level,
        '%s %s %s %s %s',
        index_message,
        project_builder.color.cyan('Finished ==>'),
        project_builder.color.blue('Recipe:'),
        cfg.display_name,
        tag,
    ]

    # Log finish to the root log if recipe logs are not sent.
    if not project_builder.send_recipe_logs_to_root:
        logger.log(*build_finish_msg)
    if cfg.logfile:
        cfg.log.log(*build_finish_msg)

    if (
        not BUILDER_CONTEXT.build_stopping()
        and cfg.status.failed()
        and (cfg.status.error_count == 0 or cfg.status.has_empty_ninja_errors())
    ):
        cfg.status.log_entire_recipe_logfile()


class MissingGlobalLogfile(Exception):
    """Exception raised if a global logfile is not specifed."""


class DispatchingFormatter(logging.Formatter):
    """Dispatch log formatting based on the logger name."""

    def __init__(self, formatters, default_formatter):
        self._formatters = formatters
        self._default_formatter = default_formatter
        super().__init__()

    def format(self, record):
        logger = logging.getLogger(record.name)
        formatter = self._formatters.get(logger.name, self._default_formatter)
        return formatter.format(record)


class ProjectBuilder:  # pylint: disable=too-many-instance-attributes
    """Pigweed Project Builder

    Controls how build recipes are executed and logged.

    Example usage:

    .. code-block:: python

        import logging
        from pathlib import Path

        from pw_build.build_recipe import BuildCommand, BuildRecipe
        from pw_build.project_builder import ProjectBuilder

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

        project_builder = ProjectBuilder(
            build_recipes=[recipe1, ...]
            banners=True,
            log_level=logging.INFO
            separate_build_file_logging=True,
            root_logger=logging.getLogger(),
            root_logfile=Path('build_log.txt'),
        )

    Args:
        build_recipes: List of build recipes.
        jobs: The number of jobs bazel, make, and ninja should use by passing
            ``-j`` to each.
        keep_going: If True keep going flags are passed to bazel and ninja with
            the ``-k`` option.
        banners: Print the project banner at the start of each build.
        allow_progress_bars: If False progress bar output will be disabled.
        log_build_steps: If True all build step lines will be logged to the
            screen and logfiles. Default: False.
        colors: Print ANSI colors to stdout and logfiles
        log_level: Optional log_level, defaults to logging.INFO.
        root_logfile: Optional root logfile.
        separate_build_file_logging: If True separate logfiles will be created
            per build recipe. The location of each file depends on if a
            ``root_logfile`` is provided. If a root logfile is used each build
            recipe logfile will be created in the same location. If no
            root_logfile is specified the per build log files are placed in each
            build dir as ``log.txt``
        send_recipe_logs_to_root: If True will send all build recipie output to
            the root logger. This only makes sense to use if the builds are run
            in serial.
        use_verbatim_error_log_formatting: Use a blank log format when printing
            errors from sub builds to the root logger.
    """

    def __init__(
        # pylint: disable=too-many-arguments,too-many-locals
        self,
        build_recipes: Sequence[BuildRecipe],
        jobs: int | None = None,
        banners: bool = True,
        keep_going: bool = False,
        abort_callback: Callable = _exit,
        execute_command: Callable[
            [list, dict, BuildRecipe, logging.Logger, Callable | None], bool
        ] = execute_command_no_logging,
        charset: ProjectBuilderCharset = ASCII_CHARSET,
        colors: bool = True,
        separate_build_file_logging: bool = False,
        send_recipe_logs_to_root: bool = False,
        root_logger: logging.Logger = _LOG,
        root_logfile: Path | None = None,
        log_level: int = logging.INFO,
        allow_progress_bars: bool = True,
        use_verbatim_error_log_formatting: bool = False,
        log_build_steps: bool = False,
    ):
        self.charset: ProjectBuilderCharset = charset
        self.abort_callback = abort_callback
        # Function used to run subprocesses
        self.execute_command = execute_command
        self.banners = banners
        self.build_recipes = build_recipes
        self.max_name_width = max(
            [len(str(step.display_name)) for step in self.build_recipes]
        )
        # Set project_builder reference in each recipe.
        for recipe in self.build_recipes:
            recipe.set_project_builder(self)

        # Save build system args
        self.extra_ninja_args: list[str] = []
        self.extra_bazel_args: list[str] = []
        self.extra_bazel_build_args: list[str] = []

        # Handle jobs and keep going flags.
        if jobs:
            job_args = ['-j', f'{jobs}']
            self.extra_ninja_args.extend(job_args)
            self.extra_bazel_build_args.extend(job_args)
        if keep_going:
            self.extra_ninja_args.extend(['-k', '0'])
            self.extra_bazel_build_args.extend(['-k'])

        self.colors = colors
        # Reference to pw_cli.color, will return colored text if colors are
        # enabled.
        self.color = pw_cli.color.colors(colors)

        # Pass color setting to bazel
        if colors:
            self.extra_bazel_args.append('--color=yes')
        else:
            self.extra_bazel_args.append('--color=no')

        # Progress bar enable/disable flag
        self.allow_progress_bars = allow_progress_bars
        self.log_build_steps = log_build_steps
        self.stdout_proxy: StdoutProxy | None = None

        # Logger configuration
        self.root_logger = root_logger
        self.default_logfile = root_logfile
        self.default_log_level = log_level
        # Create separate logs per build
        self.separate_build_file_logging = separate_build_file_logging
        # Propagate logs to the root looger
        self.send_recipe_logs_to_root = send_recipe_logs_to_root

        # Setup the error logger
        self.use_verbatim_error_log_formatting = (
            use_verbatim_error_log_formatting
        )
        self.error_logger = logging.getLogger(f'{root_logger.name}.errors')
        self.error_logger.setLevel(log_level)
        self.error_logger.propagate = True
        for recipe in self.build_recipes:
            recipe.set_error_logger(self.error_logger)

        # Copy of the standard Pigweed style log formatter, used by default if
        # no formatter exists on the root logger.
        timestamp_fmt = self.color.black_on_white('%(asctime)s') + ' '
        self.default_log_formatter = logging.Formatter(
            timestamp_fmt + '%(levelname)s %(message)s', '%Y%m%d %H:%M:%S'
        )

        # Empty log formatter (optionally used for error reporting)
        self.blank_log_formatter = logging.Formatter('%(message)s')

        # Setup the default log handler and inherit user defined formatting on
        # the root_logger.
        self.apply_root_log_formatting()

        # Create a root logfile to save what is normally logged to stdout.
        if root_logfile:
            # Execute subprocesses and capture logs
            self.execute_command = execute_command_with_logging

            root_logfile.parent.mkdir(parents=True, exist_ok=True)

            build_log_filehandler = logging.FileHandler(
                root_logfile,
                encoding='utf-8',
                # Truncate the file
                mode='w',
            )
            build_log_filehandler.setLevel(log_level)
            build_log_filehandler.setFormatter(self.dispatching_log_formatter)
            root_logger.addHandler(build_log_filehandler)

        # Set each recipe to use the root logger by default.
        for recipe in self.build_recipes:
            recipe.set_logger(root_logger)

        # Create separate logfiles per build
        if separate_build_file_logging:
            self._create_per_build_logfiles()

    def _create_per_build_logfiles(self) -> None:
        """Create separate log files per build.

        If a root logfile is used, create per build log files in the same
        location. If no root logfile is specified create the per build log files
        in the build dir as ``log.txt``
        """
        self.execute_command = execute_command_with_logging

        for recipe in self.build_recipes:
            sub_logger_name = recipe.display_name.replace('.', '_')
            new_logger = logging.getLogger(
                f'{self.root_logger.name}.{sub_logger_name}'
            )
            new_logger.setLevel(self.default_log_level)
            new_logger.propagate = self.send_recipe_logs_to_root

            new_logfile_dir = recipe.build_dir
            new_logfile_name = Path('log.txt')
            new_logfile_postfix = ''
            if self.default_logfile:
                new_logfile_dir = self.default_logfile.parent
                new_logfile_name = self.default_logfile
                new_logfile_postfix = '_' + recipe.display_name.replace(
                    ' ', '_'
                )

            new_logfile = new_logfile_dir / (
                new_logfile_name.stem
                + new_logfile_postfix
                + new_logfile_name.suffix
            )

            new_logfile_dir.mkdir(parents=True, exist_ok=True)
            new_log_filehandler = logging.FileHandler(
                new_logfile,
                encoding='utf-8',
                # Truncate the file
                mode='w',
            )
            new_log_filehandler.setLevel(self.default_log_level)
            new_log_filehandler.setFormatter(self.dispatching_log_formatter)
            new_logger.addHandler(new_log_filehandler)

            recipe.set_logger(new_logger)
            recipe.set_logfile(new_logfile)

    def apply_root_log_formatting(self) -> None:
        """Inherit user defined formatting from the root_logger."""
        # Use the the existing root logger formatter if one exists.
        for handler in logging.getLogger().handlers:
            if handler.formatter:
                self.default_log_formatter = handler.formatter
                break

        formatter_mapping = {
            self.root_logger.name: self.default_log_formatter,
        }
        if self.use_verbatim_error_log_formatting:
            formatter_mapping[self.error_logger.name] = self.blank_log_formatter

        self.dispatching_log_formatter = DispatchingFormatter(
            formatter_mapping,
            self.default_log_formatter,
        )

    def should_use_progress_bars(self) -> bool:
        if not self.allow_progress_bars:
            return False
        if self.separate_build_file_logging or self.default_logfile:
            return True
        return False

    def use_stdout_proxy(self) -> None:
        """Setup StdoutProxy for progress bars."""

        self.stdout_proxy = StdoutProxy(raw=True)
        root_logger = logging.getLogger()
        handlers = root_logger.handlers + self.error_logger.handlers

        for handler in handlers:
            # Must use type() check here since this returns True:
            #   isinstance(logging.FileHandler, logging.StreamHandler)
            # pylint: disable=unidiomatic-typecheck
            if type(handler) == logging.StreamHandler:
                handler.setStream(self.stdout_proxy)  # type: ignore
                handler.setFormatter(self.dispatching_log_formatter)
            # pylint: enable=unidiomatic-typecheck

    def flush_log_handlers(self) -> None:
        root_logger = logging.getLogger()
        handlers = root_logger.handlers + self.error_logger.handlers
        for cfg in self:
            handlers.extend(cfg.log.handlers)
        for handler in handlers:
            handler.flush()
        if self.stdout_proxy:
            self.stdout_proxy.flush()
            self.stdout_proxy.close()

    def __len__(self) -> int:
        return len(self.build_recipes)

    def __getitem__(self, index: int) -> BuildRecipe:
        return self.build_recipes[index]

    def __iter__(self) -> Generator[BuildRecipe, None, None]:
        return (build_recipe for build_recipe in self.build_recipes)

    def run_build(
        self,
        cfg: BuildRecipe,
        env: dict,
        index_message: str | None = '',
    ) -> bool:
        """Run a single build config."""
        if BUILDER_CONTEXT.build_stopping():
            return False

        if self.colors:
            # Force colors in Pigweed subcommands run through the watcher.
            env['PW_USE_COLOR'] = '1'
            # Force Ninja to output ANSI colors
            env['CLICOLOR_FORCE'] = '1'

        build_succeeded = False
        cfg.reset_status()
        cfg.status.mark_started()
        for command_step in cfg.steps:
            command_args = command_step.get_args(
                additional_ninja_args=self.extra_ninja_args,
                additional_bazel_args=self.extra_bazel_args,
                additional_bazel_build_args=self.extra_bazel_build_args,
            )

            quoted_command_args = ' '.join(
                shlex.quote(arg) for arg in command_args
            )
            build_succeeded = True
            if command_step.should_run():
                cfg.log.info(
                    '%s %s %s',
                    index_message,
                    self.color.blue('Run ==>'),
                    quoted_command_args,
                )

                # Verify that the build output directories exist.
                if (
                    command_step.build_system_command is not None
                    and cfg.build_dir
                    and (not cfg.build_dir.is_dir())
                ):
                    self.abort_callback(
                        'Build directory does not exist: %s', cfg.build_dir
                    )

                build_succeeded = self.execute_command(
                    command_args, env, cfg, cfg.log, None
                )
            else:
                cfg.log.info(
                    '%s %s %s',
                    index_message,
                    self.color.yellow('Skipped ==>'),
                    quoted_command_args,
                )

            BUILDER_CONTEXT.mark_progress_step_complete(cfg)
            # Don't run further steps if a command fails.
            if not build_succeeded:
                break

        # If all steps were skipped the return code will not be set. Force
        # status to passed in this case.
        if build_succeeded and not cfg.status.passed():
            cfg.status.set_passed()

        cfg.status.mark_done()

        return build_succeeded

    def print_pass_fail_banner(
        self,
        cancelled: bool = False,
        logger: logging.Logger = _LOG,
    ) -> None:
        # Check conditions where banners should not be shown:
        # Banner flag disabled.
        if not self.banners:
            return
        # If restarting or interrupted.
        if BUILDER_CONTEXT.interrupted():
            if BUILDER_CONTEXT.ctrl_c_pressed:
                _LOG.info(
                    self.color.yellow('Exited due to keyboard interrupt.')
                )
            return
        # If any build is still pending.
        if any(recipe.status.pending() for recipe in self):
            return

        # Show a large color banner for the overall result.
        if all(recipe.status.passed() for recipe in self) and not cancelled:
            for line in PASS_MESSAGE.splitlines():
                logger.info(self.color.green(line))
        else:
            for line in FAIL_MESSAGE.splitlines():
                logger.info(self.color.red(line))

    def print_build_summary(
        self,
        cancelled: bool = False,
        logger: logging.Logger = _LOG,
    ) -> None:
        """Print build status summary table."""

        build_descriptions = []
        build_status = []

        for cfg in self:
            description = [str(cfg.display_name).ljust(self.max_name_width)]
            description.append(' '.join(cfg.targets()))
            build_descriptions.append('  '.join(description))

            if cfg.status.passed():
                build_status.append(self.charset.slug_ok)
            elif cfg.status.failed():
                build_status.append(self.charset.slug_fail)
            else:
                build_status.append(self.charset.slug_building)

        if not cancelled:
            logger.info(' ╔════════════════════════════════════')
            logger.info(' ║')

            for slug, cmd in zip(build_status, build_descriptions):
                logger.info(' ║   %s  %s', slug, cmd)

            logger.info(' ║')
            logger.info(" ╚════════════════════════════════════")
        else:
            # Build was interrupted.
            logger.info('')
            logger.info(' ╔════════════════════════════════════')
            logger.info(' ║')
            logger.info(' ║  %s- interrupted', self.charset.slug_fail)
            logger.info(' ║')
            logger.info(" ╚════════════════════════════════════")


def run_recipe(
    index: int, project_builder: ProjectBuilder, cfg: BuildRecipe, env
) -> bool:
    if BUILDER_CONTEXT.interrupted():
        return False
    if not cfg.enabled:
        return False

    num_builds = len(project_builder)
    index_message = f'[{index}/{num_builds}]'

    result = False

    log_build_recipe_start(index_message, project_builder, cfg)

    result = project_builder.run_build(cfg, env, index_message=index_message)

    log_build_recipe_finish(index_message, project_builder, cfg)

    return result


def run_builds(project_builder: ProjectBuilder, workers: int = 1) -> int:
    """Execute build steps in the ProjectBuilder and print a summary.

    Returns: 1 for a failed build, 0 for success."""
    num_builds = len(project_builder)
    _LOG.info('Starting build with %d directories', num_builds)
    if project_builder.default_logfile:
        _LOG.info(
            '%s %s',
            project_builder.color.blue('Root logfile:'),
            project_builder.default_logfile.resolve(),
        )

    env = os.environ.copy()

    # Print status before starting
    if not project_builder.should_use_progress_bars():
        project_builder.print_build_summary()
    project_builder.print_pass_fail_banner()

    if workers > 1 and not project_builder.separate_build_file_logging:
        _LOG.warning(
            project_builder.color.yellow(
                'Running in parallel without --separate-logfiles; All build '
                'output will be interleaved.'
            )
        )

    BUILDER_CONTEXT.set_project_builder(project_builder)
    BUILDER_CONTEXT.set_building()

    def _cleanup() -> None:
        if not project_builder.should_use_progress_bars():
            project_builder.print_build_summary()
        project_builder.print_pass_fail_banner()
        project_builder.flush_log_handlers()
        BUILDER_CONTEXT.set_idle()
        BUILDER_CONTEXT.exit_progress()

    if workers == 1:
        # TODO(tonymd): Try to remove this special case. Using
        # ThreadPoolExecutor when running in serial (workers==1) currently
        # breaks Ctrl-C handling. Build processes keep running.
        try:
            if project_builder.should_use_progress_bars():
                BUILDER_CONTEXT.add_progress_bars()
            for i, cfg in enumerate(project_builder, start=1):
                run_recipe(i, project_builder, cfg, env)
        # Ctrl-C on Unix generates KeyboardInterrupt
        # Ctrl-Z on Windows generates EOFError
        except (KeyboardInterrupt, EOFError):
            _exit_due_to_interrupt()
        finally:
            _cleanup()

    else:
        with concurrent.futures.ThreadPoolExecutor(
            max_workers=workers
        ) as executor:
            futures = []
            for i, cfg in enumerate(project_builder, start=1):
                futures.append(
                    executor.submit(run_recipe, i, project_builder, cfg, env)
                )

            try:
                if project_builder.should_use_progress_bars():
                    BUILDER_CONTEXT.add_progress_bars()
                for future in concurrent.futures.as_completed(futures):
                    future.result()
            # Ctrl-C on Unix generates KeyboardInterrupt
            # Ctrl-Z on Windows generates EOFError
            except (KeyboardInterrupt, EOFError):
                _exit_due_to_interrupt()
            finally:
                _cleanup()

    project_builder.flush_log_handlers()
    return BUILDER_CONTEXT.exit_code()


def main() -> int:
    """Build a Pigweed Project."""
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser = add_project_builder_arguments(parser)
    args = parser.parse_args()

    pw_env = pw_cli.env.pigweed_environment()
    if pw_env.PW_EMOJI:
        charset = EMOJI_CHARSET
    else:
        charset = ASCII_CHARSET

    prefs = ProjectBuilderPrefs(
        load_argparse_arguments=add_project_builder_arguments
    )
    prefs.apply_command_line_args(args)
    build_recipes = create_build_recipes(prefs)

    log_level = logging.DEBUG if args.debug_logging else logging.INFO

    pw_cli.log.install(
        level=log_level,
        use_color=args.colors,
        hide_timestamp=False,
    )

    project_builder = ProjectBuilder(
        build_recipes=build_recipes,
        jobs=args.jobs,
        banners=args.banners,
        keep_going=args.keep_going,
        colors=args.colors,
        charset=charset,
        separate_build_file_logging=args.separate_logfiles,
        root_logfile=args.logfile,
        root_logger=_LOG,
        log_level=log_level,
    )

    if project_builder.should_use_progress_bars():
        project_builder.use_stdout_proxy()

    workers = 1
    if args.parallel:
        # If parallel is requested and parallel_workers is set to 0 run all
        # recipes in parallel. That is, use the number of recipes as the worker
        # count.
        if args.parallel_workers == 0:
            workers = len(project_builder)
        else:
            workers = args.parallel_workers

    return run_builds(project_builder, workers)


if __name__ == '__main__':
    sys.exit(main())
