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
"""Clone the Pigweed examples repo."""

import pathlib
from typing import Sequence

import pw_package.git_repo
import pw_package.package_manager


class PigweedExamplesRepo(pw_package.git_repo.GitRepo):
    """Install and check status of the Pigweed examples repo."""

    def __init__(self, *args, **kwargs):
        super().__init__(
            *args,
            name='pigweed_examples_repo',
            url='https://pigweed.googlesource.com/pigweed/examples',
            commit='HEAD',
            **kwargs,
        )

    def info(self, path: pathlib.Path) -> Sequence[str]:
        return (f'{self.name} installed in: {path}',)


pw_package.package_manager.register(PigweedExamplesRepo)
