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

import argparse
import logging
from typing import Callable
from typing import NamedTuple
_LOG = logging.getLogger(__name__)

DefineArgsFunction = Callable[[argparse.ArgumentParser], None]


class Plugin(NamedTuple):
    name: str
    command_function: Callable
    help: str = ''
    define_args_function: DefineArgsFunction = lambda _: None


# This is the global CLI plugin registry.
registry = []


def register(
        name: str,
        command_function: Callable,
        help: str = '',
        define_args_function: DefineArgsFunction = lambda _: None,
) -> None:
    registry.append(
        Plugin(
            name=name,
            command_function=command_function,
            help=help,
            define_args_function=define_args_function,
        ))
    _LOG.debug('Registered plugin: %s', name)
