#!/usr/bin/env python
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
"""Pigweed Zephyr argparser."""
import argparse


def add_parser_arguments(
    parser: argparse.ArgumentParser,
) -> argparse.ArgumentParser:
    """Sets up an argument parser for pw zephyr."""
    parser.add_argument(
        '-v',
        '--verbose',
        dest='verbose',
        action='store_true',
        help="Enables debug logging",
    )

    subparsers = parser.add_subparsers(dest='zephyr_subcommand')
    subparsers.add_parser(
        'manifest',
        help=(
            'Prints the West manifest entries needed to add this Pigweed'
            ' instance'
        ),
    )

    return parser
