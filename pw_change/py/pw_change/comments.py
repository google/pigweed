#!/usr/bin/python3
# Copyright 2025 The Pigweed Authors
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
"""Get comments from a Gerrit change."""

from __future__ import annotations

import argparse
import collections
import dataclasses
import functools
import json
import logging
import re
import shutil
import subprocess
import sys
import urllib.parse
from collections.abc import Sequence
from pathlib import Path
from typing import Any

import pw_change.remote_dest

_LOG = logging.getLogger(__name__)

GERRIT_JSON_PREFIX = ")]}'"


def _get_gerrit_host() -> str:
    """Get the Gerrit host from the git remote."""
    remote, _ = pw_change.remote_dest.remote_dest()
    remote_url = (
        subprocess.run(
            ['git', 'config', '--get', f'remote.{remote}.url'],
            capture_output=True,
            check=True,
        )
        .stdout.decode()
        .strip()
    )

    host = urllib.parse.urlparse(remote_url).netloc
    host = host.removesuffix('.git.corp.google.com')
    if '.' not in host:
        host = f'{host}.googlesource.com'
    if host:
        return host.replace('.', '-review.', 1)
    raise ValueError(f'Could not extract Gerrit host from {remote_url}')


@functools.cache
def _curl_executable() -> Path:
    curl = shutil.which('gob-curl') or shutil.which('curl')
    if not curl:
        raise FileNotFoundError('Neither gob-curl nor curl found in PATH')
    return Path(curl)


def _gerrit_json_query(url: str) -> Any:
    cmd: list[Path | str] = [_curl_executable(), '-s', url]
    result = subprocess.run(cmd, capture_output=True, check=True, text=True)
    return json.loads(result.stdout.removeprefix(GERRIT_JSON_PREFIX))


def fetch_comments(change_id: str) -> dict:
    """Fetch comments from a Gerrit change."""
    host = _get_gerrit_host()
    change = _gerrit_json_query(f'https://{host}/changes/{change_id}/detail')
    return _gerrit_json_query(
        f'https://{host}/changes/{change["_number"]}/comments',
    )


@dataclasses.dataclass
class CommentThread:
    comments: list[dict] = dataclasses.field(default_factory=list)
    resolved: bool = True


def thread_comments(all_comments: dict) -> dict[str, list[CommentThread]]:
    result: dict[str, list[CommentThread]] = {}
    for file, comments in all_comments.items():
        threads: list[CommentThread] = []
        threads_by_id: dict[str, CommentThread] = {}
        for comment in comments:
            if parent := comment.get('in_reply_to'):
                thread = threads_by_id[parent]
            else:
                thread = CommentThread()
                threads.append(thread)
            thread.comments.append(comment)
            thread.resolved = not comment['unresolved']
            threads_by_id[comment['id']] = thread

        result[file] = threads

    return result


def _comment_prefix(lines: Sequence[str], *, fallback: str = '#') -> str:
    """Extract the comment prefix used by the given sequence of lines.

    Look at the first five non-empty lines. If at least three of them start with
    the same character(s), assume that is the comment prefix for the file.
    """
    count: dict[str, int] = {}
    total: int = 0
    for line in lines:
        if not line.strip():
            continue
        prefix = line.split()[0]
        count.setdefault(prefix, 0)
        count[prefix] += 1
        total += 1
        if total >= 5:
            break

    for prefix, occurrences in count.items():
        if occurrences >= 3:
            return f'{prefix} '

    return f'{fallback.rstrip()} '


def _inline_threads(
    *,
    threads: dict[str, list[CommentThread]],
    unresolved_only: bool,
) -> None:
    for file, file_threads in threads.items():
        if file == '/PATCHSET_LEVEL':
            continue

        path = Path(file)
        if not path.exists():
            print(f'File not found: {file}', file=sys.stderr)
            continue

        lines = path.read_text().splitlines()
        comment_prefix = _comment_prefix(lines)

        line_threads = collections.defaultdict(list)

        for thread in file_threads:
            if thread.resolved and unresolved_only:
                continue
            line_threads[thread.comments[0]['line']].append(thread)

        num_comments = num_threads = 0

        for line_num in sorted(line_threads.keys(), reverse=True):
            inserted_lines = ['START INLINE COMMENT']
            thread_blocks = []
            for thread in line_threads[line_num]:
                comment_blocks = []
                for comment in thread.comments:
                    resolved = not comment.get("unresolved")
                    author = comment.get('author', {}).get('name', 'Unknown')
                    comment_blocks.append(
                        f'Author: {author}\n'
                        f'{comment["message"]}\n'
                        f'{"Resolved" if resolved else "Unresolved"}',
                    )
                    num_comments += 1
                thread_blocks.append('\n--------\n'.join(comment_blocks))
                num_threads += 1
            inserted_lines.append('\n========\n'.join(thread_blocks))
            inserted_lines.append('END INLINE COMMENT')

            # Some lines have embedded newlines. Change those into multiple
            # entries in the list.
            flattened_lines: list[str] = []
            for orig_line in inserted_lines:
                for line in orig_line.split('\n'):
                    flattened_lines.append(line)
            lines.insert(
                line_num,
                '\n'.join(
                    f'{comment_prefix}{line}' for line in flattened_lines
                ),
            )

        if num_comments:
            path.write_text('\n'.join(lines) + '\n')
            _LOG.info(
                'Inserted %d comments in %d threads in %s',
                num_comments,
                num_threads,
                path,
            )


def parse(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        'change_id',
        nargs='?',
        help='The Gerrit change ID. If not provided, it is extracted from the '
        'HEAD commit.',
    )
    parser.add_argument(
        '--all',
        '-a',
        dest='unresolved_only',
        action='store_false',
        help='Show all comments.',
    )
    parser.add_argument(
        '--inline',
        action='store_true',
        help='Insert comments into files.',
    )
    parser.add_argument(
        '--json',
        dest='json_',
        action='store_true',
        help='Outputs raw json comments as-is.',
    )
    return parser.parse_args(argv)


def get_comments(
    change_id: str,
    unresolved_only: bool = True,
    inline: bool = False,
    json_: bool = False,
) -> int:
    """Retrieve comments from Gerrit and show or inline them."""
    if not change_id:
        commit_msg = subprocess.run(
            ['git', 'log', '-1', '--pretty=%B'],
            capture_output=True,
            check=True,
            text=True,
        ).stdout
        match = re.search(r'Change-Id: (I[a-f0-9]{40})', commit_msg)
        if not match:
            print('Could not find Change-Id in HEAD commit.', file=sys.stderr)
            return 1
        change_id = match.group(1)

    comments = fetch_comments(change_id)
    if json_:
        print(json.dumps(comments, indent=2))
        return 0

    threads = thread_comments(comments)

    if inline:
        _inline_threads(threads=threads, unresolved_only=unresolved_only)
        return 0

    output_threads = []
    for file, file_threads in threads.items():
        for thread in file_threads:
            if unresolved_only and thread.resolved:
                continue

            thread_lines = []

            for comment in thread.comments:
                line = comment.get('line', 'N/A')
                author = comment.get('author', {}).get('name', 'Unknown')
                message = comment['message']
                resolved = '❌' if comment['unresolved'] else '✅'
                thread_lines.append(
                    f'{resolved} {file}:{line} - {author}: {message}',
                )

            output_threads.append('\n'.join(thread_lines))

    print('\n--------\n'.join(output_threads))

    return 0


def main(argv: Sequence[str] | None = None) -> int:
    return get_comments(**vars(parse(argv)))


if __name__ == '__main__':
    sys.exit(main())
