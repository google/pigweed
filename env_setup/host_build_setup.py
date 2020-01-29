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
"""Builds and sets up environment to use host build."""

import os
import subprocess


def install(pw_root, env):
    host_dir = os.path.join(pw_root, 'out', 'host')
    with env():
        subprocess.check_call(['gn', 'gen', host_dir], cwd=pw_root)
        subprocess.check_call(['ninja', '-C', host_dir], cwd=pw_root)

    env.prepend('PATH', os.path.join(host_dir, 'host_tools'))
