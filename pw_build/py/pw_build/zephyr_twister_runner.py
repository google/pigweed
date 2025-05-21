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
"""Wrapper around Zephyr's twister script allow templating of tests"""

import argparse
import logging
from pathlib import Path
import shutil
import subprocess
import importlib.resources
import sys
from typing import Iterable
import yaml
from jsonschema import validate  # type: ignore
import pw_cli.env
import pw_cli.log

_LOG = logging.getLogger("pw_build")
_PW_ROOT: Path = pw_cli.env.pigweed_environment().PW_ROOT
_PW_PACKAGE_ROOT: Path = pw_cli.env.pigweed_environment().PW_PACKAGE_ROOT
_ENVIRONMENT_DIR = _PW_ROOT / "environment"


def get_resource_path(res: str) -> Path:
    """
    Returns the absolute path to a resource file within this package

    Args:
        res: The name of the resource file.

    Returns:
        pathlib.Path: The absolute path to the resource file
    """
    with importlib.resources.as_file(
        importlib.resources.files("pw_build") / res
    ) as path:
        return path


DEFAULT_CMAKE_FILE = get_resource_path("zephyr_default_CMakeLists.txt")

DEFAULT_PRJ_CONF_FILE = get_resource_path("zephyr_default_prj.conf")

TEMPLATE_SCHEMA = {
    "type": "object",
    "properties": {
        "name": {"type": "string"},
        "testcase": {"type": "object"},
        "file-map": {
            "type": "object",
            "additionalProperties": {"type": "string"},
        },
        "use-default-cmake": {"type": "boolean", "default": True},
        "use-default-prj-conf": {"type": "boolean", "default": True},
    },
    "required": ["name", "testcase"],
}


def handle_file_map(
    template_dir: Path, build_dir: Path, file_map: dict[str, str]
):
    """Maps source files to the build directory

    Creates symlinks in the build directory based on the file map.
    Raises an exception if a source file in the map cannot be found
    relative to the template directory.
    """
    for source, target in file_map.items():
        source_path = template_dir.absolute() / source
        link_path = build_dir / target

        if not source_path.exists():
            raise FileNotFoundError(
                f"Source '{source}' not found relative to '{template_dir}'."
            )

        # Create parent directories for the symlink if they don't exist
        link_path.parent.mkdir(parents=True, exist_ok=True)

        link_path.symlink_to(source_path)
        _LOG.debug(
            "Created symlink: %s -> %s",
            link_path,
            source_path,
        )


def process_test_template(template_file: Path, build_dir: Path) -> Path:
    """
    Processes a test template file. Reads the YAML content, validates it
    against a JSON schema, creates a directory based on 'name', and writes
    'testcase' content to testcase.yaml in the new directory.
    """
    _LOG.info(
        "Processing test template: %s, build_dir: %s",
        template_file,
        build_dir,
    )

    with template_file.open("r") as f:
        template_content = yaml.safe_load(f)
        _LOG.debug(
            "Successfully read template file: %s\nContent: %s",
            template_file,
            template_content,
        )

    validate(instance=template_content, schema=TEMPLATE_SCHEMA)

    test_dir_name = template_content["name"]
    temp_test_dir = build_dir / test_dir_name
    try:
        temp_test_dir.mkdir(parents=True, exist_ok=False)
    except FileExistsError as e:
        raise RuntimeError(
            "Directory already exists, please remove it before running"
        ) from e

    _LOG.debug("Created test directory: %s", temp_test_dir)

    testcase_content = template_content["testcase"]
    output_testcase_file = temp_test_dir / "testcase.yaml"

    with output_testcase_file.open("w") as outfile:
        yaml.dump(testcase_content, outfile)
    _LOG.debug("Wrote testcase.yaml to: %s", output_testcase_file)

    handle_file_map(
        template_dir=template_file.parent,
        build_dir=temp_test_dir,
        file_map=template_content.get("file-map", {}),
    )

    # Handle use-default-cmake
    if template_content.get("use-default-cmake", True):
        link_path = temp_test_dir / "CMakeLists.txt"
        link_path.symlink_to(DEFAULT_CMAKE_FILE.absolute())
        _LOG.debug(
            "Created symlink for default CMakeLists.txt: %s -> %s",
            link_path,
            DEFAULT_CMAKE_FILE,
        )

    # Handle use-default-prj-conf
    if template_content.get("use-default-prj-conf", True):
        link_path = temp_test_dir / "prj.conf"
        link_path.symlink_to(DEFAULT_PRJ_CONF_FILE.absolute())
        _LOG.debug(
            "Created symlink for default prj.conf: %s -> %s",
            link_path,
            DEFAULT_PRJ_CONF_FILE,
        )

    return temp_test_dir


def process_testsuite_root(root_dir: Path, build_dir: Path) -> Iterable[Path]:
    """
    Recursively searches for testcase.yaml and testtemplate.yaml files
    within the given root directory, ignoring those within subdirectories of
    PW_ROOT / 'environment'. Returns a list of parent directories containing
    test cases.
    """

    # Search for testcase.yaml files recursively
    for testcase_file in root_dir.rglob("testcase.yaml"):
        absolute_parent = testcase_file.parent.resolve()
        if not is_subdirectory(absolute_parent, _ENVIRONMENT_DIR):
            _LOG.info("Found testcase.yaml in: %s", absolute_parent)
            yield absolute_parent
        else:
            _LOG.debug(
                "Ignoring %s as its parent (%s) is within %s",
                testcase_file,
                absolute_parent,
                _ENVIRONMENT_DIR,
            )

    # Search for testtemplate.yaml files recursively
    for testtemplate_file in root_dir.rglob("testtemplate.yaml"):
        absolute_parent = testtemplate_file.parent.resolve()
        if not is_subdirectory(absolute_parent, _ENVIRONMENT_DIR):
            _LOG.info("Found testtemplate.yaml in: %s", testtemplate_file)
            processed_path = process_test_template(testtemplate_file, build_dir)
            if processed_path:
                yield processed_path
        else:
            _LOG.debug(
                "Ignoring %s as its parent (%s) is within %s",
                testtemplate_file,
                absolute_parent,
                _ENVIRONMENT_DIR,
            )


def is_subdirectory(child: Path, parent: Path) -> bool:
    """
    Checks if the 'child' path is a subdirectory of the 'parent' path.
    """
    try:
        child.relative_to(parent)
        return True
    except ValueError:
        return False


def main():
    """Main entry point for the file"""
    parser = argparse.ArgumentParser(
        description="Process test suite paths and forward arguments to twister."
    )
    parser.add_argument(
        "-T",
        "--testsuite-root",
        type=Path,
        action="append",
        help=(
            "Root directory to search for testcase.yaml or testtemplate.yaml "
            "files. Can be specified multiple times."
        ),
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=_PW_ROOT / "twister-template-build",
        help=(
            "Build directory. Defaults to "
            f"{_PW_ROOT / 'twister-template-build'}."
        ),
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="count",
        default=0,
        help="Increase verbosity level. Can be specified up to three times.",
    )
    parser.add_argument(
        "-c",
        "--clobber-output",
        action="store_true",
        help=(
            "Delete the build directory before processing. Without this, if the"
            " build directory already exists the script will fail."
        ),
    )

    # Use parse_known_args to capture defined and undefined arguments
    args, unknown_args = parser.parse_known_args()

    # Set logging level based on verbosity
    log_levels = {
        0: logging.ERROR,
        1: logging.WARNING,
        2: logging.INFO,
        3: logging.DEBUG,
    }
    log_level = log_levels.get(
        args.verbose, logging.DEBUG
    )  # Default to DEBUG if more than 3
    pw_cli.log.install(
        level=log_level,
        use_color=True,
        hide_timestamp=False,
        logger=_LOG,
    )

    # Process the test suite root paths
    test_suite_roots = args.testsuite_root if args.testsuite_root else []
    test_cases = []

    # Conditionally delete the build_dir
    if (
        args.clobber_output
        and args.build_dir.exists()
        and args.build_dir.is_dir()
    ):
        _LOG.debug("Clobbering build directory: %s", args.build_dir)
        shutil.rmtree(args.build_dir)

    for root_dir in test_suite_roots:
        test_cases.extend(
            process_testsuite_root(
                root_dir=root_dir,
                build_dir=args.build_dir,
            )
        )
    if not test_cases:
        _LOG.info("No test cases found")
        return

    _LOG.info("Test suite roots: [%s]", ", ".join(str(d) for d in test_cases))

    # Prepare the command for twister
    twister = _PW_PACKAGE_ROOT / "zephyr" / "scripts" / "twister"
    twister_command = [sys.executable, str(twister)]
    twister_command.extend(
        args for dir in test_cases for args in ("--testsuite-root", str(dir))
    )
    if args.verbose > 0:
        twister_command.extend(["-v"] * args.verbose)
    if args.clobber_output:
        twister_command.append("--clobber-output")
    twister_command.extend([*unknown_args])
    _LOG.debug("Executing: [%s]", ", ".join(twister_command))
    subprocess.run(twister_command, check=True)


if __name__ == "main":
    main()
