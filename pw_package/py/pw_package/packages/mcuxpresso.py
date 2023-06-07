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
"""Install and check status of the NXP mcuxpresso SDK."""

from pathlib import Path
from typing import Sequence

import pw_package.git_repo
import pw_package.package_manager


class McuxpressoSdk(pw_package.package_manager.Package):
    """Install and check status of the NXP mcuxpresso SDK."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, name='mcuxpresso', **kwargs)
        self._evkmimxrt595 = pw_package.git_repo.GitRepo(
            name='evkmimxrt595',
            url=(
                'https://pigweed-internal.googlesource.com/'
                'third_party/vendor/nxp/evkmimxrt595'
            ),
            commit='10176a38fa830208332de132b112addb189d6d24',
        )

    def install(self, path: Path) -> None:
        self._evkmimxrt595.install(path)

    def info(self, path: Path) -> Sequence[str]:
        return (
            f'{self.name} installed in: {path}',
            "Enable by running 'gn args out' and adding this line:",
            f'  dir_pw_third_party_mcuxpresso = "{path}"',
        )


pw_package.package_manager.register(McuxpressoSdk)
