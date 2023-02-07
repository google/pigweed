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
import copy
import dataclasses
import enum
from inspect import Parameter, signature
import itertools
import json
import logging
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import types
from typing import (
    Any,
    Callable,
    Collection,
    Dict,
    Iterable,
    Iterator,
    List,
    Optional,
    Pattern,
    Sequence,
    Set,
    Tuple,
    Union,
)

import pw_cli.color
import pw_cli.env
from pw_package import package_manager
from pw_presubmit import git_repo, tools
from pw_presubmit.tools import plural

_LOG: logging.Logger = logging.getLogger(__name__)

_COLOR = pw_cli.color.colors()

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
    return box.format(
        *style,
        section1=left + ('' if left.endswith(' ') else ' '),
        width1=_LEFT,
        section2=' ' + middle,
        width2=WIDTH - _LEFT - _RIGHT - 4,
        section3=right + ' ',
        width3=_RIGHT,
    )


class PresubmitFailure(Exception):
    """Optional exception to use for presubmit failures."""

    def __init__(
        self,
        description: str = '',
        path: Optional[Path] = None,
        line: Optional[int] = None,
    ):
        line_part: str = ''
        if line is not None:
            line_part = f'{line}:'
        super().__init__(
            f'{path}:{line_part} {description}' if path else description
        )


class PresubmitResult(enum.Enum):

    PASS = 'PASSED'  # Check completed successfully.
    FAIL = 'FAILED'  # Check failed.
    CANCEL = 'CANCEL'  # Check didn't complete.

    def colorized(self, width: int, invert: bool = False) -> str:
        if self is PresubmitResult.PASS:
            color = _COLOR.black_on_green if invert else _COLOR.green
        elif self is PresubmitResult.FAIL:
            color = _COLOR.black_on_red if invert else _COLOR.red
        elif self is PresubmitResult.CANCEL:
            color = _COLOR.yellow
        else:
            color = lambda value: value

        padding = (width - len(self.value)) // 2 * ' '
        return padding + color(self.value) + padding


class Program(collections.abc.Sequence):
    """A sequence of presubmit checks; basically a tuple with a name."""

    def __init__(self, name: str, steps: Iterable[Callable]):
        self.name = name

        def ensure_check(step):
            if isinstance(step, Check):
                return step
            return Check(step)

        self._steps: tuple[Check, ...] = tuple(
            {ensure_check(s): None for s in tools.flatten(steps)}
        )

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
            name: Program(name, checks) for name, checks in programs.items()
        }

    def all_steps(self) -> Dict[str, Check]:
        return {c.name: c for c in itertools.chain(*self.values())}

    def __getitem__(self, item: str) -> Program:
        return self._programs[item]

    def __iter__(self) -> Iterator[str]:
        return iter(self._programs)

    def __len__(self) -> int:
        return len(self._programs)


@dataclasses.dataclass
class LuciPipeline:
    round: int
    builds_from_previous_iteration: Sequence[int]

    @staticmethod
    def create(
        bbid: int,
        fake_pipeline_props: Optional[Dict[str, Any]] = None,
    ) -> Optional['LuciPipeline']:
        pipeline_props: Dict[str, Any]
        if fake_pipeline_props is not None:
            pipeline_props = fake_pipeline_props
        else:
            pipeline_props = (
                get_buildbucket_info(bbid)
                .get('input', {})
                .get('properties', {})
                .get('$pigweed/pipeline', {})
            )
        if not pipeline_props.get('inside_a_pipeline', False):
            return None

        return LuciPipeline(
            round=int(pipeline_props['round']),
            builds_from_previous_iteration=[
                int(x) for x in pipeline_props['builds_from_previous_iteration']
            ],
        )


def get_buildbucket_info(bbid) -> Dict[str, Any]:
    if not bbid or not shutil.which('bb'):
        return {}

    output = subprocess.check_output(
        ['bb', 'get', '-json', '-p', f'{bbid}'], text=True
    )
    return json.loads(output)


@dataclasses.dataclass
class LuciTrigger:
    """Details the pending change or submitted commit triggering the build."""

    number: int
    remote: str
    branch: str
    ref: str
    gerrit_name: str
    submitted: bool

    @property
    def gerrit_url(self):
        if not self.number:
            return self.gitiles_url
        return 'https://{}-review.googlesource.com/c/{}'.format(
            self.gerrit_name, self.number
        )

    @property
    def gitiles_url(self):
        return '{}/+/{}'.format(self.remote, self.ref)

    @staticmethod
    def create_from_environment(
        env: Optional[Dict[str, str]] = None,
    ) -> Sequence['LuciTrigger']:
        if not env:
            env = os.environ.copy()
        raw_path = env.get('TRIGGERING_CHANGES_JSON')
        if not raw_path:
            return ()
        path = Path(raw_path)
        if not path.is_file():
            return ()

        result = []
        with open(path, 'r') as ins:
            for trigger in json.load(ins):
                keys = {
                    'number',
                    'remote',
                    'branch',
                    'ref',
                    'gerrit_name',
                    'submitted',
                }
                if keys <= trigger.keys():
                    result.append(LuciTrigger(**{x: trigger[x] for x in keys}))

        return tuple(result)

    @staticmethod
    def create_for_testing():
        change = {
            'number': 123456,
            'remote': 'https://pigweed.googlesource.com/pigweed/pigweed',
            'branch': 'main',
            'ref': 'refs/changes/56/123456/1',
            'gerrit_name': 'pigweed',
            'submitted': True,
        }
        with tempfile.TemporaryDirectory() as tempdir:
            changes_json = Path(tempdir) / 'changes.json'
            with changes_json.open('w') as outs:
                json.dump([change], outs)
            env = {'TRIGGERING_CHANGES_JSON': changes_json}
            return LuciTrigger.create_from_environment(env)


@dataclasses.dataclass
class LuciContext:
    """LUCI-specific information about the environment."""

    buildbucket_id: int
    build_number: int
    project: str
    bucket: str
    builder: str
    swarming_task_id: str
    pipeline: Optional[LuciPipeline]
    triggers: Sequence[LuciTrigger] = dataclasses.field(default_factory=tuple)

    @staticmethod
    def create_from_environment(
        env: Optional[Dict[str, str]] = None,
        fake_pipeline_props: Optional[Dict[str, Any]] = None,
    ) -> Optional['LuciContext']:
        """Create a LuciContext from the environment."""

        if not env:
            env = os.environ.copy()

        luci_vars = [
            'BUILDBUCKET_ID',
            'BUILDBUCKET_NAME',
            'BUILD_NUMBER',
            'SWARMING_TASK_ID',
        ]
        if any(x for x in luci_vars if x not in env):
            return None

        project, bucket, builder = env['BUILDBUCKET_NAME'].split(':')

        bbid: int = 0
        pipeline: Optional[LuciPipeline] = None
        try:
            bbid = int(env['BUILDBUCKET_ID'])
            pipeline = LuciPipeline.create(bbid, fake_pipeline_props)

        except ValueError:
            pass

        result = LuciContext(
            buildbucket_id=bbid,
            build_number=int(env['BUILD_NUMBER']),
            project=project,
            bucket=bucket,
            builder=builder,
            swarming_task_id=env['SWARMING_TASK_ID'],
            pipeline=pipeline,
            triggers=LuciTrigger.create_from_environment(env),
        )
        _LOG.debug('%r', result)
        return result

    @staticmethod
    def create_for_testing():
        env = {
            'BUILDBUCKET_ID': '881234567890',
            'BUILDBUCKET_NAME': 'pigweed:bucket.try:builder-name',
            'BUILD_NUMBER': '123',
            'SWARMING_TASK_ID': 'cd2dac62d2',
        }
        return LuciContext.create_from_environment(env, {})


@dataclasses.dataclass
class FormatContext:
    """Context passed into formatting helpers.

    This class is a subset of PresubmitContext containing only what's needed by
    formatters.

    For full documentation on the members see the PresubmitContext section of
    pw_presubmit/docs.rst.

    Args:
        root: Source checkout root directory
        output_dir: Output directory for this specific language
        paths: Modified files for the presubmit step to check (often used in
            formatting steps but ignored in compile steps)
        package_root: Root directory for pw package installations
    """

    root: Optional[Path]
    output_dir: Path
    paths: Tuple[Path, ...]
    package_root: Path


@dataclasses.dataclass
class PresubmitContext:
    """Context passed into presubmit checks.

    For full documentation on the members see pw_presubmit/docs.rst.

    Args:
        root: Source checkout root directory
        repos: Repositories (top-level and submodules) processed by
            pw presubmit
        output_dir: Output directory for this specific presubmit step
        failure_summary_log: Path where steps should write a brief summary of
            any failures encountered for use by other tooling.
        paths: Modified files for the presubmit step to check (often used in
            formatting steps but ignored in compile steps)
        package_root: Root directory for pw package installations
        override_gn_args: Additional GN args processed by build.gn_gen()
        luci: Information about the LUCI build or None if not running in LUCI
        num_jobs: Number of jobs to run in parallel
        continue_after_build_error: For steps that compile, don't exit on the
            first compilation error
    """

    root: Path
    repos: Tuple[Path, ...]
    output_dir: Path
    failure_summary_log: Path
    paths: Tuple[Path, ...]
    package_root: Path
    luci: Optional[LuciContext]
    override_gn_args: Dict[str, str]
    num_jobs: Optional[int] = None
    continue_after_build_error: bool = False
    _failed: bool = False

    @property
    def failed(self) -> bool:
        return self._failed

    def fail(
        self,
        description: str,
        path: Optional[Path] = None,
        line: Optional[int] = None,
    ):
        """Add a failure to this presubmit step.

        If this is called at least once the step fails, but not immediately—the
        check is free to continue and possibly call this method again.
        """
        _LOG.warning('%s', PresubmitFailure(description, path, line))
        self._failed = True

    @staticmethod
    def create_for_testing():
        parsed_env = pw_cli.env.pigweed_environment()
        root = Path(parsed_env.PW_PROJECT_ROOT)
        presubmit_root = root / 'out' / 'presubmit'
        return PresubmitContext(
            root=root,
            repos=(root,),
            output_dir=presubmit_root / 'test',
            failure_summary_log=presubmit_root / 'failure-summary.log',
            paths=(root / 'foo.cc', root / 'foo.py'),
            package_root=root / 'environment' / 'packages',
            luci=None,
            override_gn_args={},
        )


class FileFilter:
    """Allows checking if a path matches a series of filters.

    Positive filters (e.g. the file name matches a regex) and negative filters
    (path does not match a regular expression) may be applied.
    """

    _StrOrPattern = Union[Pattern, str]

    def __init__(
        self,
        *,
        exclude: Iterable[_StrOrPattern] = (),
        endswith: Iterable[str] = (),
        name: Iterable[_StrOrPattern] = (),
        suffix: Iterable[str] = (),
    ) -> None:
        """Creates a FileFilter with the provided filters.

        Args:
            endswith: True if the end of the path is equal to any of the passed
                      strings
            exclude: If any of the passed regular expresion match return False.
                     This overrides and other matches.
            name: Regexs to match with file names(pathlib.Path.name). True if
                  the resulting regex matches the entire file name.
            suffix: True if final suffix (as determined by pathlib.Path) is
                    matched by any of the passed str.
        """
        self.exclude = tuple(re.compile(i) for i in exclude)

        self.endswith = tuple(endswith)
        self.name = tuple(re.compile(i) for i in name)
        self.suffix = tuple(suffix)

    def matches(self, path: Union[str, Path]) -> bool:
        """Returns true if the path matches any filter but not an exclude.

        If no positive filters are specified, any paths that do not match a
        negative filter are considered to match.

        If 'path' is a Path object it is rendered as a posix path (i.e.
        using "/" as the path seperator) before testing with 'exclude' and
        'endswith'.
        """

        posix_path = path.as_posix() if isinstance(path, Path) else path
        if any(bool(exp.search(posix_path)) for exp in self.exclude):
            return False

        # If there are no positive filters set, accept all paths.
        no_filters = not self.endswith and not self.name and not self.suffix

        path_obj = Path(path)
        return (
            no_filters
            or path_obj.suffix in self.suffix
            or any(regex.fullmatch(path_obj.name) for regex in self.name)
            or any(posix_path.endswith(end) for end in self.endswith)
        )

    def apply_to_check(self, always_run: bool = False) -> Callable:
        def wrapper(func: Callable) -> Check:
            if isinstance(func, Check):
                clone = copy.copy(func)
                clone.filter = self
                clone.always_run = clone.always_run or always_run
                return clone

            return Check(check=func, path_filter=self, always_run=always_run)

        return wrapper


def _print_ui(*args) -> None:
    """Prints to stdout and flushes to stay in sync with logs on stderr."""
    print(*args, flush=True)


@dataclasses.dataclass
class FilteredCheck:
    check: Check
    paths: Sequence[Path]
    substep: Optional[str] = None

    @property
    def name(self) -> str:
        return self.check.name

    def run(self, ctx: PresubmitContext, count: int, total: int):
        return self.check.run(ctx, count, total, self.substep)


class Presubmit:
    """Runs a series of presubmit checks on a list of files."""

    def __init__(
        self,
        root: Path,
        repos: Sequence[Path],
        output_directory: Path,
        paths: Sequence[Path],
        package_root: Path,
        override_gn_args: Dict[str, str],
        continue_after_build_error: bool,
    ):
        self._root = root.resolve()
        self._repos = tuple(repos)
        self._output_directory = output_directory.resolve()
        self._paths = tuple(paths)
        self._relative_paths = tuple(
            tools.relative_paths(self._paths, self._root)
        )
        self._package_root = package_root.resolve()
        self._override_gn_args = override_gn_args
        self._continue_after_build_error = continue_after_build_error

    def run(
        self,
        program: Program,
        keep_going: bool = False,
        substep: Optional[str] = None,
    ) -> bool:
        """Executes a series of presubmit checks on the paths."""

        checks = self.apply_filters(program)
        if substep:
            assert (
                len(checks) == 1
            ), 'substeps not supported with multiple steps'
            checks[0].substep = substep

        _LOG.debug('Running %s for %s', program.title(), self._root.name)
        _print_ui(_title(f'{self._root.name}: {program.title()}'))

        _LOG.info(
            '%d of %d checks apply to %s in %s',
            len(checks),
            len(program),
            plural(self._paths, 'file'),
            self._root,
        )

        _print_ui()
        for line in tools.file_summary(self._relative_paths):
            _print_ui(line)
        _print_ui()

        if not self._paths:
            _print_ui(_COLOR.yellow('No files are being checked!'))

        _LOG.debug('Checks:\n%s', '\n'.join(c.name for c in checks))

        start_time: float = time.time()
        passed, failed, skipped = self._execute_checks(checks, keep_going)
        self._log_summary(time.time() - start_time, passed, failed, skipped)

        return not failed and not skipped

    def apply_filters(self, program: Sequence[Callable]) -> List[FilteredCheck]:
        """Returns list of FilteredCheck for checks that should run."""
        checks = [c if isinstance(c, Check) else Check(c) for c in program]
        filter_to_checks: Dict[
            FileFilter, List[Check]
        ] = collections.defaultdict(list)

        for chk in checks:
            filter_to_checks[chk.filter].append(chk)

        check_to_paths = self._map_checks_to_paths(filter_to_checks)
        return [
            FilteredCheck(c, check_to_paths[c])
            for c in checks
            if c in check_to_paths
        ]

    def _map_checks_to_paths(
        self, filter_to_checks: Dict[FileFilter, List[Check]]
    ) -> Dict[Check, Sequence[Path]]:
        checks_to_paths: Dict[Check, Sequence[Path]] = {}

        posix_paths = tuple(p.as_posix() for p in self._relative_paths)

        for filt, checks in filter_to_checks.items():
            filtered_paths = tuple(
                path
                for path, filter_path in zip(self._paths, posix_paths)
                if filt.matches(filter_path)
            )

            for chk in checks:
                if filtered_paths or chk.always_run:
                    checks_to_paths[chk] = filtered_paths
                else:
                    _LOG.debug('Skipping "%s": no relevant files', chk.name)

        return checks_to_paths

    def _log_summary(
        self, time_s: float, passed: int, failed: int, skipped: int
    ) -> None:
        summary_items = []
        if passed:
            summary_items.append(f'{passed} passed')
        if failed:
            summary_items.append(f'{failed} failed')
        if skipped:
            summary_items.append(f'{skipped} not run')
        summary = ', '.join(summary_items) or 'nothing was done'

        if failed or skipped:
            result = PresubmitResult.FAIL
        else:
            result = PresubmitResult.PASS
        total = passed + failed + skipped

        _LOG.debug(
            'Finished running %d checks on %s in %.1f s',
            total,
            plural(self._paths, 'file'),
            time_s,
        )
        _LOG.debug('Presubmit checks %s: %s', result.value, summary)

        _print_ui(
            _box(
                _SUMMARY_BOX,
                result.colorized(_LEFT, invert=True),
                f'{total} checks on {plural(self._paths, "file")}: {summary}',
                _format_time(time_s),
            )
        )

    def _create_presubmit_context(  # pylint: disable=no-self-use
        self, **kwargs
    ):
        """Create a PresubmitContext. Override if needed in subclasses."""
        return PresubmitContext(**kwargs)

    @contextlib.contextmanager
    def _context(self, filtered_check: FilteredCheck):
        # There are many characters banned from filenames on Windows. To
        # simplify things, just strip everything that's not a letter, digit,
        # or underscore.
        sanitized_name = re.sub(r'[\W_]+', '_', filtered_check.name).lower()
        output_directory = self._output_directory.joinpath(sanitized_name)
        os.makedirs(output_directory, exist_ok=True)

        failure_summary_log = output_directory / 'failure-summary.log'
        failure_summary_log.unlink(missing_ok=True)

        handler = logging.FileHandler(
            output_directory.joinpath('step.log'), mode='w'
        )
        handler.setLevel(logging.DEBUG)

        try:
            _LOG.addHandler(handler)

            yield self._create_presubmit_context(
                root=self._root,
                repos=self._repos,
                output_dir=output_directory,
                failure_summary_log=failure_summary_log,
                paths=filtered_check.paths,
                package_root=self._package_root,
                override_gn_args=self._override_gn_args,
                continue_after_build_error=self._continue_after_build_error,
                luci=LuciContext.create_from_environment(),
            )

        finally:
            _LOG.removeHandler(handler)

    def _execute_checks(
        self, program: List[FilteredCheck], keep_going: bool
    ) -> Tuple[int, int, int]:
        """Runs presubmit checks; returns (passed, failed, skipped) lists."""
        passed = failed = 0

        for i, filtered_check in enumerate(program, 1):
            with self._context(filtered_check) as ctx:
                result = filtered_check.run(ctx, i, len(program))

            if result is PresubmitResult.PASS:
                passed += 1
            elif result is PresubmitResult.CANCEL:
                break
            else:
                failed += 1
                if not keep_going:
                    break

        return passed, failed, len(program) - passed - failed


def _process_pathspecs(
    repos: Iterable[Path], pathspecs: Iterable[str]
) -> Dict[Path, List[str]]:
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
                    f'{pathspec} is not in a Git repository in this presubmit'
                )

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


def run(  # pylint: disable=too-many-arguments,too-many-locals
    program: Sequence[Check],
    root: Path,
    repos: Collection[Path] = (),
    base: Optional[str] = None,
    paths: Sequence[str] = (),
    exclude: Sequence[Pattern] = (),
    output_directory: Optional[Path] = None,
    package_root: Optional[Path] = None,
    only_list_steps: bool = False,
    override_gn_args: Sequence[Tuple[str, str]] = (),
    keep_going: bool = False,
    continue_after_build_error: bool = False,
    presubmit_class: type = Presubmit,
    list_steps_file: Optional[Path] = None,
    substep: Optional[str] = None,
) -> bool:
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
        override_gn_args: additional GN args to set on steps
        keep_going: continue running presubmit steps after a step fails
        continue_after_build_error: continue building if a build step fails
        presubmit_class: class to use to run Presubmits, should inherit from
            Presubmit class above
        list_steps_file: File created by --only-list-steps, used to keep from
            recalculating affected files.
        substep: run only part of a single check

    Returns:
        True if all presubmit checks succeeded
    """
    repos = [repo.resolve() for repo in repos]

    non_empty_repos = []
    for repo in repos:
        if list(repo.iterdir()):
            non_empty_repos.append(repo)
            if git_repo.root(repo) != repo:
                raise ValueError(
                    f'{repo} is not the root of a Git repo; '
                    'presubmit checks must be run from a Git repo'
                )
    repos = non_empty_repos

    pathspecs_by_repo = _process_pathspecs(repos, paths)

    files: List[Path] = []
    list_steps_data: List = []

    if list_steps_file:
        with list_steps_file.open() as ins:
            list_steps_data = json.load(ins)
        for step in list_steps_data:
            files.extend(Path(x) for x in step.get("paths", ()))
        files = sorted(set(files))
        _LOG.info('Loaded %d paths from file %s', len(files), list_steps_file)

    else:
        for repo, pathspecs in pathspecs_by_repo.items():
            files += tools.exclude_paths(
                exclude, git_repo.list_files(base, pathspecs, repo), root
            )

            _LOG.info(
                'Checking %s',
                git_repo.describe_files(
                    repo, repo, base, pathspecs, exclude, root
                ),
            )

    if output_directory is None:
        output_directory = root / '.presubmit'

    if package_root is None:
        package_root = output_directory / 'packages'

    presubmit = presubmit_class(
        root=root,
        repos=repos,
        output_directory=output_directory,
        paths=files,
        package_root=package_root,
        override_gn_args=dict(override_gn_args or {}),
        continue_after_build_error=continue_after_build_error,
    )

    if only_list_steps:
        steps: List[Dict] = []
        for filtered_check in presubmit.apply_filters(program):
            step = {
                'name': filtered_check.name,
                'paths': [str(x) for x in filtered_check.paths],
            }
            substeps = filtered_check.check.substeps()
            if len(substeps) > 1:
                step['substeps'] = [x.name for x in substeps]
            steps.append(step)
        json.dump(steps, sys.stdout, indent=2)
        sys.stdout.write('\n')
        return True

    if not isinstance(program, Program):
        program = Program('', program)

    return presubmit.run(program, keep_going, substep=substep)


def _make_str_tuple(value: Union[Iterable[str], str]) -> Tuple[str, ...]:
    return tuple([value] if isinstance(value, str) else value)


def check(*args, **kwargs):
    """Turn a function into a presubmit check.

    Args:
        *args: Passed through to function.
        *kwargs: Passed through to function.

    If only one argument is provided and it's a function, this function acts
    as a decorator and creates a Check from the function. Example of this kind
    of usage:

    @check
    def pragma_once(ctx: PresubmitContext):
        pass

    Otherwise, save the arguments, and return a decorator that turns a function
    into a Check, but with the arguments added onto the Check constructor.
    Example of this kind of usage:

    @check(name='pragma_twice')
    def pragma_once(ctx: PresubmitContext):
        pass
    """
    if (
        len(args) == 1
        and isinstance(args[0], types.FunctionType)
        and not kwargs
    ):
        # Called as a regular decorator.
        return Check(args[0])

    def decorator(check_function):
        return Check(check_function, *args, **kwargs)

    return decorator


@dataclasses.dataclass
class SubStep:
    name: Optional[str]
    _func: Callable[..., PresubmitResult]
    args: Sequence[Any] = ()
    kwargs: Dict[str, Any] = dataclasses.field(default_factory=lambda: {})

    def __call__(self, ctx: PresubmitContext) -> PresubmitResult:
        if self.name:
            _LOG.info('%s', self.name)
        return self._func(ctx, *self.args, **self.kwargs)


class Check:
    """Wraps a presubmit check function.

    This class consolidates the logic for running and logging a presubmit check.
    It also supports filtering the paths passed to the presubmit check.
    """

    def __init__(
        self,
        check: Union[  # pylint: disable=redefined-outer-name
            Callable, Iterable[SubStep]
        ],
        path_filter: FileFilter = FileFilter(),
        always_run: bool = True,
        name: Optional[str] = None,
        doc: Optional[str] = None,
    ) -> None:
        # Since Check wraps a presubmit function, adopt that function's name.
        self.name: str = ''
        self.doc: str = ''
        if isinstance(check, Check):
            self.name = check.name
            self.doc = check.doc
        elif callable(check):
            self.name = check.__name__
            self.doc = check.__doc__ or ''

        if name:
            self.name = name
        if doc:
            self.doc = doc

        if not self.name:
            raise ValueError('no name for step')

        self._substeps_raw: Iterable[SubStep]
        if isinstance(check, collections.abc.Iterator):
            self._substeps_raw = check
        else:
            assert callable(check)
            _ensure_is_valid_presubmit_check_function(check)
            self._substeps_raw = iter((SubStep(None, check),))
        self._substeps_saved: Sequence[SubStep] = ()

        self.filter = path_filter
        self.always_run: bool = always_run

    def substeps(self) -> Sequence[SubStep]:
        """Return the SubSteps of the current step.

        This is where the list of SubSteps is actually evaluated. It can't be
        evaluated in the constructor because the Iterable passed into the
        constructor might not be ready yet.
        """
        if not self._substeps_saved:
            self._substeps_saved = tuple(self._substeps_raw)
        return self._substeps_saved

    def __repr__(self):
        # This returns just the name so it's easy to show the entire list of
        # steps with '--help'.
        return self.name

    def unfiltered(self) -> Check:
        """Create a new check identical to this one, but without the filter."""
        clone = copy.copy(self)
        clone.filter = FileFilter()
        return clone

    def with_filter(
        self,
        *,
        endswith: Iterable[str] = (),
        exclude: Iterable[Union[Pattern[str], str]] = (),
    ) -> Check:
        """Create a new check identical to this one, but with extra filters.

        Add to the existing filter, perhaps to exclude an additional directory.

        Args:
            endswith: Passed through to FileFilter.
            exclude: Passed through to FileFilter.

        Returns a new check.
        """
        return self.with_file_filter(
            FileFilter(endswith=_make_str_tuple(endswith), exclude=exclude)
        )

    def with_file_filter(self, file_filter: FileFilter) -> Check:
        """Create a new check identical to this one, but with extra filters.

        Add to the existing filter, perhaps to exclude an additional directory.

        Args:
            file_filter: Additional filter rules.

        Returns a new check.
        """
        clone = copy.copy(self)
        if clone.filter:
            clone.filter.exclude = clone.filter.exclude + file_filter.exclude
            clone.filter.endswith = clone.filter.endswith + file_filter.endswith
            clone.filter.name = file_filter.name or clone.filter.name
            clone.filter.suffix = clone.filter.suffix + file_filter.suffix
        else:
            clone.filter = file_filter
        return clone

    def run(
        self,
        ctx: PresubmitContext,
        count: int,
        total: int,
        substep: Optional[str] = None,
    ) -> PresubmitResult:
        """Runs the presubmit check on the provided paths."""

        _print_ui(
            _box(
                _CHECK_UPPER,
                f'{count}/{total}',
                self.name,
                plural(ctx.paths, "file"),
            )
        )

        substep_part = f'.{substep}' if substep else ''
        _LOG.debug(
            '[%d/%d] Running %s%s on %s',
            count,
            total,
            self.name,
            substep_part,
            plural(ctx.paths, "file"),
        )

        start_time_s = time.time()
        result: PresubmitResult
        if substep:
            result = self.run_substep(ctx, substep)
        else:
            result = self(ctx)
        time_str = _format_time(time.time() - start_time_s)
        _LOG.debug('%s %s', self.name, result.value)

        _print_ui(
            _box(_CHECK_LOWER, result.colorized(_LEFT), self.name, time_str)
        )
        _LOG.debug('%s duration:%s', self.name, time_str)

        return result

    def _try_call(
        self,
        func: Callable,
        ctx,
        *args,
        **kwargs,
    ) -> PresubmitResult:
        try:
            result = func(ctx, *args, **kwargs)
            if ctx.failed:
                return PresubmitResult.FAIL
            if isinstance(result, PresubmitResult):
                return result
            return PresubmitResult.PASS

        except PresubmitFailure as failure:
            if str(failure):
                _LOG.warning('%s', failure)
            return PresubmitResult.FAIL

        except Exception as failure:  # pylint: disable=broad-except
            _LOG.exception('Presubmit check %s failed!', self.name)
            return PresubmitResult.FAIL

        except KeyboardInterrupt:
            _print_ui()
            return PresubmitResult.CANCEL

    def run_substep(
        self, ctx: PresubmitContext, name: Optional[str]
    ) -> PresubmitResult:
        for substep in self.substeps():
            if substep.name == name:
                return substep(ctx)

        expected = ', '.join(repr(s.name) for s in self.substeps())
        raise LookupError(f'bad substep name: {name!r} (expected: {expected})')

    def __call__(self, ctx: PresubmitContext) -> PresubmitResult:
        """Calling a Check calls its underlying substeps directly.

        This makes it possible to call functions wrapped by @filter_paths. The
        prior filters are ignored, so new filters may be applied.
        """
        result: PresubmitResult
        for substep in self.substeps():
            result = self._try_call(substep, ctx)
            if result and result != PresubmitResult.PASS:
                return result
        return PresubmitResult.PASS


def _required_args(function: Callable) -> Iterable[Parameter]:
    """Returns the required arguments for a function."""
    optional_types = Parameter.VAR_POSITIONAL, Parameter.VAR_KEYWORD

    for param in signature(function).parameters.values():
        if param.default is param.empty and param.kind not in optional_types:
            yield param


def _ensure_is_valid_presubmit_check_function(chk: Callable) -> None:
    """Checks if a Callable can be used as a presubmit check."""
    try:
        required_args = tuple(_required_args(chk))
    except (TypeError, ValueError):
        raise TypeError(
            'Presubmit checks must be callable, but '
            f'{chk!r} is a {type(chk).__name__}'
        )

    if len(required_args) != 1:
        raise TypeError(
            f'Presubmit check functions must have exactly one required '
            f'positional argument (the PresubmitContext), but '
            f'{chk.__name__} has {len(required_args)} required arguments'
            + (
                f' ({", ".join(a.name for a in required_args)})'
                if required_args
                else ''
            )
        )


def filter_paths(
    *,
    endswith: Iterable[str] = (),
    exclude: Iterable[Union[Pattern[str], str]] = (),
    file_filter: Optional[FileFilter] = None,
    always_run: bool = False,
) -> Callable[[Callable], Check]:
    """Decorator for filtering the paths list for a presubmit check function.

    Path filters only apply when the function is used as a presubmit check.
    Filters are ignored when the functions are called directly. This makes it
    possible to reuse functions wrapped in @filter_paths in other presubmit
    checks, potentially with different path filtering rules.

    Args:
        endswith: str or iterable of path endings to include
        exclude: regular expressions of paths to exclude
        file_filter: FileFilter used to select files
        always_run: Run check even when no files match
    Returns:
        a wrapped version of the presubmit function
    """

    if file_filter:
        real_file_filter = file_filter
        if endswith or exclude:
            raise ValueError(
                'Must specify either file_filter or '
                'endswith/exclude args, not both'
            )
    else:
        # TODO(b/238426363): Remove these arguments and use FileFilter only.
        real_file_filter = FileFilter(
            endswith=_make_str_tuple(endswith), exclude=exclude
        )

    def filter_paths_for_function(function: Callable):
        return Check(function, real_file_filter, always_run=always_run)

    return filter_paths_for_function


def call(*args, **kwargs) -> None:
    """Optional subprocess wrapper that causes a PresubmitFailure on errors."""
    attributes, command = tools.format_command(args, kwargs)
    _LOG.debug('[RUN] %s\n%s', attributes, command)

    tee = kwargs.pop('tee', None)
    propagate_sigterm = kwargs.pop('propagate_sigterm', False)

    env = pw_cli.env.pigweed_environment()
    kwargs['stdout'] = subprocess.PIPE
    kwargs['stderr'] = subprocess.STDOUT

    process = subprocess.Popen(args, **kwargs)
    assert process.stdout

    # Set up signal handler if requested.
    signaled = False
    if propagate_sigterm:

        def signal_handler(_signal_number: int, _stack_frame: Any) -> None:
            nonlocal signaled
            signaled = True
            process.terminate()

        previous_signal_handler = signal.signal(signal.SIGTERM, signal_handler)

    if env.PW_PRESUBMIT_DISABLE_SUBPROCESS_CAPTURE:
        while True:
            line = process.stdout.readline().decode(errors='backslashreplace')
            if not line:
                break
            _LOG.info(line.rstrip())
            if tee:
                tee.write(line)

    stdout, _ = process.communicate()
    if tee:
        tee.write(stdout.decode(errors='backslashreplace'))

    logfunc = _LOG.warning if process.returncode else _LOG.debug
    logfunc('[FINISHED]\n%s', command)
    logfunc(
        '[RESULT] %s with return code %d',
        'Failed' if process.returncode else 'Passed',
        process.returncode,
    )
    if stdout:
        logfunc('[OUTPUT]\n%s', stdout.decode(errors='backslashreplace'))

    if propagate_sigterm:
        signal.signal(signal.SIGTERM, previous_signal_handler)
        if signaled:
            _LOG.warning('Exiting due to SIGTERM.')
            sys.exit(1)

    if process.returncode:
        raise PresubmitFailure


def install_package(
    ctx: Union[FormatContext, PresubmitContext],
    name: str,
    force: bool = False,
) -> None:
    """Install package with given name in given path."""
    root = ctx.package_root
    mgr = package_manager.PackageManager(root)

    if not mgr.list():
        raise PresubmitFailure(
            'no packages configured, please import your pw_package '
            'configuration module'
        )

    if not mgr.status(name) or force:
        mgr.install(name, force=force)
