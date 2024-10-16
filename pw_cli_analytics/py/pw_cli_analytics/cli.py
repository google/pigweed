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
"""Pigweed Analytics entry point."""

import argparse
import json
import logging
import os.path
from pathlib import Path
import sys
from typing import Any, Literal, Sequence
import uuid

from . import config

_LOG = logging.getLogger(__package__)


def _build_argument_parser() -> argparse.ArgumentParser:
    """Setup argparse."""
    parser = argparse.ArgumentParser(
        prog="python -m pw_cli_analytics", description=__doc__
    )

    opt_group = parser.add_mutually_exclusive_group()
    opt_group.add_argument(
        '--opt-in',
        dest='opt',
        action='store_const',
        const='in',
        help='Opt-in to analytics collection.',
    )

    opt_group.add_argument(
        '--opt-out',
        dest='opt',
        action='store_const',
        const='out',
        help='Opt-out of analytics collection.',
    )

    return parser


def write(path: Path, unique_id: uuid.UUID | None) -> None:
    config_section_title = config.CONFIG_SECTION_TITLE

    cfg: dict[str, Any]
    try:
        with path.open() as ins:
            cfg = json.load(ins)
    except FileNotFoundError:
        cfg = {'config_title': '.'.join(config_section_title)}
    top = cfg

    if cfg['config_title'] == '.'.join(config_section_title):
        cfg['uuid'] = str(unique_id) if unique_id else unique_id

    else:
        for title in config_section_title:
            if title not in cfg:
                raise ValueError(f'expected {title} in {list(cfg.keys())}')
            cfg = cfg[title]
        cfg['uuid'] = str(unique_id)

    cfg['enabled'] = bool(cfg['uuid'])

    with path.open('w') as outs:
        json.dump(top, outs, sort_keys=True, indent=4)
        outs.write('\n')


def update(opt: Literal['in', 'out']) -> None:
    prefs = config.AnalyticsPrefs(
        project_file=None,
        project_user_file=None,
    )

    # If opting in, only rewrite the file if the uuid isn't set or enabled is
    # False. Successive opt-in calls should preserve the uuid.
    if opt == 'in':
        if not prefs['uuid'] or not prefs['enabled']:
            _LOG.info('enabling analytics')
            write(config.DEFAULT_USER_FILE, unique_id=uuid.uuid4())

    # If opting out, only rewrite the file if the uuid is set or enabled is
    # True. They should be consistent but if not we'll make them consistent.
    if opt == 'out':
        if prefs['uuid'] or prefs['enabled']:
            _LOG.info('disabling analytics')
            write(config.DEFAULT_USER_FILE, unique_id=None)


def summary() -> None:
    """Write configuration to the terminal."""
    prefs = config.AnalyticsPrefs()

    if prefs['uuid']:
        _LOG.info(
            "Analytics enabled, run 'pw cli-analytics --opt-out' to disable."
        )
    else:
        _LOG.info(
            "Analytics disabled, run 'pw cli-analytics --opt-in' to enable."
        )

    _LOG.info('See https://pigweed.dev/pw_cli_analytics/ for more details.')
    _LOG.info('')

    home = os.path.expanduser('~')

    _LOG.info('Per-project settings:')
    _LOG.info('    %s', str(config.DEFAULT_PROJECT_FILE).replace(home, '~'))
    _LOG.info('')

    _LOG.info('Per-user per-project settings:')
    _LOG.info(
        '    %s', str(config.DEFAULT_PROJECT_USER_FILE).replace(home, '~')
    )
    _LOG.info('')

    _LOG.info('Per-user settings:')
    _LOG.info('    %s', str(config.DEFAULT_USER_FILE).replace(home, '~'))
    _LOG.info('')

    for key in prefs:
        assert isinstance(key, str)
        _LOG.info('%s = %s', key, prefs[key])


def run(opt: Literal['in', 'out']) -> None:
    update(opt)
    summary()


def main(argv: Sequence[str] | None = None) -> int:
    """Interface to Pigweed analytics."""

    parser = _build_argument_parser()
    args = parser.parse_args(argv)

    run(**vars(args))

    return 0


if __name__ == '__main__':
    sys.exit(main())
