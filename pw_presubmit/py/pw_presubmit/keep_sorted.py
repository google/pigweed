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
from typing import (Callable, Collection, List, Optional, Pattern, Sequence,
                    Tuple, Union)

import pw_cli
from . import cli, git_repo, presubmit, tools

_COLOR = pw_cli.color.colors()
_LOG: logging.Logger = logging.getLogger(__name__)

# Ignore a whole section. Please do not change the order of these lines.
_START = re.compile(r'keep-sorted: (begin|start)', re.IGNORECASE)
_END = re.compile(r'keep-sorted: (stop|end)', re.IGNORECASE)
_IGNORE_CASE = re.compile(r'ignore-case', re.IGNORECASE)
_ALLOW_DUPES = re.compile(r'allow-dupes', re.IGNORECASE)
_IGNORE_PREFIX = re.compile(r'ignore-prefix=(\S+)', re.IGNORECASE)

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
             path: Optional[Path] = None,
             line: Optional[int] = None) -> None:
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


@dataclasses.dataclass
class _Block:
    ignore_case: bool = False
    allow_dupes: bool = False
    ignored_prefixes: Sequence[str] = dataclasses.field(default_factory=list)
    start_line_number: int = -1
    start_line: str = ''
    end_line: str = ''
    lines: List[str] = dataclasses.field(default_factory=list)


class _FileSorter:
    def __init__(self, ctx: Union[presubmit.PresubmitContext,
                                  KeepSortedContext], path: Path):
        self.ctx = ctx
        self.path: Path = path
        self.all_lines: List[str] = []
        self.changed: bool = False

    def _process_block(self, block: _Block) -> Sequence[str]:
        lines_after_dupes: List[str] = []
        if block.allow_dupes:
            lines_after_dupes = block.lines
        else:
            lines_after_dupes = list({x: None for x in block.lines})

        sort_key_funcs: List[Callable[[Tuple[str, ...]], Tuple[str, ...]]] = []

        if block.ignored_prefixes:

            def strip_ignored_prefixes(val):
                """Remove one ignored prefix from val, if present."""
                wo_white = val[0].lstrip()
                white = val[0][0:-len(wo_white)]
                for prefix in block.ignored_prefixes:
                    if wo_white.startswith(prefix):
                        return (f'{white}{wo_white[len(prefix):]}', val[1])
                return (val[0], val[1])

            sort_key_funcs.append(strip_ignored_prefixes)

        if block.ignore_case:
            sort_key_funcs.append(lambda val: (val[0].lower(), val[1]))

        def sort_key(val):
            vals = (val, val)
            for sort_key_func in sort_key_funcs:
                vals = sort_key_func(vals)
            return vals

        for val in lines_after_dupes:
            _LOG.debug('For sorting: %r => %r', val, sort_key(val))

        sorted_lines = sorted(lines_after_dupes, key=sort_key)

        if block.lines != sorted_lines:
            self.changed = True
            self.ctx.fail('keep-sorted block is not sorted', self.path,
                          block.start_line_number)
            _LOG.info('  %s', block.start_line.rstrip())
            diff = difflib.Differ()
            for dline in diff.compare(
                [x.rstrip() for x in block.lines],
                [x.rstrip() for x in sorted_lines],
            ):
                if dline.startswith('-'):
                    dline = _COLOR.red(dline)
                elif dline.startswith('+'):
                    dline = _COLOR.green(dline)
                _LOG.info(dline)
            _LOG.info('  %s', block.end_line.rstrip())

        return sorted_lines

    def _parse_file(self, ins):
        block: Optional[_Block] = None

        for i, line in enumerate(ins, start=1):
            if block:
                if _START.search(line):
                    raise KeepSortedParsingError(
                        f'found {line.strip()!r} inside keep-sorted block',
                        self.path, i)

                if _END.search(line):
                    _LOG.debug('Found end line %d %r', i, line)
                    block.end_line = line
                    self.all_lines.extend(self._process_block(block))
                    block = None
                    self.all_lines.append(line)

                else:
                    _LOG.debug('Adding to block line %d %r', i, line)
                    block.lines.append(line)

            elif start_match := _START.search(line):
                _LOG.debug('Found start line %d %r', i, line)

                block = _Block()

                block.ignore_case = bool(_IGNORE_CASE.search(line))
                _LOG.debug('ignore_case: %s', block.ignore_case)

                block.allow_dupes = bool(_ALLOW_DUPES.search(line))
                _LOG.debug('allow_dupes: %s', block.allow_dupes)

                match = _IGNORE_PREFIX.search(line)
                if match:
                    block.ignored_prefixes = match.group(1).split(',')

                    # We want to check the longest prefixes first, in case one
                    # prefix is a prefix of another prefix.
                    block.ignored_prefixes.sort(key=lambda x: (-len(x), x))
                _LOG.debug('ignored_prefixes: %r', block.ignored_prefixes)

                block.start_line = line
                block.start_line_number = i
                self.all_lines.append(line)

                remaining = line[start_match.end():].strip()
                remaining = _IGNORE_CASE.sub('', remaining, count=1).strip()
                remaining = _ALLOW_DUPES.sub('', remaining, count=1).strip()
                remaining = _IGNORE_PREFIX.sub('', remaining, count=1).strip()
                if remaining.strip():
                    raise KeepSortedParsingError(
                        f'unrecognized directive on keep-sorted line: '
                        f'{remaining}', self.path, i)

            elif _END.search(line):
                raise KeepSortedParsingError(
                    f'found {line.strip()!r} outside keep-sorted block',
                    self.path, i)

            else:
                self.all_lines.append(line)

        if block:
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

    def write(self, path: Optional[Path] = None) -> None:
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


@presubmit.check(name='keep_sorted')
def presubmit_check(ctx: presubmit.PresubmitContext) -> None:
    """Presubmit check that ensures specified lists remain sorted."""

    changed_paths = _process_files(ctx)

    if changed_paths:
        _print_howto_fix(changed_paths)


# TODO(b/251659424) Remove.
keep_sorted = presubmit_check


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
