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

Usage examples:

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

import argparse
import concurrent.futures
import os
import logging
from pathlib import Path
import re
import shlex
import sys
import subprocess
from typing import (
    Callable,
    Dict,
    Generator,
    List,
    NoReturn,
    Optional,
    Sequence,
    NamedTuple,
)


import pw_cli.log
import pw_cli.env
from pw_build.build_recipe import BuildRecipe, create_build_recipes
from pw_build.project_builder_prefs import ProjectBuilderPrefs
from pw_build.project_builder_argparse import add_project_builder_arguments

_COLOR = pw_cli.color.colors()
_LOG = logging.getLogger('pw_build')

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


def _exit_due_to_interrupt() -> NoReturn:
    # To keep the log lines aligned with each other in the presence of
    # a '^C' from the keyboard interrupt, add a newline before the log.
    print()
    _LOG.info('Got Ctrl-C; exiting...')
    sys.exit(1)


_NINJA_BUILD_STEP = re.compile(
    r'^\[(?P<step>[0-9]+)/(?P<total_steps>[0-9]+)\] (?P<action>.*)$'
)

_NINJA_FAILURE_TEXT = '\033[31mFAILED: '


def execute_command_no_logging(
    command: List,
    env: Dict,
    recipe: BuildRecipe,
    # pylint: disable=unused-argument
    logger: logging.Logger = _LOG,
    line_processed_callback: Optional[Callable[[BuildRecipe], None]] = None,
    # pylint: enable=unused-argument
) -> bool:
    print()
    current_build = subprocess.run(command, env=env, errors='replace')
    print()
    recipe.status.return_code = current_build.returncode
    return current_build.returncode == 0


def execute_command_with_logging(
    command: List,
    env: Dict,
    recipe: BuildRecipe,
    logger: logging.Logger = _LOG,
    line_processed_callback: Optional[Callable[[BuildRecipe], None]] = None,
) -> bool:
    """Run a command in a subprocess and log all output."""
    current_stdout = ''
    returncode = None

    with subprocess.Popen(
        command,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        errors='replace',
    ) as proc:
        # Empty line at the start.
        logger.info('')

        while returncode is None:
            output = ''
            error_output = ''

            if proc.stdout:
                output += proc.stdout.readline()
                current_stdout += output
            if proc.stderr:
                error_output += proc.stderr.readline()
                current_stdout += error_output

            if not output and not error_output:
                returncode = proc.poll()
                continue

            line_match_result = _NINJA_BUILD_STEP.match(output)
            if line_match_result:
                matches = line_match_result.groupdict()
                recipe.status.current_step = line_match_result.group(0)
                recipe.status.percent = float(
                    int(matches.get('step', 0))
                    / int(matches.get('total_steps', 1))
                )

            if output.startswith(_NINJA_FAILURE_TEXT):
                logger.error(output.strip())
                recipe.status.error_count += 1

            else:
                # Mypy output mixes character encoding in its colored output
                # due to it's use of the curses module retrieving the 'sgr0'
                # (or exit_attribute_mode) capability from the host
                # machine's terminfo database.
                #
                # This can result in this sequence ending up in STDOUT as
                # b'\x1b(B\x1b[m'. (B tells terminals to interpret text as
                # USASCII encoding but will appear in prompt_toolkit as a B
                # character.
                #
                # The following replace calls will strip out those
                # instances.
                logger.info(
                    output.replace('\x1b(B\x1b[m', '')
                    .replace('\x1b[1m', '')
                    .strip()
                )

            if line_processed_callback:
                line_processed_callback(recipe)

        recipe.status.return_code = returncode
        # Empty line at the end.
        logger.info('')

    return returncode == 0


def log_build_recipe_start(
    index_message: str,
    project_builder: 'ProjectBuilder',
    cfg: BuildRecipe,
    logger: logging.Logger = _LOG,
) -> None:
    if project_builder.separate_build_file_logging:
        cfg.log.propagate = False

    build_start_msg = ' '.join(
        [index_message, _COLOR.cyan('Starting ==>'), str(cfg)]
    )

    logger.info(build_start_msg)
    if cfg.logfile:
        logger.info(
            '%s %s: %s',
            index_message,
            _COLOR.yellow('Logging to'),
            cfg.logfile.resolve(),
        )
        cfg.log.info(build_start_msg)


def log_build_recipe_finish(
    index_message: str,
    project_builder: 'ProjectBuilder',  # pylint: disable=unused-argument
    cfg: BuildRecipe,
    logger: logging.Logger = _LOG,
) -> None:

    if cfg.status.failed():
        level = logging.ERROR
        tag = _COLOR.red('(FAIL)')
    else:
        level = logging.INFO
        tag = _COLOR.green('(OK)')

    build_finish_msg = [
        level,
        '%s %s %s %s',
        index_message,
        _COLOR.cyan('Finished ==>'),
        cfg,
        tag,
    ]
    logger.log(*build_finish_msg)
    if cfg.logfile:
        cfg.log.log(*build_finish_msg)


class MissingGlobalLogfile(Exception):
    """Exception raised if a global logfile is not specifed."""


class ProjectBuilder:  # pylint: disable=too-many-instance-attributes
    """Pigweed Project Builder"""

    def __init__(
        # pylint: disable=too-many-arguments
        self,
        build_recipes: Sequence[BuildRecipe],
        jobs: Optional[int] = None,
        banners: bool = True,
        keep_going: bool = False,
        abort_callback: Callable = _exit,
        execute_command: Callable[
            [List, Dict, BuildRecipe, logging.Logger, Optional[Callable]], bool
        ] = execute_command_no_logging,
        charset: ProjectBuilderCharset = ASCII_CHARSET,
        colors: bool = True,
        separate_build_file_logging: bool = False,
        root_logger: logging.Logger = _LOG,
        root_logfile: Optional[Path] = None,
        log_level: int = logging.INFO,
    ):

        self.charset: ProjectBuilderCharset = charset
        self.abort_callback = abort_callback
        self.execute_command = execute_command
        self.banners = banners
        self.build_recipes = build_recipes
        self.max_name_width = max(
            [len(str(step.display_name)) for step in self.build_recipes]
        )

        self.extra_ninja_args: List[str] = []
        self.extra_bazel_args: List[str] = []
        self.extra_bazel_build_args: List[str] = []

        if jobs:
            job_args = ['-j', f'{jobs}']
            self.extra_ninja_args.extend(job_args)
            self.extra_bazel_build_args.extend(job_args)
        if keep_going:
            self.extra_ninja_args.extend(['-k', '0'])
            self.extra_bazel_build_args.extend(['-k'])

        self.colors = colors
        if colors:
            self.extra_bazel_args.append('--color=yes')
        else:
            self.extra_bazel_args.append('--color=no')

        if separate_build_file_logging and not root_logfile:
            raise MissingGlobalLogfile(
                '\n\nA logfile must be specified if using separate logs per '
                'build directory.'
            )

        self.separate_build_file_logging = separate_build_file_logging
        self.default_log_level = log_level
        self.default_logfile = root_logfile

        if root_logfile:
            timestamp_fmt = _COLOR.black_on_white('%(asctime)s') + ' '
            formatter = logging.Formatter(
                timestamp_fmt + '%(levelname)s %(message)s', '%Y%m%d %H:%M:%S'
            )

            self.execute_command = execute_command_with_logging

            build_log_filehandler = logging.FileHandler(
                root_logfile, encoding='utf-8'
            )
            build_log_filehandler.setLevel(log_level)
            build_log_filehandler.setFormatter(formatter)
            root_logger.addHandler(build_log_filehandler)

            if not separate_build_file_logging:
                for recipe in self.build_recipes:
                    recipe.set_logger(root_logger)
            else:
                for recipe in self.build_recipes:
                    new_logger = logging.getLogger(
                        f'{root_logger.name}.{recipe.display_name}'
                    )
                    new_logger.setLevel(log_level)
                    new_logger.propagate = False
                    new_logfile = root_logfile.parent / (
                        root_logfile.stem
                        + '_'
                        + recipe.display_name.replace(' ', '_')
                        + root_logfile.suffix
                    )

                    new_log_filehandler = logging.FileHandler(
                        new_logfile, encoding='utf-8'
                    )
                    new_log_filehandler.setLevel(log_level)
                    new_log_filehandler.setFormatter(formatter)
                    new_logger.addHandler(new_log_filehandler)

                    recipe.set_logger(new_logger)
                    recipe.set_logfile(new_logfile)

    def __len__(self) -> int:
        return len(self.build_recipes)

    def __getitem__(self, index: int) -> BuildRecipe:
        return self.build_recipes[index]

    def __iter__(self) -> Generator[BuildRecipe, None, None]:
        return (build_recipe for build_recipe in self.build_recipes)

    def run_build(
        self,
        cfg: BuildRecipe,
        env: Dict,
        index_message: Optional[str] = '',
    ) -> bool:
        """Run a single build config."""
        if self.colors:
            # Force colors in Pigweed subcommands run through the watcher.
            env['PW_USE_COLOR'] = '1'
            # Force Ninja to output ANSI colors
            env['CLICOLOR_FORCE'] = '1'

        build_succeded = False
        cfg.reset_status()
        for command_step in cfg.steps:
            command_args = command_step.get_args(
                additional_ninja_args=self.extra_ninja_args,
                additional_bazel_args=self.extra_bazel_args,
                additional_bazel_build_args=self.extra_bazel_build_args,
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

            quoted_command_args = ' '.join(
                shlex.quote(arg) for arg in command_args
            )
            build_succeded = True
            if command_step.should_run():
                cfg.log.info(
                    '%s %s %s',
                    index_message,
                    _COLOR.blue('Run ==>'),
                    quoted_command_args,
                )
                build_succeded = self.execute_command(
                    command_args, env, cfg, cfg.log, None
                )
            else:
                cfg.log.info(
                    '%s %s %s',
                    index_message,
                    _COLOR.yellow('Skipped ==>'),
                    quoted_command_args,
                )
            # Don't run further steps if a command fails.
            if not build_succeded:
                break

        return build_succeded

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

            for (slug, cmd) in zip(build_status, build_descriptions):
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

        # Show a large color banner for the overall result.
        if self.banners and not any(recipe.status.pending() for recipe in self):
            if all(recipe.status.passed() for recipe in self) and not cancelled:
                for line in PASS_MESSAGE.splitlines():
                    logger.info(_COLOR.green(line))
            else:
                for line in FAIL_MESSAGE.splitlines():
                    logger.info(_COLOR.red(line))


def run_recipe(
    index: int, project_builder: ProjectBuilder, cfg: BuildRecipe, env
) -> None:
    num_builds = len(project_builder)
    index_message = f'[{index}/{num_builds}]'

    log_build_recipe_start(index_message, project_builder, cfg)

    try:
        project_builder.run_build(cfg, env, index_message=index_message)
    # Ctrl-C on Unix generates KeyboardInterrupt
    # Ctrl-Z on Windows generates EOFError
    except (KeyboardInterrupt, EOFError):
        _exit_due_to_interrupt()

    log_build_recipe_finish(index_message, project_builder, cfg)


def run_builds(project_builder: ProjectBuilder, workers: int = 1) -> None:
    """Execute build steps in the ProjectBuilder and print a summary."""
    num_builds = len(project_builder)
    _LOG.info('Starting build with %d directories', num_builds)
    if project_builder.default_logfile:
        _LOG.info(
            '%s: %s',
            _COLOR.yellow('Logging to'),
            project_builder.default_logfile.resolve(),
        )

    env = os.environ.copy()

    # Print status before starting
    project_builder.print_build_summary()

    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as executor:
        future_to_index = {}
        for i, cfg in enumerate(project_builder, 1):
            future_to_index[
                executor.submit(run_recipe, i, project_builder, cfg, env)
            ] = i

        try:
            for future in concurrent.futures.as_completed(future_to_index):
                future.result()
        # Ctrl-C on Unix generates KeyboardInterrupt
        # Ctrl-Z on Windows generates EOFError
        except (KeyboardInterrupt, EOFError):
            _exit_due_to_interrupt()

    # Print status when finished
    project_builder.print_build_summary()


def main() -> None:
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

    run_builds(project_builder)


if __name__ == '__main__':
    main()
