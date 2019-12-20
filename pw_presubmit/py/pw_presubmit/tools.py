# Copyright 2019 The Pigweed Authors
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
"""Tools for running presubmit checks in a Git repository.

Presubmit checks are defined as a function or other callable. The function may
take either no arguments or a list of the paths on which to run. Presubmit
checks communicate failure by raising any exception.

For example, either of these functions may be used as presubmit checks:

  @pw_presubmit.filter_paths(endswith='.py')
  def file_contains_ni(ctx: PresubmitContext):
      for path in ctx.paths:
          with open(path) as file:
              contents = file.read()
              if 'ni' not in contents and 'nee' not in contents:
                  raise PresumitFailure('Files must say "ni"!', path=path)

  def run_the_build():
      subprocess.run(['make', 'release'], check=True)

Presubmit checks are provided to the parse_args_and_run_presubmit or
run_presubmit function as a list. For example,

  PRESUBMIT_CHECKS = [file_contains_ni, run_the_build]
  sys.exit(0 if parse_args_and_run_presubmit(PRESUBMIT_CHECKS) else 1)

Presubmit checks that accept a list of paths may use the filter_paths decorator
to automatically filter the paths list for file types they care about. See the
pragma_once function for an example.
"""

import argparse
from collections import Counter, defaultdict
import contextlib
import dataclasses
import enum
import logging
import re
import os
import pathlib
import shlex
import subprocess
import sys
import time
from typing import Callable, Dict, Iterable, List, NamedTuple, Optional
from typing import Sequence, Tuple
from inspect import signature

_LOG: logging.Logger = logging.getLogger(__name__)
_LOG.setLevel(logging.DEBUG)


def plural(items_or_count, singular: str, count_format='') -> str:
    """Returns the singular or plural form of a word based on a count."""

    try:
        count = len(items_or_count)
    except TypeError:
        count = items_or_count

    num = f'{count:{count_format}}'

    if singular.endswith('y'):
        return f'{num} {singular[:-1]}{"y" if count == 1 else "ies"}'
    if singular.endswith('s'):
        return f'{num} {singular}{"" if count == 1 else "es"}'
    return f'{num} {singular}{"" if count == 1 else "s"}'


def git_stdout(*args: str, repo: str = '.') -> str:
    return subprocess.run(('git', '-C', repo, *args),
                          stdout=subprocess.PIPE,
                          check=True).stdout.decode().strip()


def _git_ls_files(*args: str, repo: str = '.') -> Sequence[str]:
    return [
        os.path.abspath(os.path.join(repo, path))
        for path in git_stdout('ls-files', '--', *args, repo=repo).split()
    ]


def git_diff_names(commit: str = 'HEAD',
                   paths: Sequence[str] = (),
                   repo: str = '.') -> Sequence[str]:
    """Returns absolute paths of files changed since the specified commit."""
    root = git_repo_path(repo=repo)
    return [
        os.path.abspath(os.path.join(root, path)) for path in git_stdout(
            'diff', '--name-only', commit, '--', *paths, repo=repo).split()
    ]


def list_git_files(
        commit: Optional[str] = None,
        paths: Sequence[str] = (),
        exclude: Sequence = (),
        repo: str = '.',
) -> Sequence[str]:
    """Lists files with git ls-files or git diff --name-only.

    This function may only be called if repo is or is in a Git repository.
    """

    if commit:
        files = git_diff_names(commit, paths, repo=repo)
    else:
        files = _git_ls_files(*paths, repo=repo)
    return sorted(
        set(path for path in files
            if not any(exp.search(path) for exp in exclude)))


def is_git_repo(path='.') -> bool:
    return not subprocess.run(['git', '-C', path, 'rev-parse'],
                              stderr=subprocess.DEVNULL).returncode


def git_repo_path(*paths, repo: str = '.') -> pathlib.Path:
    """Returns a path relative to a Git repository's root."""
    return pathlib.Path(git_stdout('rev-parse', '--show-toplevel',
                                   repo=repo)).joinpath(*paths)


def _make_color(*codes: int):
    start = ''.join(f'\033[{code}m' for code in codes)
    return f'{start}{{}}\033[0m'.format if os.name == 'posix' else str


color_red = _make_color(31)
color_bold_red = _make_color(31, 1)
color_black_on_red = _make_color(30, 41)
color_yellow = _make_color(33, 1)
color_green = _make_color(32)
color_black_on_green = _make_color(30, 42)
color_aqua = _make_color(36)
color_bold_white = _make_color(37, 1)


def _make_box(section_alignments: Sequence[str]) -> str:
    indices = [i + 1 for i in range(len(section_alignments))]
    top_sections = '{2}'.join('{1:{1}^{width%d}}' % i for i in indices)
    mid_sections = '{5}'.join('{section%d:%s{width%d}}' %
                              (i, section_alignments[i - 1], i)
                              for i in indices)
    bot_sections = '{9}'.join('{8:{8}^{width%d}}' % i for i in indices)

    return ''.join(['{0}', *top_sections, '{3}\n',
                    '{4}', *mid_sections, '{6}\n',
                    '{7}', *bot_sections, '{10}'])  # yapf: disable


_DOUBLE = '╔═╦╗║║║╚═╩╝'
_TOP = '┏━┯┓┃│┃┃ │┃'
_BOTTOM = '┃ │┃┃│┃┗━┷┛'

WIDTH = 80

_LEFT = 8
_RIGHT = 11


def _title(msg, style=_DOUBLE):
    msg = f' {msg} '.center(WIDTH - 2)
    return _make_box('^').format(*style, section1=msg, width1=len(msg))


def _format_time(time_s: float) -> str:
    minutes, seconds = divmod(time_s, 60)
    return f' {int(minutes)}:{seconds:04.1f}'


def _box(style, left, middle, right, box=_make_box('><>')):
    return box.format(*style,
                      section1=left + ('' if left.endswith(' ') else ' '),
                      width1=_LEFT,
                      section2=' ' + middle,
                      width2=WIDTH - _LEFT - _RIGHT - 4,
                      section3=right + ' ',
                      width3=_RIGHT)


def file_summary(paths: Iterable) -> str:
    files = Counter(
        os.path.splitext(path)[1] or os.path.basename(path) for path in paths)

    if not files:
        return ''

    width = max(len(f) for f in files) + 2
    max_count_width = len(str(max(files.values())))

    return '\n'.join(
        f'{ext:>{width}}: {plural(count, "file", max_count_width)}'
        for ext, count in sorted(files.items()))


class PresubmitFailure(Exception):
    """Optional exception to use for presubmit failures."""
    def __init__(self, description: str = '', path: Optional[str] = None):
        super().__init__(f'{path}: {description}' if path else description)


class _Result(enum.Enum):

    PASS = 'PASSED'  # Check completed successfully.
    FAIL = 'FAILED'  # Check failed.
    CANCEL = 'CANCEL'  # Check didn't complete.

    def colorized(self, width: int, invert=False) -> Callable:
        if self is _Result.PASS:
            color = color_black_on_green if invert else color_green
        elif self is _Result.FAIL:
            color = color_black_on_red if invert else color_red
        elif self is _Result.CANCEL:
            color = color_yellow
        else:
            color = lambda value: value

        padding = (width - len(self.value)) // 2 * ' '
        return padding + color(self.value) + padding


@dataclasses.dataclass(frozen=True)
class PresubmitContext:
    """Context passed into presubmit checks."""
    repository_root: pathlib.Path
    output_directory: pathlib.Path
    paths: List[str]


class Presubmit:
    """Runs a series of presubmit checks on a list of files."""
    def __init__(self, repository_root: pathlib.Path,
                 output_directory: pathlib.Path, paths: List[str]):
        self._repository_root = repository_root
        self._output_directory = output_directory
        self._paths = paths

    def run(self, full_program: Sequence, keep_going: bool = False) -> bool:
        """Executes a series of presubmit checks on the paths."""

        program = _apply_filters(full_program, self._paths)

        print(_title(f'Presubmit checks for {self._repository_root.name}'))
        _LOG.info('Running %d of %s on %s in %s', len(program),
                  plural(full_program, 'check'), plural(self._paths, 'file'),
                  self._repository_root)

        _LOG.debug('Paths:\n%s', '\n'.join(self._paths))
        print(
            file_summary(self._paths)
            or color_yellow('No files are being checked!'))

        _LOG.debug('Checks:\n%s', '\n'.join(c.name for c, _ in program))

        start_time: float = time.time()
        passed, failed, skipped = self._execute_checks(program, keep_going)
        _log_summary(time.time() - start_time, passed, failed, skipped)

        return not failed and not skipped

    @contextlib.contextmanager
    def _context(self, name, paths):
        # There are many characters banned from filenames on Windows. To
        # simplify things, just strip everything that's not a letter, digit,
        # or underscore.
        sanitized_name = re.sub(r'[\W_]+', '_', name).lower()
        output_directory = self._output_directory.joinpath(sanitized_name)
        os.makedirs(output_directory, exist_ok=True)

        handler = logging.FileHandler(output_directory.joinpath(f'step.log'),
                                      mode='w')
        handler.setLevel(logging.DEBUG)

        try:
            _LOG.addHandler(handler)

            yield PresubmitContext(
                repository_root=self._repository_root.absolute(),
                output_directory=output_directory.absolute(),
                paths=paths,
            )

        finally:
            _LOG.removeHandler(handler)

    def _execute_checks(self, program,
                        keep_going: bool) -> Tuple[int, int, int]:
        """Runs presubmit checks; returns (passed, failed, skipped) lists."""
        passed = failed = 0

        for i, (check, paths) in enumerate(program, 1):
            paths = [self._repository_root.joinpath(p) for p in paths]
            with self._context(check.name, paths) as ctx:
                result = check.run(ctx, i, len(program))

            if result is _Result.PASS:
                passed += 1
            elif result is _Result.CANCEL:
                break
            else:
                failed += 1
                if not keep_going:
                    break

        return passed, failed, len(program) - passed - failed


def _apply_filters(program, paths) -> List[Tuple['_Check', List]]:
    """Returns a list of (check, paths_to_check) for checks that should run."""
    program = [c if isinstance(c, _Check) else _Check(c) for c in program]
    filter_to_paths: Dict[_PathFilter, List[str]] = defaultdict(list)

    for check in program:
        filter_to_paths[check.filter].append(check)

    check_to_paths = _map_checks_to_paths(filter_to_paths, paths)
    return [(c, check_to_paths[c]) for c in program if c in check_to_paths]


def _map_checks_to_paths(filter_to_paths, paths):
    checks_to_paths = {}

    for filt, checks in filter_to_paths.items():
        exclude = [re.compile(exp) for exp in filt.exclude]

        filtered_paths = tuple(
            path for path in paths
            if any(path.endswith(end) for end in filt.endswith) and not any(
                exp.fullmatch(path) for exp in exclude))

        for check in checks:
            if filtered_paths or check.always_run:
                checks_to_paths[check] = filtered_paths
            else:
                _LOG.debug('Skipping "%s": no relevant files', check.name)

    return checks_to_paths


def _log_summary(time_s: float, passed: int, failed: int,
                 skipped: int) -> None:
    summary = []
    if passed:
        summary.append(f'{passed} passed')
    if failed:
        summary.append(f'{failed} failed')
    if skipped:
        summary.append(f'{skipped} did not run')
    summary_str = ', '.join(summary) or 'nothing was done'

    if failed or skipped:
        result_text = _Result.FAIL.colorized(_LEFT, invert=True)
    else:
        result_text = _Result.PASS.colorized(_LEFT, invert=True)

    print(
        _box(_DOUBLE, result_text,
             f'{passed + failed + skipped} checks: {summary_str}',
             _format_time(time_s)))


def add_path_arguments(parser) -> None:
    """Adds common presubmit check options to an argument parser."""

    parser.add_argument(
        'paths',
        nargs='*',
        help=(
            'Paths to which to restrict the presubmit checks. '
            'Directories are expanded with git ls-files. '
            'If --base is provided, all paths are interpreted as Git paths.'))
    parser.add_argument(
        '-b',
        '--base',
        metavar='COMMIT',
        help=('Git revision against which to diff for changed files. '
              'If none is provided, the entire repository is used.'))
    parser.add_argument(
        '-e',
        '--exclude',
        metavar='REGULAR_EXPRESSION',
        default=[],
        action='append',
        type=re.compile,
        help='Exclude paths matching any of these regular expressions.')


def add_arguments(parser) -> None:
    """Adds common presubmit check options to an argument parser."""

    add_path_arguments(parser)
    parser.add_argument(
        '-r',
        '--repository',
        default='.',
        help=(
            'Change to this directory before resolving paths or running the '
            'presubmit. Presubmit checks must be run from a Git repository.'))
    parser.add_argument('-k',
                        '--keep-going',
                        action='store_true',
                        help='Continue instead of aborting when errors occur.')


def run_presubmit(program: Sequence[Callable],
                  base: Optional[str] = None,
                  paths: Sequence[str] = (),
                  exclude: Sequence = (),
                  repository=None,
                  output_directory=None,
                  keep_going: bool = False) -> bool:
    """Lists files in the current Git repo and runs a Presubmit with them.

    This changes the directory to the root of the Git repository after listing
    paths, so all presubmit checks can assume they run from there.

    Args:
        program: list of presubmit check functions to run
        base: optional base Git commit to list files against
        paths: optional list of paths to run the presubmit checks against
        exclude: regular expressions of paths to exclude from checks
        repository: git repository to check
        output_directory: where to place output files
        keep_going: whether to continue running checks if an error occurs

    Returns:
        True if all presubmit checks succeeded
    """

    if not is_git_repo(repository):
        _LOG.critical('Presubmit checks must be run from a Git repo')
        return False

    files = list_git_files(base, paths, exclude, repository)
    root = git_repo_path(repo=repository)

    if not root.samefile(repository):
        _LOG.info('Checking files in the %s subdirectory of the %s repository',
                  pathlib.Path.cwd().relative_to(root), root)

    files = [os.path.relpath(path, root) for path in files]

    if not output_directory:
        output_directory = root.joinpath('.presubmit')

    presubmit = Presubmit(
        repository_root=root,
        output_directory=output_directory,
        paths=files,
    )
    return presubmit.run(program, keep_going)


def parse_args_and_run_presubmit(
        program: Sequence[Callable],
        arg_parser: Optional[argparse.ArgumentParser] = None) -> bool:
    """Parses the command line arguments and calls run_presubmit with them."""

    if arg_parser is None:
        arg_parser = argparse.ArgumentParser(
            description='Runs presubmit checks on a Git repository.',
            formatter_class=argparse.RawDescriptionHelpFormatter)

    add_arguments(arg_parser)
    return run_presubmit(program, **vars(arg_parser.parse_args()))


def find_python_packages(python_paths, repo='.') -> Dict[str, List[str]]:
    """Returns Python package directories for the files in python_paths."""
    setup_pys = [
        os.path.dirname(file)
        for file in _git_ls_files('setup.py', '*/setup.py', repo=repo)
    ]

    package_dirs: Dict[str, List[str]] = defaultdict(list)

    for path in (os.path.abspath(p) for p in python_paths):
        try:
            setup_dir = max(setup for setup in setup_pys
                            if path.startswith(setup))
            package_dirs[os.path.abspath(setup_dir)].append(path)
        except ValueError:
            continue

    return package_dirs


class _PathFilter(NamedTuple):
    endswith: Tuple[str, ...] = ('', )
    exclude: Tuple[str, ...] = ()


class _Check:
    """Wraps a presubmit check function.

    This class consolidates the logic for running and logging a presubmit check.
    It also supports filtering the paths passed to the presubmit check.
    """
    def __init__(self,
                 check_function: Callable,
                 path_filter: _PathFilter = _PathFilter(),
                 always_run: bool = True):
        self._check: Callable = check_function
        self.filter: _PathFilter = path_filter
        self.always_run: bool = always_run

        # Since _Check wraps a presubmit function, adopt that function's name.
        self.__name__ = self._check.__name__

    @property
    def name(self):
        return self.__name__

    def run(self, ctx: PresubmitContext, count: int, total: int) -> _Result:
        """Runs the presubmit check on the provided paths."""

        print('\n'.join(
            _box(_TOP, f'{count}/{total}', self.name,
                 plural(ctx.paths, "file")).splitlines()[:-1]))

        _LOG.debug('[%d/%d] Running %s on %s', count, total, self.name,
                   plural(ctx.paths, "file"))

        start_time_s = time.time()
        result = self._call_function(ctx)
        time_str = _format_time(time.time() - start_time_s)
        _LOG.debug('%s %s', self.name, result.value)

        print(_box(_BOTTOM, result.colorized(_LEFT), self.name, time_str))
        _LOG.debug('%s duration:%s', self.name, time_str)

        return result

    def _call_function(self, ctx: PresubmitContext) -> _Result:
        try:
            self._check(ctx)
        except PresubmitFailure as failure:
            if str(failure):
                _LOG.warning('%s', failure)
            return _Result.FAIL
        except Exception as failure:  # pylint: disable=broad-except
            _LOG.exception('Presubmit check %s failed!', self.name)
            return _Result.FAIL
        except KeyboardInterrupt:
            print()
            return _Result.CANCEL

        return _Result.PASS


def _make_tuple(value: Iterable[str]) -> Tuple[str, ...]:
    return tuple([value] if isinstance(value, str) else value)


def filter_paths(endswith: Iterable[str] = (''),
                 exclude: Iterable[str] = (),
                 always_run: bool = False):
    """Decorator for filtering the paths list for a presubmit check function.

    Args:
        endswith: str or iterable of path endings to include
        exclude: regular expressions of paths to exclude

    Returns:
        a wrapped version of the presubmit function
    """
    def filter_paths_for_function(function: Callable):
        if len(signature(function).parameters) != 1:
            raise TypeError('Functions wrapped with @filter_paths must take '
                            f'exactly one argument: {function.__name__} takes '
                            f'{len(signature(function).parameters)}.')

        return _Check(function,
                      _PathFilter(_make_tuple(endswith), _make_tuple(exclude)),
                      always_run=always_run)

    return filter_paths_for_function


def log_run(*args, **kwargs) -> subprocess.CompletedProcess:
    """Logs a command then runs it with subprocess.run."""
    _LOG.debug('[COMMAND] %s\n%s',
               ', '.join(f'{k}={v}' for k, v in sorted(kwargs.items())),
               ' '.join(shlex.quote(str(arg)) for arg in args))
    return subprocess.run(args, **kwargs)


def call(*args, **kwargs) -> None:
    """Optional subprocess wrapper that causes a PresubmitFailure on errors."""
    _LOG.debug('call: %s %s', args, kwargs)
    process = subprocess.run(args,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT,
                             **kwargs)
    logfunc = _LOG.warning if process.returncode else _LOG.debug

    logfunc('[COMMAND] %s\n%s',
            ', '.join(f'{k}={v}' for k, v in sorted(kwargs.items())),
            ' '.join(shlex.quote(str(arg)) for arg in args))

    logfunc('[RESULT] %s with return code %d',
            'Failed' if process.returncode else 'Passed', process.returncode)

    output = process.stdout.decode(errors='backslashreplace')
    if output:
        logfunc('[OUTPUT]\n%s', output)

    if process.returncode:
        raise PresubmitFailure


@filter_paths(endswith='.h')
def pragma_once(ctx: PresubmitContext) -> None:
    """Presubmit check that ensures all header files contain '#pragma once'."""

    for path in ctx.paths:
        _LOG.debug('Checking %s', path)
        with open(path) as file:
            for line in file:
                if line.startswith('#pragma once'):
                    break
            else:
                raise PresubmitFailure('#pragma once is missing!', path=path)


if __name__ == '__main__':
    # As an example, run a presubmit with the pragma_once check.
    sys.exit(0 if parse_args_and_run_presubmit([pragma_once]) else 1)
