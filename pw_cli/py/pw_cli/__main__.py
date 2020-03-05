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
import asyncio
import sys
import logging
import importlib
import pkgutil

from pw_cli.color import colors
import pw_cli.log

_LOG = logging.getLogger(__name__)

_PIGWEED_BANNER = '''
 ▒█████▄   █▓  ▄███▒  ▒█    ▒█ ░▓████▒ ░▓████▒ ▒▓████▄
  ▒█░  █░ ░█▒ ██▒ ▀█▒ ▒█░ █ ▒█  ▒█   ▀  ▒█   ▀  ▒█  ▀█▌
  ▒█▄▄▄█░ ░█▒ █▓░ ▄▄░ ▒█░ █ ▒█  ▒███    ▒███    ░█   █▌
  ▒█▀     ░█░ ▓█   █▓ ░█░ █ ▒█  ▒█   ▄  ▒█   ▄  ░█  ▄█▌
  ▒█      ░█░ ░▓███▀   ▒█▓▀▓█░ ░▓████▒ ░▓████▒ ▒▓████▀
'''


class ArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        print(colors().magenta(_PIGWEED_BANNER), file=sys.stderr)
        self.print_usage(sys.stderr)
        self.exit(2, '%s: error: %s\n' % (self.prog, message))


def main(raw_args=None):
    """Entry point for pw command."""

    if raw_args is None:
        raw_args = sys.argv[1:]

    # TODO(keir): Add support for configurable logging levels.
    pw_cli.log.install()

    # Add commands and their parsers.
    parser = ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)

    parser.add_argument('--loglevel', default='INFO')
    parser.add_argument('--no-banner',
                        action='store_true',
                        help="Don't print the Pigweed banner")

    # Default command is 'help'
    pw_cli.plugins.register(
        name='help',
        short_help='Show the Pigweed CLI help',
        command_function=parser.print_help,
    )
    # Setting this default on the top-level parser makes 'pw' show help by
    # default when invoked with no arguments.
    parser.set_defaults(_command=parser.print_help)
    parser.set_defaults(_run_async=False)

    # Find and load registered command line plugins.
    #
    # Plugins are located by finding modules starting with "pw_", and importing
    # them. On import, modules must call pw_cli.plugins.register(), which adds
    # that plugin to the registry.
    #
    # Note: We may want to make plugin loading explicit rather than doing this
    # via search, since the search slows down starting 'pw' considerably.
    for module in pkgutil.iter_modules():
        if module.name.startswith('pw_'):
            _LOG.debug('Found module that may have plugins: %s', module.name)
            unused_plugin = importlib.__import__(module.name)

    # Pull plugins out of the registry and set them up with the parser.
    subparsers = parser.add_subparsers(help='pw subcommand to run')
    for command in pw_cli.plugins.registry:
        subparser = subparsers.add_parser(command.name, help=command.help)
        command.define_args_function(subparser)
        subparser.set_defaults(_command=command.command_function)

        # Check whether the sub-command's entry point is asynchronous.
        subparser.set_defaults(
            _run_async=asyncio.iscoroutinefunction(command.command_function))

    args = parser.parse_args(raw_args)

    # Start with the most critical part of the Pigweed command line tool.
    if not args.no_banner:
        print(colors().magenta(_PIGWEED_BANNER))

    args_as_dict = dict(vars(args))
    del args_as_dict['_command']
    del args_as_dict['_run_async']

    # Set root log level; but then remove the arg to avoid breaking the command.
    if 'loglevel' in args_as_dict:
        pw_cli.log.set_level(getattr(logging,
                                     args_as_dict['loglevel'].upper()))
        del args_as_dict['loglevel']

    if 'no_banner' in args_as_dict:
        del args_as_dict['no_banner']

    # Run the command and exit with the appropriate status.
    # pylint: disable=protected-access
    if args._run_async:
        sys.exit(asyncio.run(args._command(**args_as_dict)))
    else:
        sys.exit(args._command(**args_as_dict))
    # pylint: enable=protected-access


if __name__ == "__main__":
    main()
