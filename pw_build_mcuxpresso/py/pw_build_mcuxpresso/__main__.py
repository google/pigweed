#!/usr/bin/env python3
# Copyright 2021 The Pigweed Authors
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
"""Command line interface for mcuxpresso_builder."""

import argparse
import pathlib
import shutil
import sys

try:
    from pw_build_mcuxpresso import (
        bazel,
        components,
        gn,
        readme_generator,
        west_wrap,
    )
except ImportError:
    # Load from this directory if pw_build_mcuxpresso is not available.
    import bazel  # type: ignore
    import components  # type: ignore
    import gn  # type: ignore
    import readme_generator  # type: ignore
    import west_wrap  # type: ignore


def _parse_args() -> argparse.Namespace:
    """Setup argparse and parse command line args."""
    parser = argparse.ArgumentParser()

    parser.add_argument('manifest_filename', type=pathlib.Path)
    parser.add_argument(
        '--include', type=str, action='extend', nargs="+", required=True
    )
    parser.add_argument(
        '--exclude', type=str, action='extend', nargs="+", required=True
    )
    parser.add_argument('--device-core', type=str, required=True)
    parser.add_argument('--output-path', type=pathlib.Path, required=True)
    parser.add_argument('--mcuxpresso-repo', type=str, required=True)
    parser.add_argument('--mcuxpresso-rev', type=str, required=True)

    parser.add_argument('--clean', action='store_true')
    parser.add_argument('--skip-bazel', action='store_true')
    parser.add_argument('--skip-gn', action='store_true')

    return parser.parse_args()


def main():
    """Main command line function."""
    args = _parse_args()
    output_path: pathlib.Path = args.output_path

    if args.clean and output_path.is_dir():
        print("# Removing old output directory...")
        shutil.rmtree(output_path)

    west_wrap.fetch_project(
        output_path, args.mcuxpresso_repo, args.mcuxpresso_rev
    )
    modules = west_wrap.list_modules(output_path, args.mcuxpresso_repo)

    project = components.Project.from_file(
        output_path / 'core' / 'manifests' / args.manifest_filename,
        output_path,
        include=args.include,
        exclude=args.exclude,
        device_core=args.device_core,
    )

    print("# Generating output directory...")
    project.cleanup_unknown_files()

    if not args.skip_bazel:
        bazel.generate_bazel_files(project, output_path)

    if not args.skip_gn:
        gn.generate_gn_files(project, output_path)

    readme_generator.generate_readme(
        modules=modules,
        output_dir=args.output_path,
        filename="README.md",
    )

    print(f"Output directory: {output_path.resolve().as_posix()}")

    sys.exit(0)


if __name__ == '__main__':
    main()
