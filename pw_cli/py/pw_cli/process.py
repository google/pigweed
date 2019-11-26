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

"""Module for running subprocesses from pw and capturing their output."""

import asyncio
import logging
import shlex

from pw_cli.color import Color
import pw_cli.log

_LOG = logging.getLogger(__name__)


async def run_async(*args: str, silent: bool = False) -> int:
    """Runs a command, capturing and logging its output.

    Returns the exit status of the command.
    """

    command = args[0]
    _LOG.info('Running `%s`', shlex.join(command))

    stdout = asyncio.subprocess.DEVNULL if silent else asyncio.subprocess.PIPE
    process = await asyncio.create_subprocess_exec(
        *command,
        stdout=stdout,
        stderr=asyncio.subprocess.STDOUT)

    if process.stdout is not None:
        while line := await process.stdout.readline():
            _LOG.log(pw_cli.log.LOGLEVEL_STDOUT,
                     '[%s] %s',
                     Color.bold_white(process.pid),
                     line.decode().rstrip())

    status = await process.wait()
    if status == 0:
        _LOG.info('%s exited successfully', command[0])
    else:
        _LOG.error('%s exited with status %d', command[0], status)

    return status


def run(*args: str, silent: bool = False) -> int:
    """Synchronous wrapper for run_async."""
    return asyncio.run(run_async(args, silent))
