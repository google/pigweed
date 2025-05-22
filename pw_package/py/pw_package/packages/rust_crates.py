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
"""Install and check status of pigweed rust's third party dependencies."""

import os
from pathlib import Path
import logging
import shutil
import subprocess
from typing import Sequence

import pw_package.package_manager

_LOG = logging.getLogger(__package__)

_CARGO_CONFIG_TEMPLATE = """
[source.crates-io]
replace-with = "vendored-sources"

[source.vendored-sources]
directory = "{vendored_sources_path}"
"""


class RustCrates(pw_package.package_manager.Package):
    """Install and check status of pigweed rust's third party dependencies"""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, name='rust_crates', **kwargs)

    def install(self, path: Path) -> None:
        env = os.environ.copy()

        def log_and_run_in_directory(
            command: Sequence[str], cwd: str | None, **kwargs
        ):
            _LOG.info('==> %s', ' '.join(command))
            return subprocess.run(
                command,
                env=env,
                cwd=cwd,
                check=True,
                **kwargs,
            )

        def log_and_run(command: Sequence[str], **kwargs):
            log_and_run_in_directory(command, cwd=None, **kwargs)

        pw_root_env_path = Path(env.get('PW_ROOT', ''))
        project_root_env_path = Path(env.get('PW_PROJECT_ROOT', ''))
        gn_path = shutil.which('gn')
        gn_path = gn_path if gn_path is not None else ""
        if (
            not pw_root_env_path.is_dir()
            or not project_root_env_path.is_dir()
            or not gn_path
        ):
            raise EnvironmentError("Environment not properly bootstrapped!")

        if not shutil.which("gnaw"):
            raise FileNotFoundError(
                'Missing required tool `gnaw`! '
                'Try `pw package install cargo_gnaw`.'
            )

        for subdir in ('crates_std', 'crates_no_std'):
            source_project_root = (
                project_root_env_path / 'third_party' / 'crates_io' / subdir
            )
            vendor_project_root = path / subdir

            # Vendor crates sources according to the manifest.
            log_and_run(
                (
                    'cargo',
                    'vendor',
                    '--manifest-path',
                    str(source_project_root / 'Cargo.toml'),
                    '--locked',
                    str(vendor_project_root),
                )
            )

            # After crates being vendored, use the directory with vendored
            # crates as a new cargo project. All the following GN rule
            # generation are going to happen in the new project and without
            # touching the source tree.

            # Adding cargo configuration to point the crates sources to the
            # vendored directory instead of crates-io.
            cargo_config = vendor_project_root / ".cargo" / "config.toml"
            os.makedirs(cargo_config.parent, exist_ok=True)
            with open(cargo_config, "w") as f:
                f.write(
                    _CARGO_CONFIG_TEMPLATE.format(
                        vendored_sources_path=vendor_project_root
                    )
                )

            shutil.copyfile(
                source_project_root / "Cargo.toml",
                vendor_project_root / "Cargo.toml",
            )
            shutil.copyfile(
                source_project_root / "Cargo.lock",
                vendor_project_root / "Cargo.lock",
            )

            log_and_run_in_directory(
                (
                    'gnaw',
                    '--manifest-path',
                    str(vendor_project_root / 'Cargo.toml'),
                    '-o',
                    str(vendor_project_root / 'BUILD.gn'),
                    '--no-default-features',
                    '--features',
                    'gn-build',
                    '--project-root',
                    str(project_root_env_path),
                    '--skip-root',
                    '--gn-bin',
                    gn_path,
                ),
                cwd=str(vendor_project_root),
            )

        _LOG.info('Done! %s installs successfully!', self.name)

    def info(self, path: Path) -> Sequence[str]:
        return (f'{self.name} installed to: {path}',)


pw_package.package_manager.register(RustCrates)
