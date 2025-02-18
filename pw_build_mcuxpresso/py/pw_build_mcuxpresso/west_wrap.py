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
"""West manifest output support."""

import io
import os
from contextlib import redirect_stdout
from pathlib import Path
from subprocess import check_output

from west.app.main import WestApp


def fetch_project(
    workdir_path: Path, src_repo: str, src_rev: str | None = None
):
    """
    Fetch West project into given directory
    """
    # Save cwd to restore it after running west
    current_dir = os.getcwd()
    app = WestApp()

    argv = ['init', '-m', src_repo, str(workdir_path)]
    if src_rev is not None:
        argv.extend(['--mr', src_rev])
    app.run(argv)

    os.chdir(workdir_path)
    app.run(['update', '-n'])

    os.chdir(current_dir)


def list_modules(
    workdir_path: Path, src_repo: str
) -> list[tuple[str, str, str]]:
    """
    Parses manifest to return project's source information

    Args:
        workdir_path: path to project's working directory
        src_repo: url of source repository

    Returns:
        list of (module_name, module_revision, module_url) for all
        modules
    """
    # Save cwd to restore it after running west
    current_dir = os.getcwd()
    workdir_path = workdir_path.resolve()
    os.chdir(workdir_path)

    app = WestApp()
    with io.StringIO() as buf, redirect_stdout(buf):
        app.run(['list', '-f', '{name} {sha} {url} {path}'])
        output_raw = buf.getvalue()

    # First line is core module, rest is submodules
    core, *submodules = output_raw.splitlines()

    # We have to manually extract core's commit hash
    core_path = workdir_path / core.split()[3]
    core_revision = check_output(
        ['git', 'rev-parse', 'HEAD'],
        text=True,
        cwd=core_path,
    ).strip()

    os.chdir(current_dir)

    modules = [("mcuxpresso-sdk", core_revision, src_repo)]
    modules.extend(
        (module[0], module[1], module[2])
        for module in map(lambda line: line.split(" "), submodules)
    )
    return modules
