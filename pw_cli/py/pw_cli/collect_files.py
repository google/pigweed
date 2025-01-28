# Copyright 2024 The Pigweed Authors
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
"""Utilities for file collection in a repository."""

import argparse
from collections import Counter, defaultdict
import logging
import os
from pathlib import Path
import re
from typing import Any, Collection, Iterable, Pattern, Sequence

from pw_cli.tool_runner import ToolRunner
from pw_cli.file_filter import exclude_paths
from pw_cli.git_repo import (
    describe_git_pattern,
    find_git_repo,
    GitError,
    TRACKING_BRANCH_ALIAS,
)
from pw_cli.plural import plural


_LOG = logging.getLogger(__name__)


def add_file_collection_arguments(parser: argparse.ArgumentParser) -> None:
    """Adds arguments required by ``collect_files()``."""

    parser.add_argument(
        'paths',
        metavar='pathspec',
        nargs='*',
        help=(
            'Paths or patterns to which to restrict the checks. These are '
            'interpreted as Git pathspecs. If --base is provided, only '
            'paths changed since that commit are checked.'
        ),
    )

    base = parser.add_mutually_exclusive_group()
    base.add_argument(
        '-b',
        '--base',
        metavar='commit',
        default=TRACKING_BRANCH_ALIAS,
        help=(
            'Git revision against which to diff for changed files. '
            'Default is the tracking branch of the current branch: '
            f'{TRACKING_BRANCH_ALIAS}'
        ),
    )

    base.add_argument(
        '--all',
        '--full',
        dest='base',
        action='store_const',
        const=None,
        help='Run actions for all files, not just changed files.',
    )

    parser.add_argument(
        '-e',
        '--exclude',
        metavar='regular_expression',
        default=[],
        action='append',
        type=re.compile,  # type: ignore[arg-type]
        help=(
            'Exclude paths matching any of these regular expressions, '
            "which are interpreted relative to each Git repository's root."
        ),
    )


def collect_files_in_current_repo(
    pathspecs: Collection[Path | str],
    tool_runner: ToolRunner,
    modified_since_git_ref: str | None = None,
    exclude_patterns: Collection[Pattern] = tuple(),
    action_flavor_text: str = 'Collecting',
) -> Sequence[Path]:
    """Collects files given a variety of pathspecs and maps them to their repo.

    This is a relatively fuzzy file finder for projects tracked in a Git repo.
    It's designed to adhere to the following constraints:

      * If a pathspec is a real file, unconditionally return it.
      * If no pathspecs are passed, collect from the current working directory.
      * Return the path of any files modified since `modified_since_git_ref`
        (which may be a branch, tag, or commit) that match the provided
        pathspecs.
      * Passing no pathspecs has the same behavior as passing `.` (everything in
        the current directory).

    Args:
        pathspecs: Files or git pathspecs to collect files from. Wildcards (e.g.
            `pw_cl*`) are accepted.
        tool_runner: The ToolRunner to use for Git operations.
        modified_since_git_ref: If the passed pathspec is tracked by a git repo,
            it is excluded if unmodified since the specified pathspec. If the
            pathspec is `None`, no files are excluded.
        exclude_patterns: A collection of exclude patterns to exclude from the
            set of collected files.
        action_flavor_text: Replaces "Collecting" in the
            "Collecting all files in the foo repo" log message with a
            personalized string (e.g. "Formatting all files...").

    Returns:
        A dictionary mapping a GitRepo to a list of paths relative to that
        repo's root that match the provided pathspecs. Files not tracked by
        any git repo are mapped to the `None` key.
    """
    # TODO: https://pwbug.dev/391690594 - This is brittle and not covered by
    # tests. Someday it should be re-thought, particularly to better handle
    # multi-repo setups.
    files = [Path(path).resolve() for path in pathspecs if Path(path).is_file()]
    try:
        current_repo = find_git_repo(Path.cwd(), tool_runner)
    except GitError:
        current_repo = None

    # If this is a Git repo, list the original paths with git ls-files or diff.
    if current_repo is not None:
        # Implement a graceful fallback in case the tracking branch isn't
        # available.
        if (
            modified_since_git_ref == TRACKING_BRANCH_ALIAS
            and not current_repo.tracking_branch()
        ):
            _LOG.warning(
                'Failed to determine the tracking branch, using --base HEAD~1 '
                'instead of listing all files'
            )
            modified_since_git_ref = 'HEAD~1'

        _LOG.info(
            '%s %s',
            action_flavor_text,
            describe_git_pattern(
                Path.cwd(),
                modified_since_git_ref,
                pathspecs,
                exclude_patterns,
                tool_runner,
                current_repo.root(),
            ),
        )

        # Add files from Git and remove duplicates.
        files = sorted(
            set(
                exclude_paths(
                    exclude_patterns,
                    current_repo.list_files(modified_since_git_ref, pathspecs),
                )
            )
            | set(files)
        )
    elif modified_since_git_ref:
        _LOG.critical(
            'A base commit may only be provided if running from a Git repo'
        )

    return files


def file_summary(
    paths: Iterable[Path],
    levels: int = 2,
    max_lines: int = 12,
    max_types: int = 3,
    pad: str = ' ',
    pad_start: str = ' ',
    pad_end: str = ' ',
) -> list[str]:
    """Summarizes a list of files by the file types in each directory."""

    # Count the file types in each directory.
    all_counts: dict[Any, Counter] = defaultdict(Counter)

    for path in paths:
        parent = path.parents[max(len(path.parents) - levels, 0)]
        all_counts[parent][path.suffix] += 1

    # If there are too many lines, condense directories with the fewest files.
    if len(all_counts) > max_lines:
        counts = sorted(
            all_counts.items(), key=lambda item: -sum(item[1].values())
        )
        counts, others = (
            sorted(counts[: max_lines - 1]),
            counts[max_lines - 1 :],
        )
        counts.append(
            (
                f'({plural(others, "other")})',
                sum((c for _, c in others), Counter()),
            )
        )
    else:
        counts = sorted(all_counts.items())

    width = max(len(str(d)) + len(os.sep) for d, _ in counts) if counts else 0
    width += len(pad_start)

    # Prepare the output.
    output = []
    for path, files in counts:
        total = sum(files.values())
        del files['']  # Never display no-extension files individually.

        if files:
            extensions = files.most_common(max_types)
            other_extensions = total - sum(count for _, count in extensions)
            if other_extensions:
                extensions.append(('other', other_extensions))

            types = ' (' + ', '.join(f'{c} {e}' for e, c in extensions) + ')'
        else:
            types = ''

        root = f'{path}{os.sep}{pad_start}'.ljust(width, pad)
        output.append(f'{root}{pad_end}{plural(total, "file")}{types}')

    return output
