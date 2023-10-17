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
"""Pigweed renode frontend."""

import socket
import time
import xmlrpc.client

from pathlib import Path
from typing import Optional, List, Any

from pw_emu.core import (
    Connector,
    Handles,
    InvalidChannelType,
    Launcher,
    Error,
    WrongEmulator,
)


class RenodeRobotError(Error):
    """Exception for Renode robot errors."""

    def __init__(self, err: str):
        super().__init__(err)


class RenodeLauncher(Launcher):
    """Start a new renode process for a given target and config file."""

    def __init__(self, config_path: Optional[Path] = None):
        super().__init__('renode', config_path)
        self._start_cmd: List[str] = []

    @staticmethod
    def _allocate_port() -> int:
        """Allocate renode ports.

        This is inherently racy but renode currently does not have proper
        support for dynamic ports. It accecept 0 as a port and the OS allocates
        a dynamic port but there is no API to retrive the port.

        """
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.bind(('localhost', 0))
        port = sock.getsockname()[1]
        sock.close()

        return port

    def _pre_start(
        self,
        target: str,
        file: Optional[Path] = None,
        pause: bool = False,
        debug: bool = False,
        args: Optional[str] = None,
    ) -> List[str]:
        renode = self._config.get_target_emu(['executable'])
        if not renode:
            renode = self._config.get_emu(['executable'], optional=False)

        self._start_cmd.extend([f'{renode}', '--disable-xwt'])
        port = self._allocate_port()
        self._start_cmd.extend(['--robot-server-port', str(port)])
        self._handles.add_channel_tcp('robot', 'localhost', port)

        machine = self._config.get_target_emu(['machine'], optional=False)
        self._start_cmd.extend(['--execute', f'mach add "{target}"'])

        self._start_cmd.extend(
            ['--execute', f'machine LoadPlatformDescription @{machine}']
        )

        terms = self._config.get_target_emu(
            ['channels', 'terminals'], entry_type=dict
        )
        for name in terms.keys():
            port = self._allocate_port()
            dev_path = self._config.get_target_emu(
                ['channels', 'terminals', name, 'device-path'],
                optional=False,
                entry_type=str,
            )
            term_type = self._config.get_target_emu(
                ['channels', 'terminals', name, 'type'],
                entry_type=str,
            )
            if not term_type:
                term_type = self._config.get_emu(
                    ['channels', 'terminals', 'type'],
                    entry_type=str,
                )
            if not term_type:
                term_type = 'tcp'

            cmd = 'emulation '
            if term_type == 'tcp':
                cmd += f'CreateServerSocketTerminal {port} "{name}" false'
                self._handles.add_channel_tcp(name, 'localhost', port)
            elif term_type == 'pty':
                path = self._path(name)
                cmd += f'CreateUartPtyTerminal "{name}" "{path}"'
                self._handles.add_channel_pty(name, str(path))
            else:
                raise InvalidChannelType(term_type)

            self._start_cmd.extend(['--execute', cmd])
            self._start_cmd.extend(
                ['--execute', f'connector Connect {dev_path} {name}']
            )

        port = self._allocate_port()
        self._start_cmd.extend(['--execute', f'machine StartGdbServer {port}'])
        self._handles.add_channel_tcp('gdb', 'localhost', port)

        if file:
            self._start_cmd.extend(['--execute', f'sysbus LoadELF @{file}'])

        if not pause:
            self._start_cmd.extend(['--execute', 'start'])

        return self._start_cmd

    def _post_start(self) -> None:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        robot = self._handles.channels['gdb']
        assert isinstance(robot, Handles.TcpChannel)

        # renode is slow to start especially during host load
        deadline = time.monotonic() + 60
        connected = False
        while time.monotonic() < deadline:
            try:
                sock.connect((robot.host, robot.port))
                connected = True
                break
            except OSError:
                pass
            time.sleep(1)

        if not connected:
            raise RenodeRobotError('failed to connect to robot channel')

        sock.close()

    def _get_connector(self, wdir: Path) -> Connector:
        return RenodeConnector(wdir)


class RenodeConnector(Connector):
    """renode implementation for the emulator specific connector methods."""

    def __init__(self, wdir: Path) -> None:
        super().__init__(wdir)
        if self.get_emu() != 'renode':
            raise WrongEmulator('renode', self.get_emu())
        robot = self._handles.channels['robot']
        host = robot.host
        port = robot.port
        self._proxy = xmlrpc.client.ServerProxy(f'http://{host}:{port}/')

    def _request(self, cmd: str, args: List[str]) -> Any:
        """Send a request using the robot interface.

        Using the robot interface is not ideal since it is designed
        for testing. However, it is more robust than the ANSI colored,
        echoed, log mixed, telnet interface.

        """

        resp = self._proxy.run_keyword(cmd, args)
        if not isinstance(resp, dict):
            raise RenodeRobotError('expected dictionary in response')
        if resp['status'] != 'PASS':
            raise RenodeRobotError(resp['error'])
        if resp.get('return'):
            return resp['return']
        return None

    def reset(self) -> None:
        self._request('ResetEmulation', [])

    def cont(self) -> None:
        self._request('StartEmulation', [])

    def list_properties(self, path: str) -> List[Any]:
        return self._request('ExecuteCommand', [f'{path}'])

    def get_property(self, path: str, prop: str) -> Any:
        return self._request('ExecuteCommand', [f'{path} {prop}'])

    def set_property(self, path: str, prop: str, value: Any) -> None:
        return self._request('ExecuteCommand', [f'{path} {prop} {value}'])
