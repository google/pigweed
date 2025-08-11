#!/usr/bin/env python3
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
"""pw change command group."""
import argparse
import sys
from collections.abc import Sequence

from pw_change import comments, push, review


def _main(argv: Sequence[str]) -> int:
    """Parses arguments and dispatches to a pw change subcommand."""
    parser = argparse.ArgumentParser(prog='pw change', description=__doc__)
    subparsers = parser.add_subparsers(
        title='Subcommands', dest='subcommand', required=True
    )

    push_parser = subparsers.add_parser(
        'push', help='Upload the current change to Gerrit.'
    )
    push_parser.set_defaults(func=push.main)

    review_parser = subparsers.add_parser(
        'review', help='AI-based code review of the current change.'
    )
    review_parser.set_defaults(func=review.main)

    comments_parser = subparsers.add_parser(
        'comments', help='Get comments from a Gerrit change.'
    )
    comments_parser.set_defaults(func=comments.main)

    args, remaining_args = parser.parse_known_args(argv)
    return args.func(remaining_args)


def main() -> int:
    """Entry point for the `pw change` command."""
    return _main(sys.argv[1:])


if __name__ == '__main__':
    sys.exit(main())
