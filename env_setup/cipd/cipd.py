#!/usr/bin/env python3
# Copyright 2019 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy
# of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.
"""Installs and then runs cipd.

This script installs cipd in ./tools/ (if necessary) and then executes it,
passing through all arguments.

Must be tested with Python 2 and Python 3.
"""

from __future__ import print_function

import hashlib
import os
import platform
import subprocess
import sys

try:
    import httplib
except ImportError:
    import http.client as httplib

try:
    import urlparse  # Python 2.
except ImportError:
    import urllib.parse as urlparse

SCRIPT_DIR = os.path.dirname(__file__)
VERSION_FILE = os.path.join(SCRIPT_DIR, '.cipd_version')
DIGESTS_FILE = VERSION_FILE + '.digests'
# Put CIPD client in tools so that users can easily get it in their PATH.
CIPD_HOST = 'chrome-infra-packages.appspot.com'

# Get install dir from environment since args cannot be passed through this
# script (args are passed as-is to cipd).
INSTALL_DIR = os.environ.get(
    'CIPD_PY_INSTALL_DIR',
    os.path.join(SCRIPT_DIR, 'tools'))
CLIENT = os.path.join(INSTALL_DIR, 'cipd')


def platform_normalized():
    """Normalize platform into format expected in CIPD paths."""

    try:
        os_name = platform.system().lower()
        return {
            'linux': 'linux',
            'mac': 'mac',
            'darwin': 'mac',
            'windows': 'windows',
        }[os_name]
    except KeyError:
        raise Exception('unrecognized os: {}'.format(os_name))


def arch_normalized():
    """Normalize arch into format expected in CIPD paths."""

    machine = platform.machine()
    if machine.startswith('arm'):
        return machine
    if machine.endswith('64'):
        return 'amd64'
    if machine.endswith('86'):
        return '386'
    raise Exception('unrecognized arch: {}'.format(machine))


def user_agent():
    """Generate a user-agent based on the project name and current hash."""

    try:
        rev = subprocess.check_output(
            ['git', '-C', SCRIPT_DIR, 'rev-parse', 'HEAD']).strip()
    except subprocess.CalledProcessError:
        rev = '???'
    return 'pigweed-infra/tools/{}'.format(rev)


def actual_hash(path):
    """Hash the file at path and return it."""

    hasher = hashlib.sha256()
    with open(path, 'rb') as ins:
        hasher.update(ins.read())
    return hasher.hexdigest()


def expected_hash():
    """Pulls expected hash from digests file."""

    expected_plat = '{}-{}'.format(platform_normalized(), arch_normalized())

    with open(DIGESTS_FILE, 'r') as ins:
        for line in ins:
            line = line.strip()
            if line.startswith('#') or not line:
                continue
            plat, hashtype, hashval = line.split()
            if (hashtype == 'sha256' and plat == expected_plat):
                return hashval
    raise Exception('platform {} not in {}'.format(expected_plat,
                                                   DIGESTS_FILE))


def client_bytes():
    """Pull down the CIPD client and return it as a bytes object.

    Often CIPD_HOST returns a 302 FOUND with a pointer to
    storage.googleapis.com, so this needs to handle redirects, but it
    shouldn't require the initial response to be a redirect either.
    """

    with open(VERSION_FILE, 'r') as ins:
        version = ins.read().strip()

    conn = httplib.HTTPSConnection(CIPD_HOST)
    path = '/client?platform={platform}-{arch}&version={version}'.format(
        platform=platform_normalized(),
        arch=arch_normalized(),
        version=version)

    for _ in range(10):
        conn.request('GET', path)
        res = conn.getresponse()
        # Have to read the response before making a new request, so make sure
        # we always read it.
        content = res.read()

        # Found client bytes.
        if res.status == httplib.OK:  # pylint: disable=no-else-return
            return content

        # Redirecting to another location.
        elif res.status == httplib.FOUND:
            location = res.getheader('location')
            url = urlparse.urlparse(location)
            if url.netloc != conn.host:
                conn = httplib.HTTPSConnection(url.netloc)
            path = '{}?{}'.format(url.path, url.query)

        # Some kind of error in this response.
        else:
            break

    raise Exception('failed to download client')


def bootstrap():
    """Bootstrap cipd client installation."""

    client_dir = os.path.dirname(CLIENT)
    if not os.path.isdir(client_dir):
        os.makedirs(client_dir)

    print('Bootstrapping cipd client for {}-{}'.format(platform_normalized(),
                                                       arch_normalized()))
    tmp_path = os.path.join(INSTALL_DIR, '.cipd.tmp')
    with open(tmp_path, 'wb') as tmp:
        tmp.write(client_bytes())

    expected = expected_hash()
    actual = actual_hash(tmp_path)

    if expected != actual:
        raise Exception('digest of downloaded CIPD client is incorrect, '
                        'check that digests file is current')

    os.chmod(tmp_path, 0o755)
    os.rename(tmp_path, CLIENT)


def selfupdate():
    """Update cipd client."""

    cmd = [
        CLIENT,
        'selfupdate',
        '-version-file', VERSION_FILE,
        '-service-url', 'https://{}'.format(CIPD_HOST),
    ]  # yapf: disable
    subprocess.check_call(cmd)


def init():
    """Install/update cipd client."""

    os.environ['CIPD_HTTP_USER_AGENT_PREFIX'] = user_agent()

    try:
        if not os.path.isfile(CLIENT):
            bootstrap()

        try:
            selfupdate()
        except subprocess.CalledProcessError:
            print('CIPD selfupdate failed. Bootstrapping then retrying...',
                  file=sys.stderr)
            bootstrap()
            selfupdate()

    except Exception:
        print('Failed to initialize CIPD. Run '
              '`CIPD_HTTP_USER_AGENT_PREFIX={user_agent}/manual {client} '
              "selfupdate -version-file '{version_file}'` "
              'to diagnose if this is persistent.'.format(
                  user_agent=user_agent(),
                  client=CLIENT,
                  version_file=VERSION_FILE,
              ),
              file=sys.stderr)
        raise


if __name__ == '__main__':
    init()
    subprocess.check_call([CLIENT] + sys.argv[1:])
