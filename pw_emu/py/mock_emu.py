#!/usr/bin/env python
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
"""Mock emulator used for testing process and channel management."""

import argparse
import os
import socket
import sys
import time

from threading import Thread


def _tcp_thread(sock: socket.socket) -> None:
    sock.listen()
    conn, _ = sock.accept()
    while True:
        data = conn.recv(1)
        conn.send(data)


def _pty_thread(fd: int) -> None:
    while True:
        data = os.read(fd, 1)
        os.write(fd, data)


def _get_parser() -> argparse.ArgumentParser:
    """Command line parser."""

    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-C', '--working-dir', metavar='PATH', help='working directory'
    )
    parser.add_argument(
        'echo', metavar='STRING', nargs='*', help='write STRING to stdout'
    )
    parser.add_argument(
        '--tcp-channel',
        action='append',
        default=[],
        metavar='NAME',
        help='listen for TCP connections, write port WDIR/NAME',
    )
    if sys.platform != 'win32':
        parser.add_argument(
            '--pty-channel',
            action='append',
            default=[],
            metavar='NAME',
            help='create pty channel and link in WDIR/NAME',
        )
    parser.add_argument(
        '--exit', action='store_true', default=False, help='exit when done'
    )

    return parser


def main() -> None:
    """Mock emulator."""

    args = _get_parser().parse_args()

    if len(args.echo) > 0:
        print(' '.join(args.echo))
    sys.stdout.flush()

    threads = []

    for chan in args.tcp_channel:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.bind(('localhost', 0))
        port = sock.getsockname()[1]
        with open(os.path.join(args.working_dir, chan), 'w') as file:
            file.write(str(port))
        thread = Thread(target=_tcp_thread, args=(sock,))
        thread.start()
        threads.append(thread)

    if sys.platform != 'win32':
        for chan in args.pty_channel:
            controller, tty = os.openpty()
            with open(os.path.join(args.working_dir, chan), 'w') as file:
                file.write(os.ttyname(tty))
            thread = Thread(target=_pty_thread, args=(controller,))
            thread.start()
            threads.append(thread)

    for thread in threads:
        thread.join()

    if not args.exit:
        while True:
            time.sleep(1)


if __name__ == '__main__':
    main()
