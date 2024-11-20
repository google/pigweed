# Copyright 2020 The Pigweed Authors
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
"""The Pigweed command line interface."""

import logging
import os
import sys
from typing import NoReturn

from pw_cli_analytics import analytics as cli_analytics
import pw_cli.env
import pw_cli.log
from pw_cli import arguments, plugins, pw_command_plugins

_LOG = logging.getLogger(__name__)


def main() -> NoReturn:
    """Entry point for the pw command."""

    argv_copy = sys.argv[:]
    args = arguments.parse_args()

    pw_cli.log.install(level=args.loglevel, debug_log=args.debug_log)

    # Print the banner unless --no-banner or a tab completion arg is
    # present.
    # Note: args.tab_complete_{command,option} may be the empty string
    # '' so check for None instead.
    if (
        args.banner
        and args.tab_complete_option is None
        and args.tab_complete_command is None
    ):
        arguments.print_banner()

    _LOG.debug('Executing the pw command from %s', args.directory)
    os.chdir(args.directory)

    pw_command_plugins.register()

    if args.tab_complete_option is not None:
        arguments.print_completions_for_option(
            arguments.arg_parser(),
            text=args.tab_complete_option,
            tab_completion_format=args.tab_complete_format,
        )
        sys.exit(0)

    if args.tab_complete_command is not None:
        for name, plugin in sorted(pw_command_plugins.plugin_registry.items()):
            if name.startswith(args.tab_complete_command):
                if args.tab_complete_format in ('fish', 'zsh'):
                    print(':'.join([name, plugin.help()]))
                else:
                    print(name)
        sys.exit(0)

    if args.help or args.command is None:
        print(pw_command_plugins.format_help(), file=sys.stderr)
        sys.exit(0)

    if args.analytics is None:
        args.analytics = True

    env = pw_cli.env.pigweed_environment()
    if env.PW_DISABLE_CLI_ANALYTICS:
        args.analytics = False

    analytics: cli_analytics.Analytics | None = None
    analytics_state = cli_analytics.State.UNKNOWN
    if args.analytics and args.command != 'cli-analytics':
        # If there are no analytics settings we need to initialize them. Don't
        # send telemetry out on the initial run.
        analytics_state = cli_analytics.initialize()
        if analytics_state != cli_analytics.State.NEWLY_INITIALIZED:
            analytics = cli_analytics.Analytics(argv_copy, args)
            try:
                analytics.begin()
            except Exception:  # pylint: disable=broad-except
                analytics = None

    status = -1
    try:
        status = pw_command_plugins.run(args.command, args.plugin_args)
    except (plugins.Error, KeyError) as err:
        _LOG.critical('Cannot run command %s.', args.command)
        _LOG.critical('%s', err)
        status = 2
    finally:
        try:
            if analytics:
                analytics.end(status=status)
            cli_analytics.finalize()
        except Exception:  # pylint: disable=broad-except
            pass

    sys.exit(status)


if __name__ == '__main__':
    main()
