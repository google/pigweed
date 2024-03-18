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
"""Preconfigured checks for Python code.

These checks assume that they are running in a preconfigured Python environment.
"""

import difflib
import json
import logging
from pathlib import Path
import platform
import shutil
import sys
from tempfile import TemporaryDirectory
from typing import Optional

from pw_env_setup import python_packages

from pw_presubmit.presubmit import (
    call,
    Check,
    filter_paths,
)
from pw_presubmit.presubmit_context import (
    PresubmitContext,
    PresubmitFailure,
)
from pw_presubmit import build
from pw_presubmit.tools import log_run, colorize_diff_line

_LOG = logging.getLogger(__name__)

_PYTHON_EXTENSIONS = ('.py', '.gn', '.gni')

_PYTHON_PACKAGE_EXTENSIONS = (
    'setup.cfg',
    'constraint.list',
    'requirements.txt',
)

_PYTHON_IS_3_9_OR_HIGHER = sys.version_info >= (
    3,
    9,
)


@filter_paths(endswith=_PYTHON_EXTENSIONS)
def gn_python_check(ctx: PresubmitContext):
    build.gn_gen(ctx)
    build.ninja(ctx, 'python.tests', 'python.lint')


def _transform_lcov_file_paths(lcov_file: Path, repo_root: Path) -> str:
    """Modify file paths in an lcov file to be relative to the repo root.

    See `man geninfo` for info on the lcov format."""

    lcov_input = lcov_file.read_text()
    lcov_output = ''

    if not _PYTHON_IS_3_9_OR_HIGHER:
        return lcov_input

    for line in lcov_input.splitlines():
        if not line.startswith('SF:'):
            lcov_output += line + '\n'
            continue

        # Get the file path after SF:
        file_string = line[3:].rstrip()
        source_file_path = Path(file_string)

        # Attempt to map a generated Python package source file to the root
        # source tree.
        # pylint: disable=no-member
        if not source_file_path.is_relative_to(
            repo_root  # type: ignore[attr-defined]
        ):
            # pylint: enable=no-member
            source_file_path = repo_root / str(source_file_path).replace(
                'python/gen/', ''
            ).replace('py.generated_python_package/', '')

        # If mapping fails don't modify this line.
        # pylint: disable=no-member
        if not source_file_path.is_relative_to(
            repo_root  # type: ignore[attr-defined]
        ):
            # pylint: enable=no-member
            lcov_output += line + '\n'
            continue

        source_file_path = source_file_path.relative_to(repo_root)
        lcov_output += f'SF:{source_file_path}\n'

    return lcov_output


@filter_paths(endswith=_PYTHON_EXTENSIONS)
def gn_python_test_coverage(ctx: PresubmitContext):
    """Run Python tests with coverage and create reports."""
    build.gn_gen(ctx, pw_build_PYTHON_TEST_COVERAGE=True)
    build.ninja(ctx, 'python.tests')

    # Find coverage data files
    coverage_data_files = list(ctx.output_dir.glob('**/*.coverage'))
    if not coverage_data_files:
        return

    # Merge coverage data files to out/.coverage
    call(
        'coverage',
        'combine',
        # Leave existing coverage files in place; by default they are deleted.
        '--keep',
        *coverage_data_files,
        cwd=ctx.output_dir,
    )
    combined_data_file = ctx.output_dir / '.coverage'
    _LOG.info('Coverage data saved to: %s', combined_data_file.resolve())

    # Always ignore generated proto python and setup.py files.
    coverage_omit_patterns = '--omit=*_pb2.py,*/setup.py'

    # Output coverage percentage summary to the terminal of changed files.
    changed_python_files = list(
        str(p) for p in ctx.paths if str(p).endswith('.py')
    )
    report_args = [
        'coverage',
        'report',
        '--ignore-errors',
        coverage_omit_patterns,
    ]
    report_args += changed_python_files
    log_run(report_args, check=False, cwd=ctx.output_dir)

    # Generate a json report
    call('coverage', 'lcov', coverage_omit_patterns, cwd=ctx.output_dir)
    lcov_data_file = ctx.output_dir / 'coverage.lcov'
    lcov_data_file.write_text(
        _transform_lcov_file_paths(lcov_data_file, repo_root=ctx.root)
    )
    _LOG.info('Coverage lcov saved to: %s', lcov_data_file.resolve())

    # Generate an html report
    call('coverage', 'html', coverage_omit_patterns, cwd=ctx.output_dir)
    html_report = ctx.output_dir / 'htmlcov' / 'index.html'
    _LOG.info('Coverage html report saved to: %s', html_report.resolve())


@filter_paths(endswith=_PYTHON_PACKAGE_EXTENSIONS)
def vendor_python_wheels(ctx: PresubmitContext) -> None:
    """Download Python packages locally for the current platform."""
    build.gn_gen(ctx)
    build.ninja(ctx, 'pip_vendor_wheels')

    download_log = (
        ctx.output_dir
        / 'python/gen/pw_env_setup/pigweed_build_venv.vendor_wheels'
        / 'pip_download_log.txt'
    )
    _LOG.info('Python package download log: %s', download_log)

    wheel_output = (
        ctx.output_dir
        / 'python/gen/pw_env_setup'
        / 'pigweed_build_venv.vendor_wheels/wheels/'
    )
    wheel_destination = ctx.output_dir / 'python_wheels'
    shutil.rmtree(wheel_destination, ignore_errors=True)
    shutil.copytree(wheel_output, wheel_destination, dirs_exist_ok=True)

    _LOG.info('Python packages downloaded to: %s', wheel_destination)


def _generate_constraint_with_hashes(
    ctx: PresubmitContext, input_file: Path, output_file: Path
) -> None:
    assert input_file.is_file()

    call(
        "pip-compile",
        input_file,
        "--generate-hashes",
        "--reuse-hashes",
        "--resolver=backtracking",
        "--strip-extras",
        # Force pinning pip and setuptools
        "--allow-unsafe",
        "-o",
        output_file,
    )

    # Remove absolute paths from comments
    output_text = output_file.read_text()
    output_text = output_text.replace(str(ctx.output_dir), '')
    output_text = output_text.replace(str(ctx.root), '')
    output_text = output_text.replace(str(output_file.parent), '')

    final_output_text = ''
    for line in output_text.splitlines(keepends=True):
        # Remove --find-links lines
        if line.startswith('--find-links'):
            continue
        # Remove blank lines
        if line == '\n':
            continue
        final_output_text += line

    output_file.write_text(final_output_text)


def _update_upstream_python_constraints(
    ctx: PresubmitContext,
    update_files: bool = False,
) -> None:
    """Regenerate platform specific Python constraint files with hashes."""
    with TemporaryDirectory() as tmpdirname:
        out_dir = Path(tmpdirname)
        build.gn_gen(
            ctx,
            pw_build_PIP_REQUIREMENTS=[],
            # Use the constraint file without hashes as the input. This is where
            # new packages are added by developers.
            pw_build_PIP_CONSTRAINTS=[
                '//pw_env_setup/py/pw_env_setup/virtualenv_setup/'
                'constraint.list',
            ],
            # This should always be set to false when regenrating constraints.
            pw_build_PYTHON_PIP_INSTALL_REQUIRE_HASHES=False,
        )
        build.ninja(ctx, 'pip_constraint_update')

        # Either: darwin, linux or windows
        platform_name = platform.system().lower()

        constraint_hashes_filename = f'constraint_hashes_{platform_name}.list'
        constraint_hashes_original = (
            ctx.root
            / 'pw_env_setup/py/pw_env_setup/virtualenv_setup'
            / constraint_hashes_filename
        )
        constraint_hashes_tmp_out = out_dir / constraint_hashes_filename
        _generate_constraint_with_hashes(
            ctx,
            input_file=(
                ctx.output_dir
                / 'python/gen/pw_env_setup/pigweed_build_venv'
                / 'compiled_requirements.txt'
            ),
            output_file=constraint_hashes_tmp_out,
        )

        build.gn_gen(
            ctx,
            # This should always be set to false when regenrating constraints.
            pw_build_PYTHON_PIP_INSTALL_REQUIRE_HASHES=False,
        )
        build.ninja(ctx, 'pip_constraint_update')

        upstream_requirements_lock_filename = (
            f'upstream_requirements_{platform_name}_lock.txt'
        )
        upstream_requirements_lock_original = (
            ctx.root
            / 'pw_env_setup/py/pw_env_setup/virtualenv_setup'
            / upstream_requirements_lock_filename
        )
        upstream_requirements_lock_tmp_out = (
            out_dir / upstream_requirements_lock_filename
        )
        _generate_constraint_with_hashes(
            ctx,
            input_file=(
                ctx.output_dir
                / 'python/gen/pw_env_setup/pigweed_build_venv'
                / 'compiled_requirements.txt'
            ),
            output_file=upstream_requirements_lock_tmp_out,
        )

        if update_files:
            constraint_hashes_original.write_text(
                constraint_hashes_tmp_out.read_text()
            )
            _LOG.info('Updated: %s', constraint_hashes_original)
            upstream_requirements_lock_original.write_text(
                upstream_requirements_lock_tmp_out.read_text()
            )
            _LOG.info('Updated: %s', upstream_requirements_lock_original)
            return

        # Make a diff of required changes
        constraint_hashes_diff = list(
            difflib.unified_diff(
                constraint_hashes_original.read_text(
                    'utf-8', errors='replace'
                ).splitlines(),
                constraint_hashes_tmp_out.read_text(
                    'utf-8', errors='replace'
                ).splitlines(),
                fromfile=str(constraint_hashes_original) + ' (original)',
                tofile=str(constraint_hashes_original) + ' (updated)',
                lineterm='',
                n=1,
            )
        )
        upstream_requirements_lock_diff = list(
            difflib.unified_diff(
                upstream_requirements_lock_original.read_text(
                    'utf-8', errors='replace'
                ).splitlines(),
                upstream_requirements_lock_tmp_out.read_text(
                    'utf-8', errors='replace'
                ).splitlines(),
                fromfile=str(upstream_requirements_lock_original)
                + ' (original)',
                tofile=str(upstream_requirements_lock_original) + ' (updated)',
                lineterm='',
                n=1,
            )
        )
        if constraint_hashes_diff:
            for line in constraint_hashes_diff:
                print(colorize_diff_line(line))
        if upstream_requirements_lock_diff:
            for line in upstream_requirements_lock_diff:
                print(colorize_diff_line(line))
        if constraint_hashes_diff or upstream_requirements_lock_diff:
            raise PresubmitFailure(
                'Please run:\n'
                '\n'
                '  pw presubmit --step update_upstream_python_constraints'
            )


@filter_paths(endswith=_PYTHON_PACKAGE_EXTENSIONS)
def check_upstream_python_constraints(ctx: PresubmitContext) -> None:
    _update_upstream_python_constraints(ctx, update_files=False)


@filter_paths(endswith=_PYTHON_PACKAGE_EXTENSIONS)
def update_upstream_python_constraints(ctx: PresubmitContext) -> None:
    _update_upstream_python_constraints(ctx, update_files=True)


@filter_paths(endswith=_PYTHON_EXTENSIONS + ('.pylintrc',))
def gn_python_lint(ctx: PresubmitContext) -> None:
    build.gn_gen(ctx)
    build.ninja(ctx, 'python.lint')


@Check
def check_python_versions(ctx: PresubmitContext):
    """Checks that the list of installed packages is as expected."""

    build.gn_gen(ctx)
    constraint_file: Optional[str] = None
    requirement_file: Optional[str] = None
    try:
        for arg in build.get_gn_args(ctx.output_dir):
            if arg['name'] == 'pw_build_PIP_CONSTRAINTS':
                constraint_file = json.loads(arg['current']['value'])[0].strip(
                    '/'
                )
            if arg['name'] == 'pw_build_PIP_REQUIREMENTS':
                requirement_file = json.loads(arg['current']['value'])[0].strip(
                    '/'
                )
    except json.JSONDecodeError:
        _LOG.warning('failed to parse GN args json')
        return

    if not constraint_file:
        _LOG.warning('could not find pw_build_PIP_CONSTRAINTS GN arg')
        return
    ignored_requirements_arg = None
    if requirement_file:
        ignored_requirements_arg = [(ctx.root / requirement_file)]

    if (
        python_packages.diff(
            expected=(ctx.root / constraint_file),
            ignore_requirements_file=ignored_requirements_arg,
        )
        != 0
    ):
        raise PresubmitFailure
