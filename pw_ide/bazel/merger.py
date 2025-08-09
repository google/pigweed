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

import collections
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

# A unique suffix to identify fragments created by our aspect.
_FRAGMENT_SUFFIX = ".pw_aspect.compile_commands.json"


def main() -> int:
    """Script entry point."""
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

        with open(merged_json_path, "w") as f:
            json.dump(all_commands, f, indent=2)

        with open(merged_json_path, "r") as f:
            content = f.read()
        content = content.replace("__WORKSPACE_ROOT__", workspace_root)
        with open(merged_json_path, "w") as f:
            f.write(content)

    print(f"Successfully created compilation databases in: {output_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
