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
"""Cleans previously generated compile command fragments."""

import os
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

    try:
        # Use `bazel info output_path` to robustly find the output directory.
        output_path_str = subprocess.check_output(
            ["bazel", "info", "output_path"],
            encoding="utf-8",
            cwd=workspace_root,
        ).strip()
        output_path = Path(output_path_str)
    except subprocess.CalledProcessError:
        # This can happen if a build has never been run. It's not an error.
        print("Bazel output directory not found. No fragments to clean.")
        return 0

    if not output_path.exists():
        print(
            f"Bazel output directory '{output_path}' not found. "
            "No fragments to clean."
        )
        return 0

    fragments_to_delete = list(output_path.rglob(f"*{_FRAGMENT_SUFFIX}"))

    if not fragments_to_delete:
        print("No compile command fragments found to clean.")
        return 0

    print(f"Found {len(fragments_to_delete)} fragments to delete...")
    for fragment in fragments_to_delete:
        try:
            fragment.unlink()
        except OSError as e:
            print(f"Error deleting {fragment}: {e}", file=sys.stderr)

    print("Successfully deleted old fragments.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
