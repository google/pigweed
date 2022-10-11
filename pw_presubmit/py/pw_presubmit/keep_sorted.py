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
"""Keep specified lists sorted."""

import argparse
import dataclasses
import difflib
import logging
import os
from pathlib import Path
import re
import sys
from typing import Collection, List, Optional, Pattern, Sequence, Union

import pw_cli
from . import cli, git_repo, presubmit, tools

_COLOR = pw_cli.color.colors()
_LOG: logging.Logger = logging.getLogger(__name__)

# Ignore a whole section. Please do not change the order of these lines.
_START = re.compile(r'keep-sorted: (begin|start)', re.IGNORECASE)
_END = re.compile(r'keep-sorted: (stop|end)', re.IGNORECASE)
_IGNORE_CASE = re.compile(r'ignore\W*case', re.IGNORECASE)

# Only include these literals here so keep_sorted doesn't try to reorder later
# test lines.
START, END = """
keep-sorted: start
keep-sorted: end
""".strip().splitlines()


@dataclasses.dataclass
class KeepSortedContext:
    paths: List[Path]
    fix: bool
    failed: bool = False

    def fail(self,
             description: str = '',
             path: Path = None,
             line: int = None) -> None:
        if not self.fix:
            self.failed = True

        line_part: str = ''
        if line is not None:
            line_part = f'{line}:'

        log = _LOG.error
        if self.fix:
            log = _LOG.warning

        if path:
            log('%s:%s %s', path, line_part, description)
        else:
            log('%s', description)


class KeepSortedParsingError(presubmit.PresubmitFailure):
    pass


class _FileSorter:
    def __init__(self, ctx: Union[presubmit.PresubmitContext,
                                  KeepSortedContext], path: Path):
        self.ctx = ctx
        self.path: Path = path
        self.all_lines: List[str] = []
        self.changed: bool = False

    def _process_block(self, start_line: str, lines: List[str], end_line: str,
                       i: int, ignore_case: bool) -> Sequence[str]:
        sort_key = lambda x: x
        if ignore_case:
            sort_key = lambda x: (x.lower(), x)
        sorted_lines = sorted(lines, key=sort_key)

        if lines != sorted_lines:
            self.changed = True
            self.ctx.fail('keep-sorted block is not sorted', self.path, i)
            _LOG.info('  %s', start_line.rstrip())
            diff = difflib.Differ()
            for dline in diff.compare(
                [x.rstrip() for x in lines],
                [x.rstrip() for x in sorted_lines],
            ):
                if dline.startswith('-'):
                    dline = _COLOR.red(dline)
                elif dline.startswith('+'):
                    dline = _COLOR.green(dline)
                _LOG.info(dline)
            _LOG.info('  %s', end_line.rstrip())

        return sorted_lines

    def _parse_file(self, ins):
        in_block: bool = False
        ignore_case: bool = False
        start_line: Optional[str] = None
        end_line: Optional[str] = None
        lines: List[str] = []

        for i, line in enumerate(ins, start=1):
            if in_block:
                if _START.search(line):
                    raise KeepSortedParsingError(
                        f'found {line.strip()!r} inside keep-sorted block',
                        self.path, i)

                if _END.search(line):
                    _LOG.debug('Found end line %d %r', i, line)
                    end_line = line
                    in_block = False
                    assert start_line  # Implicitly cast from Optional.
                    self.all_lines.extend(
                        self._process_block(start_line, lines, end_line, i,
                                            ignore_case))
                    start_line = end_line = None
                    self.all_lines.append(line)
                    lines = []

                else:
                    _LOG.debug('Adding to block line %d %r', i, line)
                    lines.append(line)

            elif start_match := _START.search(line):
                _LOG.debug('Found start line %d %r', i, line)
                ignore_case = bool(_IGNORE_CASE.search(line))
                _LOG.debug('ignore_case: %s', ignore_case)
                start_line = line
                in_block = True
                self.all_lines.append(line)

                remaining = line[start_match.end():].strip()
                remaining = _IGNORE_CASE.sub('', remaining, count=1).strip()
                if remaining:
                    raise KeepSortedParsingError(
                        f'unrecognized directive on keep-sorted line: '
                        f'{remaining}', self.path, i)

            elif _END.search(line):
                raise KeepSortedParsingError(
                    f'found {line.strip()!r} outside keep-sorted block',
                    self.path, i)

            else:
                self.all_lines.append(line)

        if in_block:
            raise KeepSortedParsingError(
                f'found EOF while looking for "{END}"', self.path)

    def sort(self) -> None:
        """Check for unsorted keep-sorted blocks."""
        _LOG.debug('Evaluating path %s', self.path)
        try:
            with self.path.open() as ins:
                _LOG.debug('Processing %s', self.path)
                self._parse_file(ins)

        except UnicodeDecodeError:
            # File is not text, like a gif.
            _LOG.debug('File %s is not a text file', self.path)

    def write(self, path: Path = None) -> None:
        if not self.changed:
            return
        if not path:
            path = self.path
        with path.open('w') as outs:
            outs.writelines(self.all_lines)
            _LOG.info('Applied keep-sorted changes to %s', path)


def _print_howto_fix(paths: Sequence[Path]) -> None:
    def path_relative_to_cwd(path):
        try:
            return Path(path).resolve().relative_to(Path.cwd().resolve())
        except ValueError:
            return Path(path).resolve()

    message = (f'  pw keep-sorted --fix {path_relative_to_cwd(path)}'
               for path in paths)
    _LOG.warning('To sort these blocks, run:\n\n%s\n', '\n'.join(message))


def _process_files(
    ctx: Union[presubmit.PresubmitContext,
               KeepSortedContext]) -> Sequence[Path]:
    fix = getattr(ctx, 'fix', False)
    changed_paths = []

    for path in ctx.paths:
        if path.is_symlink() or path.is_dir():
            continue

        try:
            sorter = _FileSorter(ctx, path)

            sorter.sort()
            if sorter.changed:
                changed_paths.append(path)
                if fix:
                    sorter.write()

        except KeepSortedParsingError as exc:
            ctx.fail(str(exc))

    return changed_paths


@presubmit.Check
def keep_sorted(ctx: presubmit.PresubmitContext) -> None:
    """Presubmit check that ensures specified lists remain sorted."""

    changed_paths = _process_files(ctx)

    if changed_paths:
        _print_howto_fix(changed_paths)


def parse_args() -> argparse.Namespace:
    """Creates an argument parser and parses arguments."""

    parser = argparse.ArgumentParser(description=__doc__)
    cli.add_path_arguments(parser)
    parser.add_argument('--fix',
                        action='store_true',
                        help='Apply fixes in place.')

    return parser.parse_args()


def keep_sorted_in_repo(paths: Collection[Union[Path, str]], fix: bool,
                        exclude: Collection[Pattern[str]], base: str) -> int:
    """Checks or fixes keep-sorted blocks for files in a Git repo."""

    files = [Path(path).resolve() for path in paths if os.path.isfile(path)]
    repo = git_repo.root() if git_repo.is_repo() else None

    # Implement a graceful fallback in case the tracking branch isn't available.
    if (base == git_repo.TRACKING_BRANCH_ALIAS
            and not git_repo.tracking_branch(repo)):
        _LOG.warning(
            'Failed to determine the tracking branch, using --base HEAD~1 '
            'instead of listing all files')
        base = 'HEAD~1'

    # If this is a Git repo, list the original paths with git ls-files or diff.
    if repo:
        project_root = Path(pw_cli.env.pigweed_environment().PW_PROJECT_ROOT)
        _LOG.info(
            'Sorting %s',
            git_repo.describe_files(repo, Path.cwd(), base, paths, exclude,
                                    project_root))

        # Add files from Git and remove duplicates.
        files = sorted(
            set(tools.exclude_paths(exclude, git_repo.list_files(base, paths)))
            | set(files))
    elif base:
        _LOG.critical(
            'A base commit may only be provided if running from a Git repo')
        return 1

    ctx = KeepSortedContext(paths=files, fix=fix)
    changed_paths = _process_files(ctx)

    if not fix and changed_paths:
        _print_howto_fix(changed_paths)

    return int(ctx.failed)


def main() -> int:
    return keep_sorted_in_repo(**vars(parse_args()))


if __name__ == '__main__':
    pw_cli.log.install(logging.INFO)
    sys.exit(main())
