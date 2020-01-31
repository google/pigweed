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
"""The env module defines the environment variables used by Pigweed."""

from typing import Optional

from pw_cli import envparse


def pigweed_environment_parser() -> envparse.EnvironmentParser:
    """Defines Pigweed's environment variables on an EnvironmentParser."""
    parser = envparse.EnvironmentParser(prefix='PW_')

    parser.add_var('PW_CARGO_SETUP', type=envparse.strict_bool, default=False)
    parser.add_var('PW_EMOJI', type=envparse.strict_bool, default=False)
    parser.add_var('PW_ENVSETUP')
    parser.add_var('PW_ENVSETUP_FULL')
    parser.add_var('PW_ENVSETUP_QUIET',
                   type=envparse.strict_bool,
                   default=False)
    parser.add_var('PW_ROOT')
    parser.add_var('PW_SUBPROCESS', type=envparse.strict_bool, default=False)
    parser.add_var('PW_USE_COLOR', type=envparse.strict_bool, default=False)

    parser.add_var('PW_PIGWEED_CIPD_INSTALL_DIR')
    parser.add_var('PW_LUCI_CIPD_INSTALL_DIR')
    parser.add_var('PW_CIPD_INSTALL_DIR')

    return parser


# Internal: memoize environment parsing to avoid unnecessary computation in
# multiple calls to pigweed_environment().
_memoized_environment: Optional[envparse.EnvNamespace] = None


def pigweed_environment() -> envparse.EnvNamespace:
    """Returns Pigweed's parsed environment."""
    global _memoized_environment  # pylint: disable=global-statement

    if _memoized_environment is None:
        _memoized_environment = pigweed_environment_parser().parse_env()

    return _memoized_environment
