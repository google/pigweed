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
"""Simple command to push changes to Gerrit."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import re
import subprocess
import sys


def _default_at_google_com(x):
    if '@' in x:
        return x
    return f'{x}@google.com'


# This looks like URL-encoding but it's different in which characters need to be
# escaped and which do not.
def _escaped_string(x):
    for ch in '%^@.,~-+_:/!\'"':
        x = x.replace(ch, f'%{ord(ch):02x}')
    return x.replace(' ', '_')


def parse(argv=None):
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser()
    parser.add_argument('rev', nargs='?', default='HEAD')
    parser.add_argument('-d', '--dry-run', action='store_true')
    parser.add_argument('--force', action='store_true')
    parser.add_argument('-t', '--topic')
    parser.add_argument('--ready', action='store_true')
    parser.add_argument(
        '-q', '--commit-queue', action='append_const', const=True
    )
    parser.add_argument('-p', '--publish', action='store_true')

    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        '-r',
        '--reviewer',
        metavar='USERNAME',
        action='append',
        default=[],
        type=_default_at_google_com,
    )
    group.add_argument('-w', '--wip', action='store_true')

    parser.add_argument(
        '-c',
        '--cc',
        metavar='USERNAME',
        action='append',
        default=[],
        type=_default_at_google_com,
    )
    parser.add_argument('-l', '--label', action='append', default=[])
    parser.add_argument('--no-verify', action='store_true')
    parser.add_argument('-a', '--auto-submit', action='store_true')
    parser.add_argument('--hashtag')
    parser.add_argument('-m', '--message', type=_escaped_string)
    args = parser.parse_args(argv)

    if args.force:
        for name in 'topic wip ready reviewer'.split():
            value = getattr(args, name)
            if value:
                parser.error('--force cannot be used with --{0}'.format(name))

    if args.wip and args.ready:
        parser.error('--wip cannot be used with --ready')

    return args


def _remote_dest():
    remote_branch = (
        subprocess.run(
            ['git', 'rev-parse', '--abbrev-ref', 'HEAD'],
            capture_output=True,
        )
        .stdout.decode()
        .strip()
    )
    while '/' not in remote_branch:
        cmd = [
            'git',
            'rev-parse',
            '--abbrev-ref',
            '--symbolic-full-name',
            f'{remote_branch}@{{upstream}}',
        ]
        remote_branch = (
            subprocess.run(
                cmd,
                capture_output=True,
            )
            .stdout.decode()
            .strip()
        )

    remote, branch = remote_branch.split('/', 1)
    return remote, branch


def _auto_submit_label(host):
    return {
        'pigweed': 'Pigweed-Auto-Submit',
        'pigweed-internal': 'Pigweed-Auto-Submit',
        'fuchsia': 'Fuchsia-Auto-Submit',
        'fuchsia-internal': 'Fuchsia-Auto-Submit',
    }.get(host, 'Auto-Submit')


def push(argv=None):
    """Push changes to Gerrit."""
    args = parse(argv)
    remote, branch = _remote_dest()

    if not args.force:
        branch = f'refs/for/{branch}'

    options = []
    push_args = []

    if args.topic:
        options.append(f'topic={args.topic}')

    if args.wip:
        options.append('wip')
    elif args.ready or args.reviewer:
        options.append('ready')

    if args.commit_queue:
        options.append(f'l=Commit-Queue+{len(args.commit_queue)}')

    if args.publish:
        options.append('publish-comments')

    for reviewer in args.reviewer:
        options.append(f'r={reviewer}')

    for reviewer in args.cc:
        options.append(f'cc={reviewer}')

    for label in args.label:
        options.append(f'l={label}')

    if args.hashtag:
        options.append(f'hashtag={args.hashtag}')

    if args.message:
        options.append(f'message={args.message}')

    if args.auto_submit:
        remote_url = (
            subprocess.run(
                ['git', 'config', '--get', f'remote.{remote}.url'],
                capture_output=True,
            )
            .stdout.decode()
            .strip()
        )
        match = re.search(r'^\w+:/+(?P<name>[\w-]+)[./]', remote_url)
        name = match.group('name') if match else None
        options.append(f'l={_auto_submit_label(name)}')

    if args.no_verify:
        push_args.append('--no-verify')

    options = ','.join(options)
    branch = f'{branch}%{options}'

    cmd = ['git', 'push', remote, f'+{args.rev}:{branch}']
    cmd.extend(push_args)
    print(*cmd)

    if args.dry_run:
        print('dry run, not pushing')
    else:
        subprocess.check_call(cmd)
    return 0


if __name__ == '__main__':
    sys.exit(push())
