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

"""
The Pigweed command line interface (CLI)

Example uses:
    pw watch     Watch for changes and re-build
    pw logdemo   Show log examples
"""

import argparse
import sys
import logging

import pw_cmd.log

# TODO(keir): Make this a plugin mechanism that searches for plugins instead.
from pw_cmd.watch import WatchCommand
from pw_cmd.color import Color

_LOG = logging.getLogger(__name__)

_PIGWEED_BANNER = '''
 ▒█████▄   █▓  ▄███▒  ▒█    ▒█ ░▓████▒ ░▓████▒ ▒▓████▄
  ▒█░  █░ ░█▒ ██▒ ▀█▒ ▒█░ █ ▒█  ▒█   ▀  ▒█   ▀  ▒█  ▀█▌
  ▒█▄▄▄█░ ░█▒ █▓░ ▄▄░ ▒█░ █ ▒█  ▒███    ▒███    ░█   █▌
  ▒█▀     ░█░ ▓█   █▓ ░█░ █ ▒█  ▒█   ▄  ▒█   ▄  ░█  ▄█▌
  ▒█      ░█░ ░▓███▀   ▒█▓▀▓█░ ░▓████▒ ░▓████▒ ▒▓████▀
'''

class HelpCommand:
    """Default command to print help"""
    def __init__(self, parser):
        self.parser = parser

    def name(self):
        return 'help'

    def help(self):
        return 'Show the Pigweed CLI help'

    def register(self, parser):
        pass

    def run(self):
        self.parser.print_help()

def main(raw_args=None):
    if raw_args is None:
        raw_args = sys.argv[1:]

    pw_cmd.log.install()

    # Start with the most critical part of the Pigweed command line tool.
    print(Color.magenta(_PIGWEED_BANNER))

    # Add commands and their parsers.
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)

    help_command = HelpCommand(parser)

    # TODO(keir): Add proper subcommand discovery mechanism.
    commands = [
        help_command,
        WatchCommand(),
        pw_cmd.log.LogDemoCommand(),
    ]

    # Setting this default on the top-level parser makes 'pw' show help by
    # default when invoked with no arguments.
    parser.set_defaults(command=help_command)

    subparsers = parser.add_subparsers(help='pw subcommand to run')
    for command in commands:
        subparser = subparsers.add_parser(command.name(), help=command.help())
        command.register(subparser)
        subparser.set_defaults(command=command)

    args = parser.parse_args(raw_args)

    args_as_dict = dict(vars(args))
    del args_as_dict['command']
    args.command.run(**args_as_dict)


if __name__ == "__main__":
    main()
