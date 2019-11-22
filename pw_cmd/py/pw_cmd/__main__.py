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
    pw watch --build_dir out/clang
    pw logdemo
"""

import argparse
import sys
import logging
import importlib
import pkgutil

import pw_cmd.log
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

    # TODO(keir): Add support for configurable logging levels.
    pw_cmd.log.install()

    # Start with the most critical part of the Pigweed command line tool.
    print(Color.magenta(_PIGWEED_BANNER))

    # Add commands and their parsers.
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)

    help_command = HelpCommand(parser)

    # Note: The help command is special since it accesses parser.print_help().
    commands = [
        help_command,
    ]

    # Find and load command line plugins.
    #
    # Plugins are located by finding modules starting with "pw_", loading that
    # module, then searching for an iterable named 'PW_CLI_PLUGINS' containing
    # Command classes. The classes are instantiated and added to the commands.
    #
    # Note: We may want to make plugin loading explicit rather than doing this
    # via search, since it slows down starting 'pw' considerably.
    for module in pkgutil.iter_modules():
        if module.name.startswith('pw_'):
            plugin = importlib.__import__(module.name)
            _LOG.debug('Found module that may have plugins: %s', module.name)
            if hasattr(plugin, 'PW_CLI_PLUGINS'):
                for command_class in plugin.PW_CLI_PLUGINS:
                    command_instance = command_class()
                    commands.append(command_instance)
                    _LOG.debug('Found plugins: %s', command_instance.name())

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
