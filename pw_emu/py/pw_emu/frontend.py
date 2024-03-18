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
"""User API"""

import io
import os
import subprocess
import tempfile

from pathlib import Path
from typing import Any

from pw_emu.core import (
    AlreadyRunning,
    Config,
    ConfigError,
    Connector,
    Launcher,
    InvalidEmulator,
    InvalidChannelType,
    NotRunning,
)


class Emulator:
    """Launches, controls and interacts with an emulator instance."""

    def __init__(self, wdir: Path, config_path: Path | None = None) -> None:
        self._wdir = wdir
        self._config_path = config_path
        self._connector: Connector | None = None
        self._launcher: Launcher | None = None

    def _get_launcher(self, target: str) -> Launcher:
        """Returns an emulator for a given target.

        If there are multiple emulators for the same target it will return
        an arbitrary emulator launcher.
        """
        config = Config(self._config_path)
        target_config = config.get(
            ['targets', target],
            optional=False,
            entry_type=dict,
        )
        for key in target_config.keys():
            try:
                return Launcher.get(key, self._config_path)
            except InvalidEmulator:
                pass
        raise ConfigError(
            self._config_path,
            f'could not determine emulator for target `{target}`',
        )

    def start(
        self,
        target: str,
        file: Path | None = None,
        pause: bool = False,
        debug: bool = False,
        foreground: bool = False,
        args: str | None = None,
    ) -> None:
        """Starts the emulator for the given ``target``.

        If ``file`` is set the emulator loads the file before starting.

        If ``pause`` is ``True`` the emulator pauses until the debugger is
        connected.

        If ``debug`` is ``True`` the emulator runs in the foreground with debug
        output enabled. This is useful for seeing errors, traces, etc.

        If ``foreground`` is ``True`` the emulator runs in the foreground,
        otherwise it starts in daemon mode. Foreground mode is useful when
        there is another process controlling the emulator's life cycle,
        e.g. cuttlefish.

        ``args`` are passed directly to the emulator.

        """
        if self._connector:
            raise AlreadyRunning(self._wdir)

        if self._launcher is None:
            self._launcher = self._get_launcher(target)
        self._connector = self._launcher.start(
            wdir=self._wdir,
            target=target,
            file=file,
            pause=pause,
            debug=debug,
            foreground=foreground,
            args=args,
        )

    def _c(self) -> Connector:
        if self._connector is None:
            self._connector = Connector.get(self._wdir)
            if not self.running():
                raise NotRunning(self._wdir)
        return self._connector

    def running(self) -> bool:
        """Checks if the main emulator process is already running."""

        try:
            return self._c().running()
        except NotRunning:
            return False

    def _path(self, name: Path | str) -> Path | str:
        """Returns the full path for a given emulator file."""

        return os.path.join(self._wdir, name)

    def stop(self):
        """Stop the emulator."""

        return self._c().stop()

    def get_gdb_remote(self) -> str:
        """Returns a string that can be passed to the target remote ``gdb``
        command.

        """

        chan_type = self._c().get_channel_type('gdb')

        if chan_type == 'tcp':
            host, port = self._c().get_channel_addr('gdb')
            return f'{host}:{port}'

        if chan_type == 'pty':
            return self._c().get_channel_path('gdb')

        raise InvalidChannelType(chan_type)

    def get_gdb_cmd(self) -> list[str]:
        """Returns the ``gdb`` command for current target."""
        return self._c().get_gdb_cmd()

    def run_gdb_cmds(
        self,
        commands: list[str],
        executable: str | None = None,
        pause: bool = False,
    ) -> subprocess.CompletedProcess:
        """Connects to the target and runs the given commands silently
        in batch mode.

        ``executable`` is optional but may be required by some ``gdb`` commands.

        If ``pause`` is set, execution stops after running the given commands.

        """

        cmd = self._c().get_gdb_cmd().copy()
        if not cmd:
            raise ConfigError(self._c().get_config_path(), 'gdb not configured')

        cmd.append('-batch-silent')
        cmd.append('-ex')
        cmd.append(f'target remote {self.get_gdb_remote()}')
        for gdb_cmd in commands:
            cmd.append('-ex')
            cmd.append(gdb_cmd)
        if pause:
            cmd.append('-ex')
            cmd.append('disconnect')
        if executable:
            cmd.append(executable)
        return subprocess.run(cmd, capture_output=True)

    def reset(self) -> None:
        """Performs a software reset."""
        self._c().reset()

    def list_properties(self, path: str) -> list[dict]:
        """Returns the property list for an emulator object.

        The object is identified by a full path. The path is
        target-specific and the format of the path is backend-specific.

        QEMU path example: ``/machine/unattached/device[10]``

        renode path example: ``sysbus.uart``

        """
        return self._c().list_properties(path)

    def set_property(self, path: str, prop: str, value: Any) -> None:
        """Sets the value of an emulator's object property."""

        self._c().set_property(path, prop, value)

    def get_property(self, path: str, prop: str) -> Any:
        """Returns the value of an emulator's object property."""

        return self._c().get_property(path, prop)

    def get_channel_type(self, name: str) -> str:
        """Returns the channel type

        Currently ``pty`` and ``tcp`` are the only supported types.

        """

        return self._c().get_channel_type(name)

    def get_channel_path(self, name: str) -> str:
        """Returns the channel path. Raises ``InvalidChannelType`` if this
        is not a ``pty`` channel.

        """

        return self._c().get_channel_path(name)

    def get_channel_addr(self, name: str) -> tuple:
        """Returns a pair of ``(host, port)`` for the channel. Raises
        ``InvalidChannelType`` if this is not a TCP channel.

        """

        return self._c().get_channel_addr(name)

    def get_channel_stream(
        self,
        name: str,
        timeout: float | None = None,
    ) -> io.RawIOBase:
        """Returns a file object for a given host-exposed device.

        If ``timeout`` is ``None`` than reads and writes are blocking. If
        ``timeout`` is ``0`` the stream is operating in non-blocking
        mode. Otherwise read and write will timeout after the given
        value.

        """

        return self._c().get_channel_stream(name, timeout)

    def get_channels(self) -> list[str]:
        """Returns the list of available channels."""

        return self._c().get_channels()

    def set_emu(self, emu: str) -> None:
        """Sets the emulator type for this instance."""

        self._launcher = Launcher.get(emu, self._config_path)

    def cont(self) -> None:
        """Resumes the emulator's execution."""

        self._c().cont()


class TemporaryEmulator(Emulator):
    """Temporary emulator instances.

    Manages emulator instances that run in temporary working
    directories. The emulator instance is stopped and the working
    directory is cleared when the ``with`` block completes.

    It also supports interoperability with the ``pw_emu`` cli, e.g. starting the
    emulator with the CLI and then controlling it from the Python API.

    Usage example:

    .. code-block:: python

        # programatically start and load an executable then access it
        with TemporaryEmulator() as emu:
            emu.start(target, file)
            with emu.get_channel_stream(chan) as stream:
                ...

    .. code-block:: python

        # or start it form the command line then access it
        with TemporaryEmulator() as emu:
            build.bazel(
                ctx,
                "run",
                exec_path,
                "--run_under=pw emu start <target> --file "
            )
            with emu.get_channel_stream(chan) as stream:
                ...

    """

    def __init__(
        self,
        config_path: Path | None = None,
        cleanup: bool = True,
    ) -> None:
        self._temp = tempfile.TemporaryDirectory()
        self._cleanup = cleanup
        super().__init__(Path(self._temp.name), config_path)

    def __enter__(self):
        # Interoperability with pw emu cli.
        os.environ["PW_EMU_WDIR"] = self._wdir
        return self

    def __exit__(self, exc, value, traceback) -> None:
        self.stop()
        del os.environ["PW_EMU_WDIR"]
        if self._cleanup:
            self._temp.cleanup()
