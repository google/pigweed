#!/usr/bin/env python3
# Copyright 2025 The Pigweed Authors
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
"""
Finds all existing compile command fragments and merges them into
platform-specific compilation databases.
"""

import argparse
import collections
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List, NamedTuple

# A unique suffix to identify fragments created by our aspect.
_FRAGMENT_SUFFIX = ".pw_aspect.compile_commands.json"

# Supported architectures for clangd, based on the provided list.
# TODO(b/442862617): A better way than this than hardcoded list.
SUPPORTED_MARCH_ARCHITECTURES = {
    "nocona",
    "core2",
    "penryn",
    "bonnell",
    "atom",
    "silvermont",
    "slm",
    "goldmont",
    "goldmont-plus",
    "tremont",
    "nehalem",
    "corei7",
    "westmere",
    "sandybridge",
    "corei7-avx",
    "ivybridge",
    "core-avx-i",
    "haswell",
    "core-avx2",
    "broadwell",
    "skylake",
    "skylake-avx512",
    "skx",
    "cascadelake",
    "cooperlake",
    "cannonlake",
    "icelake-client",
    "rocketlake",
    "icelake-server",
    "tigerlake",
    "sapphirerapids",
    "alderlake",
    "raptorlake",
    "meteorlake",
    "arrowlake",
    "arrowlake-s",
    "lunarlake",
    "gracemont",
    "pantherlake",
    "sierraforest",
    "grandridge",
    "graniterapids",
    "graniterapids-d",
    "emeraldrapids",
    "clearwaterforest",
    "diamondrapids",
    "knl",
    "knm",
    "k8",
    "athlon64",
    "athlon-fx",
    "opteron",
    "k8-sse3",
    "athlon64-sse3",
    "opteron-sse3",
    "amdfam10",
    "barcelona",
    "btver1",
    "btver2",
    "bdver1",
    "bdver2",
    "bdver3",
    "bdver4",
    "znver1",
    "znver2",
    "znver3",
    "znver4",
    "znver5",
    "x86-64",
    "x86-64-v2",
    "x86-64-v3",
    "x86-64-v4",
}


class CompileCommand(NamedTuple):
    file: str
    directory: str
    arguments: List[str]


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--out-dir',
        '-o',
        type=Path,
        help=(
            'Where to write merged compile commands. By default, outputs are '
            'written to $BUILD_WORKSPACE_DIRECTORY/.compile_commands'
        ),
    )
    return parser.parse_args()


def main() -> int:
    """Script entry point."""
    args = _parse_args()
    workspace_root = os.environ.get("BUILD_WORKSPACE_DIRECTORY")
    if not workspace_root:
        print(
            "Error: This script must be run with 'bazel run'.", file=sys.stderr
        )
        return 1

    print("Searching for existing compile command fragments...")

    try:
        # Use `bazel info output_path` to robustly find the output directory,
        # respecting any user configuration.
        output_path_str = subprocess.check_output(
            ["bazel", "info", "output_path"],
            encoding="utf-8",
            cwd=workspace_root,
        ).strip()
        output_path = Path(output_path_str)
    except subprocess.CalledProcessError as e:
        print(f"Error getting bazel output_path: {e}", file=sys.stderr)
        return 1

    if not output_path.exists():
        print(
            f"Bazel output directory '{output_path}' not found.",
            file=sys.stderr,
        )
        print(
            "Did you run 'bazel build --config=ide //your/target' first?",
            file=sys.stderr,
        )
        return 1

    try:
        execution_root_str = subprocess.check_output(
            ["bazel", "info", "execution_root"],
            encoding="utf-8",
            cwd=workspace_root,
        ).strip()
        real_bazel_out = Path(execution_root_str) / "bazel-out"
    except subprocess.CalledProcessError as e:
        print(f"Error getting bazel execution_root: {e}", file=sys.stderr)
        return 1

    # Search for fragments with our unique suffix.
    all_fragments = list(output_path.rglob(f"*{_FRAGMENT_SUFFIX}"))

    if not all_fragments:
        print(
            "Error: Could not find any generated fragments.",
            file=sys.stderr,
        )
        return 1

    # Group fragments by platform using a more robust parsing method.
    fragments_by_platform = collections.defaultdict(list)
    for fragment in all_fragments:
        base_name = fragment.name.removesuffix(_FRAGMENT_SUFFIX)
        platform = base_name.split(".")[-1]
        fragments_by_platform[platform].append(fragment)

    print(f"Found fragments for {len(fragments_by_platform)} platform(s).")

    output_dir = args.out_dir
    if not output_dir:
        output_dir = Path(workspace_root) / ".compile_commands"
    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir()

    for platform, fragments in fragments_by_platform.items():
        print(f"Processing platform: {platform}...")
        all_commands = []
        for fragment_path in fragments:
            with open(fragment_path, "r") as f:
                try:
                    all_commands.extend(json.load(f))
                except json.JSONDecodeError:
                    print(
                        f"Warning: Could not parse {fragment_path}, skipping.",
                        file=sys.stderr,
                    )

        platform_dir = output_dir / platform
        platform_dir.mkdir()
        merged_json_path = platform_dir / "compile_commands.json"

        processed_commands = []
        for command_dict in all_commands:
            cmd = CompileCommand(
                file=command_dict["file"],
                directory=command_dict["directory"],
                arguments=command_dict["arguments"],
            )
            resolved_cmd = resolve_bazel_out_paths(cmd, real_bazel_out)
            filtered_cmd = filter_unsupported_march_args(resolved_cmd)
            processed_commands.append(filtered_cmd._asdict())

        with open(merged_json_path, "w") as f:
            json.dump(processed_commands, f, indent=2)

        with open(merged_json_path, "r") as f:
            content = f.read()
        content = content.replace("__WORKSPACE_ROOT__", workspace_root)
        with open(merged_json_path, "w") as f:
            f.write(content)

    print(f"Successfully created compilation databases in: {output_dir}")
    return 0


def resolve_bazel_out_paths(
    command: CompileCommand, real_bazel_out: Path
) -> CompileCommand:
    """Replaces bazel-out paths with their real paths."""
    marker = 'bazel-out/'
    new_args = []

    for arg in command.arguments:
        if marker in arg:
            parts = arg.split(marker, 1)
            prefix = parts[0]
            suffix = parts[1]
            new_path = real_bazel_out.joinpath(*suffix.split('/'))
            new_arg = prefix + str(new_path)
            new_args.append(new_arg)
        else:
            new_args.append(arg)

    new_file = command.file
    if command.file.startswith(marker):
        path_suffix = command.file[len(marker) :]
        new_file = str(real_bazel_out.joinpath(*path_suffix.split('/')))

    return command._replace(arguments=new_args, file=new_file)


def filter_unsupported_march_args(command: CompileCommand) -> CompileCommand:
    """Removes -march arguments if the arch is not supported by clangd."""
    new_args = []
    for arg in command.arguments:
        if arg.startswith("-march="):
            arch = arg.split("=", 1)[1]
            if arch in SUPPORTED_MARCH_ARCHITECTURES:
                new_args.append(arg)
        else:
            new_args.append(arg)
    return command._replace(arguments=new_args)


if __name__ == "__main__":
    sys.exit(main())
