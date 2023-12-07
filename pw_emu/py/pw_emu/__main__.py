# Copyright 2023 The Pigweed Authors
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
"""Command line interface for the Pigweed emulators frontend"""

import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import threading

from typing import Any

from pw_emu.core import Error
from pw_emu.frontend import Emulator
from serial import serial_for_url, SerialException
from serial.tools.miniterm import Miniterm, key_description

_TERM_CMD = ['python', '-m', 'serial', '--raw']


def _cmd_gdb_cmds(emu, args: argparse.Namespace) -> None:
    """Run ``gdb`` commands in batch mode."""

    emu.run_gdb_cmds(args.gdb_cmd, executable=args.executable, pause=args.pause)


def _cmd_load(emu: Emulator, args: argparse.Namespace) -> None:
    """Load an executable image via ``gdb`` and start executing it if
    ``--pause`` is not set"""

    args.gdb_cmd = ['load']
    _cmd_gdb_cmds(emu, args)


def _cmd_start(emu: Emulator, args: argparse.Namespace) -> None:
    """Launch the emulator and start executing, unless ``--pause`` is set."""

    if args.runner:
        emu.set_emu(args.runner)

    emu.start(
        target=args.target,
        file=args.file,
        pause=args.pause,
        args=args.args,
        debug=args.debug,
        foreground=args.foreground,
    )


def _get_miniterm(emu: Emulator, chan: str) -> Miniterm:
    chan_type = emu.get_channel_type(chan)
    if chan_type == 'tcp':
        host, port = emu.get_channel_addr(chan)
        url = f'socket://[{host}]:{port}'
    elif chan_type == 'pty':
        url = emu.get_channel_path(chan)
    else:
        raise Error(f'unknown channel type `{chan_type}`')
    ser = serial_for_url(url)
    ser.timeout = 1
    miniterm = Miniterm(ser)
    miniterm.raw = True
    miniterm.set_tx_encoding('UTF-8')
    miniterm.set_rx_encoding('UTF-8')

    quit_key = key_description(miniterm.exit_character)
    menu_key = key_description(miniterm.menu_character)
    help_key = key_description('\x08')
    help_desc = f'Help: {menu_key} followed by {help_key} ---'

    print(f'--- Miniterm on {chan} ---')
    print(f'--- Quit: {quit_key} | Menu: {menu_key} | {help_desc}')

    # On POSIX systems miniterm uses TIOCSTI to "cancel" the TX thread
    # (reading from the console, sending to the serial) which is
    # disabled on Linux kernels > 6.2 see
    # https://github.com/pyserial/pyserial/issues/243
    #
    # On Windows the cancel method does not seem to work either with
    # recent win10 versions.
    #
    # Workaround by terminating the process for exceptions in the read
    # and write threads.
    threading.excepthook = lambda args: signal.raise_signal(signal.SIGTERM)

    return miniterm


def _cmd_run(emu: Emulator, args: argparse.Namespace) -> None:
    """Start the emulator and connect the terminal to a channel. Stop
    the emulator when exiting the terminal."""

    emu.start(
        target=args.target,
        file=args.file,
        pause=True,
        args=args.args,
    )

    ctrl_chans = ['gdb', 'monitor', 'qmp', 'robot']
    if not args.channel:
        for chan in emu.get_channels():
            if chan not in ctrl_chans:
                args.channel = chan
                break
    if not args.channel:
        raise Error(f'only control channels {ctrl_chans} found')

    try:
        miniterm = _get_miniterm(emu, args.channel)
        emu.cont()
        miniterm.start()
        miniterm.join(True)
        print('--- exit ---')
        miniterm.stop()
        miniterm.join()
        miniterm.close()
    except SerialException as err:
        raise Error(f'error connecting to channel `{args.channel}`: {err}')
    finally:
        emu.stop()


def _cmd_restart(emu: Emulator, args: argparse.Namespace) -> None:
    """Restart the emulator and start executing, unless ``--pause`` is set."""

    if emu.running():
        emu.stop()
    _cmd_start(emu, args)


def _cmd_stop(emu: Emulator, _args: argparse.Namespace) -> None:
    """Stop the emulator."""

    emu.stop()


def _cmd_reset(emu: Emulator, _args: argparse.Namespace) -> None:
    """Perform a software reset."""

    emu.reset()


def _cmd_gdb(emu: Emulator, args: argparse.Namespace) -> None:
    """Start a ``gdb`` interactive session."""

    executable = args.executable if args.executable else ""

    signal.signal(signal.SIGINT, signal.SIG_IGN)
    try:
        cmd = emu.get_gdb_cmd() + [
            '-ex',
            f'target remote {emu.get_gdb_remote()}',
            executable,
        ]
        subprocess.run(cmd)
    finally:
        signal.signal(signal.SIGINT, signal.SIG_DFL)


def _cmd_prop_ls(emu: Emulator, args: argparse.Namespace) -> None:
    """List emulator object properties."""

    props = emu.list_properties(args.path)
    print(json.dumps(props, indent=4))


def _cmd_prop_get(emu: Emulator, args: argparse.Namespace) -> None:
    """Show the emulator's object properties."""

    print(emu.get_property(args.path, args.property))


def _cmd_prop_set(emu: Emulator, args: argparse.Namespace) -> None:
    """Set emulator's object properties."""

    emu.set_property(args.path, args.property, args.value)


def _cmd_term(emu: Emulator, args: argparse.Namespace) -> None:
    """Connect with an interactive terminal to an emulator channel"""

    try:
        miniterm = _get_miniterm(emu, args.channel)
        miniterm.start()
        miniterm.join(True)
        print('--- exit ---')
        miniterm.stop()
        miniterm.join()
        miniterm.close()
    except SerialException as err:
        raise Error(f'error connecting to channel `{args.channel}`: {err}')


def _cmd_resume(emu: Emulator, _args: argparse.Namespace) -> None:
    """Resume the execution of a paused emulator."""

    emu.cont()


def get_parser() -> argparse.ArgumentParser:
    """Command line parser"""

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        '-i',
        '--instance',
        help=(
            'Run multiple instances simultaneously by assigning each instance '
            'an ID (default: ``%(default)s``)'
        ),
        type=str,
        metavar='STRING',
        default='default',
    )
    parser.add_argument(
        '-C',
        '--working-dir',
        help=(
            'Absolute path to the working directory '
            '(default: ``%(default)s``)'
        ),
        type=Path,
        default=os.getenv('PW_EMU_WDIR'),
    )
    parser.add_argument(
        '-c',
        '--config',
        help='Absolute path to config file (default: ``%(default)s``)',
        type=str,
        default=None,
    )

    subparsers = parser.add_subparsers(dest='command', required=True)

    def add_cmd(name: str, func: Any) -> argparse.ArgumentParser:
        subparser = subparsers.add_parser(
            name, description=func.__doc__, help=func.__doc__
        )
        subparser.set_defaults(func=func)
        return subparser

    start = add_cmd('start', _cmd_start)
    restart = add_cmd('restart', _cmd_restart)

    for subparser in [start, restart]:
        subparser.add_argument(
            'target',
            type=str,
        )
        subparser.add_argument(
            '--file',
            '-f',
            metavar='FILE',
            help='File to load before starting',
        )
        subparser.add_argument(
            '--runner',
            '-r',
            help='The emulator to use (automatically detected if not set)',
            choices=[None, 'qemu', 'renode'],
            default=None,
        )
        subparser.add_argument(
            '--args',
            '-a',
            help='Options to pass to the emulator',
        )
        subparser.add_argument(
            '--pause',
            '-p',
            action='store_true',
            help='Pause the emulator after starting it',
        )
        subparser.add_argument(
            '--debug',
            '-d',
            action='store_true',
            help='Start the emulator in debug mode',
        )
        subparser.add_argument(
            '--foreground',
            '-F',
            action='store_true',
            help='Start the emulator in foreground mode',
        )

    run = add_cmd('run', _cmd_run)
    run.add_argument(
        'target',
        type=str,
    )
    run.add_argument(
        'file',
        metavar='FILE',
        help='File to load before starting',
    )
    run.add_argument(
        '--args',
        '-a',
        help='Options to pass to the emulator',
    )
    run.add_argument(
        '--channel',
        '-n',
        help='Channel to connect the terminal to',
    )

    stop = add_cmd('stop', _cmd_stop)

    load = add_cmd('load', _cmd_load)
    load.add_argument(
        'executable',
        metavar='FILE',
        help='File to load via ``gdb``',
    )
    load.add_argument(
        '--pause',
        '-p',
        help='Pause the emulator after loading the file',
        action='store_true',
    )
    load.add_argument(
        '--offset',
        '-o',
        metavar='ADDRESS',
        help='Address to load the file at',
    )

    reset = add_cmd('reset', _cmd_reset)

    gdb = add_cmd('gdb', _cmd_gdb)
    gdb.add_argument(
        '--executable',
        '-e',
        metavar='FILE',
        help='File to use for the debugging session',
    )

    prop_ls = add_cmd('prop-ls', _cmd_prop_ls)
    prop_ls.add_argument(
        'path',
        help='Absolute path to the emulator object',
    )

    prop_get = add_cmd('prop-get', _cmd_prop_get)
    prop_get.add_argument(
        'path',
        help='Absolute path to the emulator object',
    )
    prop_get.add_argument(
        'property',
        help='Name of the object property',
    )

    prop_set = add_cmd('prop-set', _cmd_prop_set)
    prop_set.add_argument(
        'path',
        help='Absolute path to the emulator object',
    )
    prop_set.add_argument(
        'property',
        help='Name of the object property',
    )
    prop_set.add_argument(
        'value',
        help='Value to set for the object property',
    )

    gdb_cmds = add_cmd('gdb-cmds', _cmd_gdb_cmds)
    gdb_cmds.add_argument(
        '--pause',
        '-p',
        help='Do not resume execution after running the commands',
        action='store_true',
    )
    gdb_cmds.add_argument(
        '--executable',
        '-e',
        metavar='FILE',
        help='Executable to use while running ``gdb`` commands',
    )
    gdb_cmds.add_argument(
        'gdb_cmd',
        nargs='+',
        help='``gdb`` command to execute',
    )

    term = add_cmd('term', _cmd_term)
    term.add_argument(
        'channel',
        help='Channel name',
    )

    resume = add_cmd('resume', _cmd_resume)

    parser.epilog = f"""commands usage:
        {start.format_usage().strip()}
        {restart.format_usage().strip()}
        {stop.format_usage().strip()}
        {run.format_usage().strip()}
        {load.format_usage().strip()}
        {reset.format_usage().strip()}
        {gdb.format_usage().strip()}
        {prop_ls.format_usage().strip()}
        {prop_get.format_usage().strip()}
        {prop_set.format_usage().strip()}
        {gdb_cmds.format_usage().strip()}
        {term.format_usage().strip()}
        {resume.format_usage().strip()}
    """

    return parser


def main() -> int:
    """Emulators frontend command line interface."""

    args = get_parser().parse_args()
    if not args.working_dir:
        args.working_dir = (
            f'{os.getenv("PW_PROJECT_ROOT")}/.pw_emu/{args.instance}'
        )

    try:
        emu = Emulator(args.working_dir, args.config)
        args.func(emu, args)
    except Error as err:
        print(err)
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
