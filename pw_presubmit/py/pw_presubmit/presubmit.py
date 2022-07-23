# Copyright 2020 The Pigweed Authors
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

Presubmit checks that accept a list of paths may use the filter_paths decorator
to automatically filter the paths list for file types they care about. See the
pragma_once function for an example.

See pigweed_presbumit.py for an example of how to define presubmit checks.
"""

from __future__ import annotations

import collections
import contextlib
import dataclasses
import enum
from inspect import Parameter, signature
import itertools
import logging
import os
from pathlib import Path
import re
import subprocess
import time
from typing import (Callable, Collection, Dict, Iterable, Iterator, List,
                    NamedTuple, Optional, Pattern, Sequence, Set, Tuple, Union)

import pw_cli.env
from pw_presubmit import git_repo, tools
from pw_presubmit.tools import plural

_LOG: logging.Logger = logging.getLogger(__name__)

color_red = tools.make_color(31)
color_bold_red = tools.make_color(31, 1)
color_black_on_red = tools.make_color(30, 41)
color_yellow = tools.make_color(33, 1)
color_green = tools.make_color(32)
color_black_on_green = tools.make_color(30, 42)
color_aqua = tools.make_color(36)
color_bold_white = tools.make_color(37, 1)

_SUMMARY_BOX = '══╦╗ ║║══╩╝'
_CHECK_UPPER = '━━━┓       '
_CHECK_LOWER = '       ━━━┛'

WIDTH = 80

_LEFT = 7
_RIGHT = 11


def _title(msg, style=_SUMMARY_BOX) -> str:
    msg = f' {msg} '.center(WIDTH - 2)
    return tools.make_box('^').format(*style, section1=msg, width1=len(msg))


def _format_time(time_s: float) -> str:
    minutes, seconds = divmod(time_s, 60)
    if minutes < 60:
        return f' {int(minutes)}:{seconds:04.1f}'
    hours, minutes = divmod(minutes, 60)
    return f'{int(hours):d}:{int(minutes):02}:{int(seconds):02}'


def _box(style, left, middle, right, box=tools.make_box('><>')) -> str:
    return box.format(*style,
                      section1=left + ('' if left.endswith(' ') else ' '),
                      width1=_LEFT,
                      section2=' ' + middle,
                      width2=WIDTH - _LEFT - _RIGHT - 4,
                      section3=right + ' ',
                      width3=_RIGHT)


class PresubmitFailure(Exception):
    """Optional exception to use for presubmit failures."""
    def __init__(self, description: str = '', path=None):
        super().__init__(f'{path}: {description}' if path else description)


class _Result(enum.Enum):

    PASS = 'PASSED'  # Check completed successfully.
    FAIL = 'FAILED'  # Check failed.
    CANCEL = 'CANCEL'  # Check didn't complete.

    def colorized(self, width: int, invert: bool = False) -> str:
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


class Program(collections.abc.Sequence):
    """A sequence of presubmit checks; basically a tuple with a name."""
    def __init__(self, name: str, steps: Iterable[Callable]):
        self.name = name
        self._steps = tuple({s: None for s in tools.flatten(steps)})

    def __getitem__(self, i):
        return self._steps[i]

    def __len__(self):
        return len(self._steps)

    def __str__(self):
        return self.name

    def title(self):
        return f'{self.name if self.name else ""} presubmit checks'.strip()


class Programs(collections.abc.Mapping):
    """A mapping of presubmit check programs.

    Use is optional. Helpful when managing multiple presubmit check programs.
    """
    def __init__(self, **programs: Sequence):
        """Initializes a name: program mapping from the provided keyword args.

        A program is a sequence of presubmit check functions. The sequence may
        contain nested sequences, which are flattened.
        """
        self._programs: Dict[str, Program] = {
            name: Program(name, checks)
            for name, checks in programs.items()
        }

    def all_steps(self) -> Dict[str, Callable]:
        return {c.__name__: c for c in itertools.chain(*self.values())}

    def __getitem__(self, item: str) -> Program:
        return self._programs[item]

    def __iter__(self) -> Iterator[str]:
        return iter(self._programs)

    def __len__(self) -> int:
        return len(self._programs)


@dataclasses.dataclass(frozen=True)
class PresubmitContext:
    """Context passed into presubmit checks."""
    root: Path
    repos: Tuple[Path, ...]
    output_dir: Path
    paths: Tuple[Path, ...]
    package_root: Path

    def relative_paths(self, start: Optional[Path] = None) -> Tuple[Path, ...]:
        return tuple(
            tools.relative_paths(self.paths, start if start else self.root))

    def paths_by_repo(self) -> Dict[Path, List[Path]]:
        repos = collections.defaultdict(list)

        for path in self.paths:
            repos[git_repo.root(path)].append(path)

        return repos


class _Filter(NamedTuple):
    endswith: Tuple[str, ...] = ('', )
    exclude: Tuple[Pattern[str], ...] = ()

    def matches(self, path: str) -> bool:
        return (any(path.endswith(end) for end in self.endswith)
                and not any(exp.search(path) for exp in self.exclude))


def _print_ui(*args) -> None:
    """Prints to stdout and flushes to stay in sync with logs on stderr."""
    print(*args, flush=True)


class Presubmit:
    """Runs a series of presubmit checks on a list of files."""
    def __init__(self, root: Path, repos: Sequence[Path],
                 output_directory: Path, paths: Sequence[Path],
                 package_root: Path):
        self._root = root.resolve()
        self._repos = tuple(repos)
        self._output_directory = output_directory.resolve()
        self._paths = tuple(paths)
        self._relative_paths = tuple(
            tools.relative_paths(self._paths, self._root))
        self._package_root = package_root.resolve()

    def run(self, program: Program, keep_going: bool = False) -> bool:
        """Executes a series of presubmit checks on the paths."""

        checks = self.apply_filters(program)

        _LOG.debug('Running %s for %s', program.title(), self._root.name)
        _print_ui(_title(f'{self._root.name}: {program.title()}'))

        _LOG.info('%d of %d checks apply to %s in %s', len(checks),
                  len(program), plural(self._paths, 'file'), self._root)

        _print_ui()
        for line in tools.file_summary(self._relative_paths):
            _print_ui(line)
        _print_ui()

        if not self._paths:
            _print_ui(color_yellow('No files are being checked!'))

        _LOG.debug('Checks:\n%s', '\n'.join(c.name for c, _ in checks))

        start_time: float = time.time()
        passed, failed, skipped = self._execute_checks(checks, keep_going)
        self._log_summary(time.time() - start_time, passed, failed, skipped)

        return not failed and not skipped

    def apply_filters(
            self,
            program: Sequence[Callable]) -> List[Tuple[Check, Sequence[Path]]]:
        """Returns list of (check, paths) for checks that should run."""
        checks = [c if isinstance(c, Check) else Check(c) for c in program]
        filter_to_checks: Dict[_Filter,
                               List[Check]] = collections.defaultdict(list)

        for check in checks:
            filter_to_checks[check.filter].append(check)

        check_to_paths = self._map_checks_to_paths(filter_to_checks)
        return [(c, check_to_paths[c]) for c in checks if c in check_to_paths]

    def _map_checks_to_paths(
        self, filter_to_checks: Dict[_Filter, List[Check]]
    ) -> Dict[Check, Sequence[Path]]:
        checks_to_paths: Dict[Check, Sequence[Path]] = {}

        posix_paths = tuple(p.as_posix() for p in self._relative_paths)

        for filt, checks in filter_to_checks.items():
            filtered_paths = tuple(
                path for path, filter_path in zip(self._paths, posix_paths)
                if filt.matches(filter_path))

            for check in checks:
                if filtered_paths or check.always_run:
                    checks_to_paths[check] = filtered_paths
                else:
                    _LOG.debug('Skipping "%s": no relevant files', check.name)

        return checks_to_paths

    def _log_summary(self, time_s: float, passed: int, failed: int,
                     skipped: int) -> None:
        summary_items = []
        if passed:
            summary_items.append(f'{passed} passed')
        if failed:
            summary_items.append(f'{failed} failed')
        if skipped:
            summary_items.append(f'{skipped} not run')
        summary = ', '.join(summary_items) or 'nothing was done'

        result = _Result.FAIL if failed or skipped else _Result.PASS
        total = passed + failed + skipped

        _LOG.debug('Finished running %d checks on %s in %.1f s', total,
                   plural(self._paths, 'file'), time_s)
        _LOG.debug('Presubmit checks %s: %s', result.value, summary)

        _print_ui(
            _box(
                _SUMMARY_BOX, result.colorized(_LEFT, invert=True),
                f'{total} checks on {plural(self._paths, "file")}: {summary}',
                _format_time(time_s)))

    @contextlib.contextmanager
    def _context(self, name: str, paths: Tuple[Path, ...]):
        # There are many characters banned from filenames on Windows. To
        # simplify things, just strip everything that's not a letter, digit,
        # or underscore.
        sanitized_name = re.sub(r'[\W_]+', '_', name).lower()
        output_directory = self._output_directory.joinpath(sanitized_name)
        os.makedirs(output_directory, exist_ok=True)

        handler = logging.FileHandler(output_directory.joinpath('step.log'),
                                      mode='w')
        handler.setLevel(logging.DEBUG)

        try:
            _LOG.addHandler(handler)

            yield PresubmitContext(
                root=self._root,
                repos=self._repos,
                output_dir=output_directory,
                paths=paths,
                package_root=self._package_root,
            )

        finally:
            _LOG.removeHandler(handler)

    def _execute_checks(self, program,
                        keep_going: bool) -> Tuple[int, int, int]:
        """Runs presubmit checks; returns (passed, failed, skipped) lists."""
        passed = failed = 0

        for i, (check, paths) in enumerate(program, 1):
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


def _process_pathspecs(repos: Iterable[Path],
                       pathspecs: Iterable[str]) -> Dict[Path, List[str]]:
    pathspecs_by_repo: Dict[Path, List[str]] = {repo: [] for repo in repos}
    repos_with_paths: Set[Path] = set()

    for pathspec in pathspecs:
        # If the pathspec is a path to an existing file, only use it for the
        # repo it is in.
        if os.path.exists(pathspec):
            # Raise an exception if the path exists but is not in a known repo.
            repo = git_repo.within_repo(pathspec)
            if repo not in pathspecs_by_repo:
                raise ValueError(
                    f'{pathspec} is not in a Git repository in this presubmit')

            # Make the path relative to the repo's root.
            pathspecs_by_repo[repo].append(os.path.relpath(pathspec, repo))
            repos_with_paths.add(repo)
        else:
            # Pathspecs that are not paths (e.g. '*.h') are used for all repos.
            for patterns in pathspecs_by_repo.values():
                patterns.append(pathspec)

    # If any paths were specified, only search for paths in those repos.
    if repos_with_paths:
        for repo in set(pathspecs_by_repo) - repos_with_paths:
            del pathspecs_by_repo[repo]

    return pathspecs_by_repo


def run(program: Sequence[Callable],
        root: Path,
        repos: Collection[Path] = (),
        base: Optional[str] = None,
        paths: Sequence[str] = (),
        exclude: Sequence[Pattern] = (),
        output_directory: Optional[Path] = None,
        package_root: Path = None,
        only_list_steps: bool = False,
        keep_going: bool = False) -> bool:
    """Lists files in the current Git repo and runs a Presubmit with them.

    This changes the directory to the root of the Git repository after listing
    paths, so all presubmit checks can assume they run from there.

    The paths argument contains Git pathspecs. If no pathspecs are provided, all
    paths in all repos are included. If paths to files or directories are
    provided, only files within those repositories are searched. Patterns are
    searched across all repositories. For example, if the pathspecs "my_module/"
    and "*.h", paths under "my_module/" in the containing repo and paths in all
    repos matching "*.h" will be included in the presubmit.

    Args:
        program: list of presubmit check functions to run
        root: root path of the project
        repos: paths to the roots of Git repositories to check
        name: name to use to refer to this presubmit check run
        base: optional base Git commit to list files against
        paths: optional list of Git pathspecs to run the checks against
        exclude: regular expressions for Posix-style paths to exclude
        output_directory: where to place output files
        package_root: where to place package files
        only_list_steps: print step names instead of running them
        keep_going: whether to continue running checks if an error occurs

    Returns:
        True if all presubmit checks succeeded
    """
    repos = [repo.resolve() for repo in repos]

    for repo in repos:
        if git_repo.root(repo) != repo:
            raise ValueError(f'{repo} is not the root of a Git repo; '
                             'presubmit checks must be run from a Git repo')

    pathspecs_by_repo = _process_pathspecs(repos, paths)

    files: List[Path] = []

    for repo, pathspecs in pathspecs_by_repo.items():
        files += tools.exclude_paths(
            exclude, git_repo.list_files(base, pathspecs, repo), root)

        _LOG.info(
            'Checking %s',
            git_repo.describe_files(repo, repo, base, pathspecs, exclude,
                                    root))

    if output_directory is None:
        output_directory = root / '.presubmit'

    if package_root is None:
        package_root = output_directory / 'packages'

    presubmit = Presubmit(
        root=root,
        repos=repos,
        output_directory=output_directory,
        paths=files,
        package_root=package_root,
    )

    if only_list_steps:
        for check, _ in presubmit.apply_filters(program):
            print(check.name)
        return True

    if not isinstance(program, Program):
        program = Program('', program)

    return presubmit.run(program, keep_going)


def _make_str_tuple(value: Union[Iterable[str], str]) -> Tuple[str, ...]:
    return tuple([value] if isinstance(value, str) else value)


class Check:
    """Wraps a presubmit check function.

    This class consolidates the logic for running and logging a presubmit check.
    It also supports filtering the paths passed to the presubmit check.
    """
    def __init__(self,
                 check_function: Callable,
                 path_filter: _Filter = _Filter(),
                 always_run: bool = True):
        _ensure_is_valid_presubmit_check_function(check_function)

        self._check: Callable = check_function
        self.filter: _Filter = path_filter
        self.always_run: bool = always_run

        # Since Check wraps a presubmit function, adopt that function's name.
        self.__name__ = self._check.__name__

    def with_filter(
        self,
        *,
        endswith: Iterable[str] = '',
        exclude: Iterable[Union[Pattern[str], str]] = ()
    ) -> Check:
        endswith = self.filter.endswith
        if endswith:
            endswith = endswith + _make_str_tuple(endswith)
        exclude = self.filter.exclude + tuple(re.compile(e) for e in exclude)

        return Check(check_function=self._check,
                     path_filter=_Filter(endswith=endswith, exclude=exclude),
                     always_run=self.always_run)

    @property
    def name(self):
        return self.__name__

    def run(self, ctx: PresubmitContext, count: int, total: int) -> _Result:
        """Runs the presubmit check on the provided paths."""

        _print_ui(
            _box(_CHECK_UPPER, f'{count}/{total}', self.name,
                 plural(ctx.paths, "file")))

        _LOG.debug('[%d/%d] Running %s on %s', count, total, self.name,
                   plural(ctx.paths, "file"))

        start_time_s = time.time()
        result = self._call_function(ctx)
        time_str = _format_time(time.time() - start_time_s)
        _LOG.debug('%s %s', self.name, result.value)

        _print_ui(
            _box(_CHECK_LOWER, result.colorized(_LEFT), self.name, time_str))
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
            _print_ui()
            return _Result.CANCEL

        return _Result.PASS

    def __call__(self, ctx: PresubmitContext, *args, **kwargs):
        """Calling a Check calls its underlying function directly.

        This makes it possible to call functions wrapped by @filter_paths. The
        prior filters are ignored, so new filters may be applied.
        """
        return self._check(ctx, *args, **kwargs)


def _required_args(function: Callable) -> Iterable[Parameter]:
    """Returns the required arguments for a function."""
    optional_types = Parameter.VAR_POSITIONAL, Parameter.VAR_KEYWORD

    for param in signature(function).parameters.values():
        if param.default is param.empty and param.kind not in optional_types:
            yield param


def _ensure_is_valid_presubmit_check_function(check: Callable) -> None:
    """Checks if a Callable can be used as a presubmit check."""
    try:
        required_args = tuple(_required_args(check))
    except (TypeError, ValueError):
        raise TypeError('Presubmit checks must be callable, but '
                        f'{check!r} is a {type(check).__name__}')

    if len(required_args) != 1:
        raise TypeError(
            f'Presubmit check functions must have exactly one required '
            f'positional argument (the PresubmitContext), but '
            f'{check.__name__} has {len(required_args)} required arguments' +
            (f' ({", ".join(a.name for a in required_args)})'
             if required_args else ''))


def filter_paths(endswith: Iterable[str] = '',
                 exclude: Iterable[Union[Pattern[str], str]] = (),
                 always_run: bool = False) -> Callable[[Callable], Check]:
    """Decorator for filtering the paths list for a presubmit check function.

    Path filters only apply when the function is used as a presubmit check.
    Filters are ignored when the functions are called directly. This makes it
    possible to reuse functions wrapped in @filter_paths in other presubmit
    checks, potentially with different path filtering rules.

    Args:
        endswith: str or iterable of path endings to include
        exclude: regular expressions of paths to exclude

    Returns:
        a wrapped version of the presubmit function
    """
    def filter_paths_for_function(function: Callable):
        return Check(function,
                     _Filter(_make_str_tuple(endswith),
                             tuple(re.compile(e) for e in exclude)),
                     always_run=always_run)

    return filter_paths_for_function


def call(*args, **kwargs) -> None:
    """Optional subprocess wrapper that causes a PresubmitFailure on errors."""
    attributes, command = tools.format_command(args, kwargs)
    _LOG.debug('[RUN] %s\n%s', attributes, command)

    env = pw_cli.env.pigweed_environment()
    kwargs['stdout'] = subprocess.PIPE
    kwargs['stderr'] = subprocess.STDOUT

    process = subprocess.Popen(args, **kwargs)
    assert process.stdout

    if env.PW_PRESUBMIT_DISABLE_SUBPROCESS_CAPTURE:
        while True:
            line = process.stdout.readline().decode(errors='backslashreplace')
            if not line:
                break
            _LOG.info(line.rstrip())

    stdout, _ = process.communicate()

    logfunc = _LOG.warning if process.returncode else _LOG.debug
    logfunc('[FINISHED]\n%s', command)
    logfunc('[RESULT] %s with return code %d',
            'Failed' if process.returncode else 'Passed', process.returncode)
    if stdout:
        logfunc('[OUTPUT]\n%s', stdout.decode(errors='backslashreplace'))

    if process.returncode:
        raise PresubmitFailure
