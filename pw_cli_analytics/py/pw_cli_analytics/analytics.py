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
"""Report usage to Google Analytics."""

import argparse
import dataclasses
import enum
import getpass
import json
import logging
import multiprocessing
import os
from pathlib import Path
import platform
import pprint
import re
import subprocess
import sys
from typing import Any, Sequence
import time

import requests

import pw_env_setup.config_file

from . import cli
from . import config

_LOG: logging.Logger = logging.getLogger(__name__)

SAFE_SUBCOMMANDS_TO_ALWAYS_REPORT = (
    'analytics',
    'bloat',
    'build',
    'console',
    'doctor',
    'emu',
    'format',
    'heap-viewer',
    'help',
    'ide',
    'keep-sorted',
    'logdemo',
    'module',
    'package',
    'presubmit',
    'python-packages',
    'requires',
    'rpc',
    'test',
    'update',
    'watch',
)


def _upstream_remote(cwd: Path | str | None = None) -> str | None:
    """Return the remote for this branch, or None if there are errors."""
    upstream = ''
    prev_upstreams = [upstream]
    while '/' not in upstream:
        try:
            upstream = (
                subprocess.run(
                    [
                        'git',
                        'rev-parse',
                        '--abbrev-ref',
                        '--symbolic-full-name',
                        f'{upstream}@{{upstream}}',
                    ],
                    check=True,
                    cwd=cwd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.DEVNULL,
                )
                .stdout.decode()
                .strip()
            )
        except subprocess.CalledProcessError:
            return None
        prev_upstreams.append(upstream)

        if upstream in prev_upstreams:
            return None

    remote = upstream.split('/')[0]
    try:
        url = (
            subprocess.run(
                ['git', 'config', '--get', f'remote.{remote}.url'],
                check=True,
                cwd=cwd,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
            )
            .stdout.decode()
            .strip()
        )
    except subprocess.CalledProcessError:
        return None

    # Don't return local paths.
    if ':' not in url:
        return None

    return url


def _fallback_remote(cwd: Path | str | None = None) -> str | None:
    """Get all the remotes and use some heuristics to pick one of them."""
    remotes = {}

    try:
        result = subprocess.run(
            ['git', 'config', '--get-regexp', r'^remote\..*\.url$'],
            check=True,
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
    except subprocess.CalledProcessError:
        return None

    for line in result.stdout.decode().strip().splitlines():
        branch, remote = line.split()

        # Exclude remotes
        if ':' not in remote:
            continue

        if branch.startswith('remote.'):
            branch = branch[len('remote.') :]
        if branch.endswith('.url'):
            branch = branch[: -len('.url')]
        remotes[branch] = remote

    # Prefer the remote named 'origin'.
    if 'origin' in remotes:
        return remotes['origin']

    # Filter to the remotes that occur most often. Then pick one of those
    # arbitrarily.
    values = list(remotes.values())
    return max(set(values), key=values.count)


def _remote(cwd: Path | str | None = None) -> str | None:
    url = _upstream_remote(cwd=cwd) or _fallback_remote(cwd=cwd)
    if not url:
        return None

    # If there's a username in the url, remove it.
    return re.sub(r'[\w_]+@', '<username>@', url)


@dataclasses.dataclass
class GerritCommit:
    commit: str
    change_id: int


def _pigweed_commit(cwd: Path | str | None = None) -> GerritCommit | None:
    """Find the most recently submitted Pigweed commit in use by this checkout.

    Returns: Tuple of the Git commit hash and the Gerrit change id.
    """
    pw_root = cwd or os.environ.get('PW_ROOT')
    if not pw_root:
        return None

    proc = subprocess.Popen(
        ['git', 'log'],
        cwd=pw_root,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )

    commit = None
    assert proc.stdout
    for i, raw_line in enumerate(proc.stdout):
        # If we get this far in we've spent too much time and should give up.
        if i > 100000:
            break

        line = raw_line.decode(encoding='utf-8')
        parts = line.split()
        if len(parts) == 2 and parts[0] == 'commit':
            commit = parts[1]
            continue

        pw_remote = 'https://pigweed-review.googlesource.com/c/pigweed/pigweed'
        if line.strip().startswith(f'Reviewed-on: {pw_remote}'):
            assert commit
            change_id = int(line.split('/')[-1])
            return GerritCommit(commit, change_id)

    return None


@dataclasses.dataclass
class UserProperties:
    luci_buildbucket_id: str = os.environ.get('BUILDBUCKET_ID', '')
    luci_buildbucket_name: str = os.environ.get('BUILDBUCKET_NAME', '')
    luci_swarming_id: str = os.environ.get('SWARMING_TASK_ID', '')
    num_cores: int = multiprocessing.cpu_count()
    cpu_architecture: str = platform.machine()
    cpu_bits: int = 64 if sys.maxsize > 2**32 else 32
    os_name: str = platform.system()
    automated_build: bool = False

    def __post_init__(self):
        # TODO: b/319320838 - Figure out how to tell if running in TreeHugger.
        if self.luci_buildbucket_id or self.luci_swarming_id:
            self.automated_build = True


@dataclasses.dataclass
class Payload:
    user_properties: UserProperties = dataclasses.field(
        default_factory=UserProperties,
    )
    event_params: dict[str, int | str | None] = dataclasses.field(
        default_factory=dict
    )


def remove_username(
    arg: Path | str,
    *,
    username: str = getpass.getuser(),
    home: str = os.path.expanduser('~'),
) -> str:
    """Remove usernames from Path-like objects.

    Examples:
        /home/user/foo/bar => ~/foo/bar
        /home/user/foo/user/bar => ~/foo/$USERNAME/bar
        /data/foo/bar => /data/foo/bar
        --force => --force

    Args:
        arg: Possible Path-like object.
        username: Username of current user.
        home: Home directory of current user.
    """
    arg = str(arg)
    if arg.startswith(home):
        arg = f'$HOME{arg[len(home):]}'

    return re.sub(rf'\b{username}\b', '$USERNAME', arg)


class State(enum.Enum):
    UNKNOWN = 0
    ALREADY_INITIALIZED = 1
    NEWLY_INITIALIZED = 2


_INITIALIZE_STATE: State | None = None


class Analytics:
    """Report usage to Google Analytics."""

    def __init__(
        self,
        argv: Sequence[str],
        parsed_args: argparse.Namespace,
        *args,
        debug: bool = False,
        **kwargs,
    ):
        """Initializes an Analytics object.

        Args:
            argv: List of arguments passed to this script.
            parsed_args: The parsed representation of those arguments.
            *args: Arguments to pass through to super().
            debug: Whether to use the debug host.
            **kwargs: Arguments to pass through to super().
        """

        super().__init__(*args, **kwargs)

        self._argv: Sequence[str] = argv[:]
        self._parsed_args: argparse.Namespace = parsed_args

        self._debug: bool = debug
        self._start_time: float | None = None

        self.config = config.AnalyticsPrefs()

        self._url: str = self.config['prod_url']
        if debug:
            self._url = self.config['debug_url']

        self._enabled = True
        if self.config['uuid'] is None or not self.config['enabled']:
            self._enabled = False
        elif self._parsed_args.command == 'analytics':
            self._enabled = False
        elif _INITIALIZE_STATE == State.NEWLY_INITIALIZED:
            self._enabled = False

    def _payload(self, cwd: Path) -> Payload:
        """Create a Payload object.

        Create a Payload object based on the system and the arguments to the
        current script based on the config.

        See https://pigweed.dev/pw_cli_analytics for details about these values.

        Args:
            cwd: Checkout directory.
        """
        payload = Payload()
        payload.event_params['command'] = 'redacted'

        rsn = self.config['report_subcommand_name']
        if rsn == 'always':
            payload.event_params['command'] = self._parsed_args.command

        elif rsn == 'limited':
            if self._parsed_args.command in SAFE_SUBCOMMANDS_TO_ALWAYS_REPORT:
                payload.event_params['command'] = self._parsed_args.command

        elif rsn == 'never':
            pass

        else:
            raise ValueError(f'unexpected report_subcommand_name value {rsn!r}')

        if self.config['report_command_line']:
            if rsn != 'always':
                raise ValueError(
                    'report_command_line is True but report_subcommand_name '
                    f'is {rsn!r}'
                )
            for i, arg in enumerate(self._argv):
                if i >= 10:
                    break
                payload.event_params[f'argv_{i}'] = remove_username(arg)

        if self.config['report_project_name']:
            pw_config: dict[str, Any] = pw_env_setup.config_file.load().get(
                'pw', {}
            )
            root_var = pw_config.get('pw_env_setup', {}).get('root_variable')
            payload.event_params['project_root_var'] = root_var

        if self.config['report_remote_url']:
            payload.event_params['git_remote'] = _remote(cwd=cwd)

        pw_commit = _pigweed_commit()
        if pw_commit:
            payload.event_params['pigweed_commit'] = pw_commit.commit
            payload.event_params['pigweed_change_id'] = pw_commit.change_id

        return payload

    def _event(self, payload: Payload):
        """Send a Payload to Google Analytics."""

        if not self._enabled:
            return

        headers = {
            'User-Agent': 'pw-command-line-tool',
            'Content-Type': 'application/json',
        }

        # Values in Google Analytics need to be wrapped in a {'value': x} dict.
        def convert(x):
            return {'value': '' if x is None else str(x)}

        # There are limitations on names. For more see
        # https://support.google.com/analytics/answer/13316687?hl=en.
        name = f'pw_{payload.event_params["command"]}'

        measurement = {
            'client_id': self.config['uuid'],
            'user_properties': {
                k: convert(v)
                for k, v in dataclasses.asdict(payload.user_properties).items()
                if v
            },
            'events': [
                {
                    'name': name.replace('-', '_'),
                    'params': payload.event_params,
                },
            ],
        }

        api = self.config['api_secret']
        meas_id = self.config['measurement_id']

        url = f'{self._url}?api_secret={api}&measurement_id={meas_id}'

        _LOG.debug('POST %s', url)
        for line in pprint.pformat(measurement).splitlines():
            _LOG.debug('%s', line)

        raw_json = json.dumps(measurement)
        _LOG.debug('Raw json: %r', raw_json)

        # Make the request but don't wait very long, and don't surface any
        # errors to the user.
        r = requests.post(
            url,
            headers=headers,
            data=raw_json,
            timeout=3,
        )
        try:
            r.raise_for_status()
            _LOG.debug('Response-Content: %s', r.content)
        except requests.exceptions.HTTPError as exc:
            _LOG.debug('Request failed: %r', exc)

    def begin(self, cwd=None):
        _LOG.debug('analytics.py begin()')
        if not self._enabled:
            return
        self._start_time = time.monotonic()
        self._event(self._payload(cwd=cwd))

    def end(self, status, cwd=None):
        _LOG.debug('analytics.py end()')
        if not self._enabled:
            return
        payload = self._payload(cwd=cwd)
        payload.event_params['duration_ms'] = int(
            (time.monotonic() - self._start_time) * 1000
        )
        payload.event_params['status'] = status
        self._event(payload)


def _intro():
    print(
        """
================================================================================
The Pigweed developer tool (`pw`) uses Google Analytics to report usage,
diagnostic, and error data. This data is used to help improve Pigweed, its
libraries, and its tools.

Telemetry is not sent on the very first run. To disable reporting of telemetry
for future invocations, run this terminal command:

    pw cli-analytics --opt-out

If you opt out of telemetry, no further information will be sent. This data is
collected in accordance with the Google Privacy Policy
(https://policies.google.com/privacy). For more details on how Pigweed collects
telemetry, see https://pigweed.dev/pw_cli_analytics.
================================================================================
""".strip(),
        file=sys.stderr,
    )


def initialize() -> State:
    """Initialize Google Analytics state.

    This checks to see if the user file exists (and thus we have a UUID). If it
    does, we return immediately. If not, we print the intro message, opt-in
    (which creates the file and UUID), and return.
    """
    global _INITIALIZE_STATE  # pylint: disable=global-statement
    if _INITIALIZE_STATE is not None:
        return _INITIALIZE_STATE

    if config.DEFAULT_USER_FILE.is_file():
        _INITIALIZE_STATE = State.ALREADY_INITIALIZED
        return _INITIALIZE_STATE

    _intro()

    cli.run(opt='in')

    _INITIALIZE_STATE = State.NEWLY_INITIALIZED
    return _INITIALIZE_STATE


def finalize() -> None:
    """Finalize Google Analytics state.

    If we printed the intro message at the beginning of the command, print it
    again at the end. (We may have produced a lot of output in between and we
    don't want people to miss this.)
    """
    if _INITIALIZE_STATE == State.NEWLY_INITIALIZED:
        _intro()
