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
"""Reads and parses the Pigweed config file.

See also https://pigweed.dev/seed/0101-pigweed.json.html.
"""

import json
import os


def _resolve_env(env):
    if env:
        return env
    return os.environ


def _get_project_root(env):
    for var in ('PW_PROJECT_ROOT', 'PW_ROOT'):
        if var in env:
            return env[var]
    raise ValueError('environment variable PW_PROJECT_ROOT not set')


def _pw_env_substitute(env, string):
    if not isinstance(string, str):
        return string

    # Substitute in environment variables based on $pw_env{VAR_NAME} tokens.
    for key, value in env.items():
        string = string.replace('$pw_env{' + key + '}', value)

    if '$pw_env{' in string:
        raise ValueError(f'Unresolved $pw_env\\{...} in JSON string: {string}')

    return string


def path(env=None):
    """Return the path where pigweed.json should reside."""
    env = _resolve_env(env)
    return os.path.join(_get_project_root(env=env), 'pigweed.json')


def load(env=None):
    """Load pigweed.json if it exists and return the contents."""
    env = _resolve_env(env)
    config_path = path(env=env)
    if not os.path.isfile(config_path):
        return {}

    def hook(obj):
        out = {}
        for key, val in obj.items():
            out[key] = _pw_env_substitute(env, val)
        return out

    with open(config_path, 'r') as file:
        return json.load(file, object_hook=hook)
