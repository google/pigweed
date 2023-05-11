#!/usr/bin/env python3
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
"""Arduino Core Installer."""

import argparse
import importlib.resources
import logging
import os
from pathlib import Path
import platform
import shutil
from typing import Dict

from pw_arduino_build import file_operations

_LOG = logging.getLogger(__name__)


class ArduinoCoreNotSupported(Exception):
    """Exception raised when a given core can not be installed."""


class ArduinoCoreInstallationFailed(Exception):
    """Exception raised when a given core failed to be installed."""


_ARDUINO_CORE_ARTIFACTS: Dict[str, Dict] = {
    # pylint: disable=line-too-long
    "teensy": {
        "all": {
            "core": {
                "version": "1.58.1",
                "url": "https://www.pjrc.com/teensy/td_158-1/teensy-package.tar.bz2",
                "file_name": "teensy-package.tar.bz2",
                "sha256": "3a3f3728045621d25068c5b5dfc24bf171550127e9fae4d0e8be2574c6636cff",
            }
        },
        "Linux": {
            "teensy-tools": {
                "url": "https://www.pjrc.com/teensy/td_158/teensy-tools-linux64.tar.bz2",
                "file_name": "teensy-tools-linux64.tar.bz2",
                "sha256": "0a272575ca42b4532ab89516df160e1d68e449fe1538c0bd71dbb768f1b3c0b6",
                "version": "1.58.0",
            },
        },
        # TODO(tonymd): Handle 32-bit Linux Install?
        "Linux32": {
            "teensy-tools": {
                "url": "https://www.pjrc.com/teensy/td_158/teensy-tools-linux32.tar.bz2",
                "file_name": "teensy-tools-linux32.tar.bz2",
                "sha256": "995d974935c8118ad6d4c191206453dd8f57c1e299264bb4cffcc62c96c6d077",
                "version": "1.58.0",
            },
        },
        # TODO(tonymd): Handle ARM32 (Raspberry Pi) Install?
        "LinuxARM32": {
            "teensy-tools": {
                "url": "https://www.pjrc.com/teensy/td_158/teensy-tools-linuxarm.tar.bz2",
                "file_name": "teensy-tools-linuxarm.tar.bz2",
                "sha256": "88cf8e55549f5d5937fa7dbc763cad49bd3680d4e5185b318c667f541035e633",
                "version": "1.58.0",
            },
        },
        # TODO(tonymd): Handle ARM64 Install?
        "LinuxARM64": {
            "teensy-tools": {
                "url": "https://www.pjrc.com/teensy/td_158/teensy-tools-linuxaarch64.tar.bz2",
                "file_name": "teensy-tools-linuxaarch64.tar.bz2",
                "sha256": "a20b1c5e91fe51c3b6591e4cfcf711d4a4c0a0bb5120c59d1c8dd8d32ae44e31",
                "version": "1.58.0",
            },
        },
        "Darwin": {
            "teensy-tools": {
                "url": "https://www.pjrc.com/teensy/td_158/teensy-tools-macos.tar.bz2",
                "file_name": "teensy-tools-macos.tar.bz2",
                "sha256": "d386412e38fe6dd6c5d849c2b1f8eea00cbf7bc3659fb6ba9f83cebfb736924b",
                "version": "1.58.0",
            },
        },
        "Windows": {
            "teensy-tools": {
                "url": "https://www.pjrc.com/teensy/td_158/teensy-tools-windows.tar.bz2",
                "file_name": "teensy-tools-windows.tar.bz2",
                "sha256": "206315ddc82381d2da92da9f633a1719e00c0e8f5432acfed434573409a48de1",
                "version": "1.58.0",
            },
        },
    },
    "adafruit-samd": {
        "all": {
            "core": {
                "version": "1.6.2",
                "url": "https://github.com/adafruit/ArduinoCore-samd/archive/1.6.2.tar.gz",
                "file_name": "adafruit-samd-1.6.2.tar.gz",
                "sha256": "5875f5bc05904c10e6313f02653f28f2f716db639d3d43f5a1d8a83d15339d64",
            }
        },
        "Linux": {},
        "Darwin": {},
        "Windows": {},
    },
    "arduino-samd": {
        "all": {
            "core": {
                "version": "1.8.8",
                "url": "http://downloads.arduino.cc/cores/samd-1.8.8.tar.bz2",
                "file_name": "arduino-samd-1.8.8.tar.bz2",
                "sha256": "7b93eb705cba9125d9ee52eba09b51fb5fe34520ada351508f4253abbc9f27fa",
            }
        },
        "Linux": {},
        "Darwin": {},
        "Windows": {},
    },
    "stm32duino": {
        "all": {
            "core": {
                "version": "1.9.0",
                "url": "https://github.com/stm32duino/Arduino_Core_STM32/archive/1.9.0.tar.gz",
                "file_name": "stm32duino-1.9.0.tar.gz",
                "sha256": "4f75ba7a117d90392e8f67c58d31d22393749b9cdd3279bc21e7261ec06c62bf",
            }
        },
        "Linux": {},
        "Darwin": {},
        "Windows": {},
    },
}


def install_core_command(args: argparse.Namespace):
    install_core(args.prefix, args.core_name)


def install_core(prefix, core_name):
    install_prefix = os.path.realpath(
        os.path.expanduser(os.path.expandvars(prefix))
    )
    install_dir = os.path.join(install_prefix, core_name)
    cache_dir = os.path.join(install_prefix, ".cache", core_name)

    if core_name in supported_cores():
        shutil.rmtree(install_dir, ignore_errors=True)
        os.makedirs(install_dir, exist_ok=True)
        os.makedirs(cache_dir, exist_ok=True)

    if core_name == "teensy":
        install_teensy_core(install_prefix, install_dir, cache_dir)
        apply_teensy_patches(install_dir)
    elif core_name == "adafruit-samd":
        install_adafruit_samd_core(install_prefix, install_dir, cache_dir)
    elif core_name == "stm32duino":
        install_stm32duino_core(install_prefix, install_dir, cache_dir)
    elif core_name == "arduino-samd":
        install_arduino_samd_core(install_prefix, install_dir, cache_dir)
    else:
        raise ArduinoCoreNotSupported(
            "Invalid core '{}'. Supported cores: {}".format(
                core_name, ", ".join(supported_cores())
            )
        )


def supported_cores():
    return _ARDUINO_CORE_ARTIFACTS.keys()


def install_teensy_core(_install_prefix: str, install_dir: str, cache_dir: str):
    """Install teensy core files and tools."""
    # Install the Teensy core source files
    artifacts = _ARDUINO_CORE_ARTIFACTS["teensy"]["all"]["core"]
    core_tarfile = file_operations.download_to_cache(
        url=artifacts["url"],
        expected_sha256sum=artifacts["sha256"],
        cache_directory=cache_dir,
        downloaded_file_name=artifacts["file_name"],
    )

    package_path = os.path.join(
        install_dir, "hardware", "avr", artifacts["version"]
    )
    os.makedirs(package_path, exist_ok=True)
    file_operations.extract_archive(core_tarfile, package_path, cache_dir)

    expected_files = [
        Path(package_path) / 'boards.txt',
        Path(package_path) / 'platform.txt',
    ]

    if any(not expected_file.is_file() for expected_file in expected_files):
        expected_files_str = "".join(
            list(f"  {expected_file}\n" for expected_file in expected_files)
        )

        raise ArduinoCoreInstallationFailed(
            "\n\nError: Installation Failed.\n"
            "Please remove the package:\n\n"
            "  pw package remove teensy\n\n"
            "Try again and ensure the following files exist:\n\n"
            + expected_files_str
        )

    teensy_tools = _ARDUINO_CORE_ARTIFACTS["teensy"][platform.system()]

    for tool_name, artifacts in teensy_tools.items():
        tool_tarfile = file_operations.download_to_cache(
            url=artifacts["url"],
            expected_sha256sum=artifacts["sha256"],
            cache_directory=cache_dir,
            downloaded_file_name=artifacts["file_name"],
        )

        tool_path = os.path.join(
            install_dir, "tools", tool_name, artifacts["version"]
        )

        os.makedirs(tool_path, exist_ok=True)
        file_operations.extract_archive(tool_tarfile, tool_path, cache_dir)

    return True


def apply_teensy_patches(install_dir):
    # On Mac the "hardware" directory is a symlink:
    #   ls -l third_party/arduino/cores/teensy/
    #   hardware -> Teensyduino.app/Contents/Java/hardware
    # Resolve paths since `git apply` doesn't work if a path is beyond a
    # symbolic link.
    patch_root_path = (
        Path(install_dir) / "hardware/avr/1.58.1/cores"
    ).resolve()

    # Get all *.diff files for the teensy core.
    patches_python_package = 'pw_arduino_build.core_patches.teensy'

    patch_file_names = sorted(
        patch
        for patch in importlib.resources.contents(patches_python_package)
        if Path(patch).suffix in ['.diff']
    )

    # Apply each patch file.
    for diff_name in patch_file_names:
        with importlib.resources.path(
            patches_python_package, diff_name
        ) as diff_path:
            file_operations.git_apply_patch(
                patch_root_path.as_posix(),
                diff_path.as_posix(),
                unsafe_paths=True,
            )


def install_arduino_samd_core(
    install_prefix: str, install_dir: str, cache_dir: str
):
    artifacts = _ARDUINO_CORE_ARTIFACTS["arduino-samd"]["all"]["core"]
    core_tarfile = file_operations.download_to_cache(
        url=artifacts["url"],
        expected_sha256sum=artifacts["sha256"],
        cache_directory=cache_dir,
        downloaded_file_name=artifacts["file_name"],
    )

    package_path = os.path.join(
        install_dir, "hardware", "samd", artifacts["version"]
    )
    os.makedirs(package_path, exist_ok=True)
    file_operations.extract_archive(core_tarfile, package_path, cache_dir)
    original_working_dir = os.getcwd()
    os.chdir(install_prefix)
    # TODO(tonymd): Fetch core/tools as specified by:
    # http://downloads.arduino.cc/packages/package_index.json
    os.chdir(original_working_dir)
    return True


def install_adafruit_samd_core(
    install_prefix: str, install_dir: str, cache_dir: str
):
    artifacts = _ARDUINO_CORE_ARTIFACTS["adafruit-samd"]["all"]["core"]
    core_tarfile = file_operations.download_to_cache(
        url=artifacts["url"],
        expected_sha256sum=artifacts["sha256"],
        cache_directory=cache_dir,
        downloaded_file_name=artifacts["file_name"],
    )

    package_path = os.path.join(
        install_dir, "hardware", "samd", artifacts["version"]
    )
    os.makedirs(package_path, exist_ok=True)
    file_operations.extract_archive(core_tarfile, package_path, cache_dir)

    original_working_dir = os.getcwd()
    os.chdir(install_prefix)
    # TODO(tonymd): Fetch platform specific tools as specified by:
    # https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
    # Specifically:
    #   https://github.com/ARM-software/CMSIS_5/archive/5.4.0.tar.gz
    os.chdir(original_working_dir)
    return True


def install_stm32duino_core(install_prefix, install_dir, cache_dir):
    artifacts = _ARDUINO_CORE_ARTIFACTS["stm32duino"]["all"]["core"]
    core_tarfile = file_operations.download_to_cache(
        url=artifacts["url"],
        expected_sha256sum=artifacts["sha256"],
        cache_directory=cache_dir,
        downloaded_file_name=artifacts["file_name"],
    )

    package_path = os.path.join(
        install_dir, "hardware", "stm32", artifacts["version"]
    )
    os.makedirs(package_path, exist_ok=True)
    file_operations.extract_archive(core_tarfile, package_path, cache_dir)
    original_working_dir = os.getcwd()
    os.chdir(install_prefix)
    # TODO(tonymd): Fetch platform specific tools as specified by:
    # https://github.com/stm32duino/BoardManagerFiles/raw/HEAD/STM32/package_stm_index.json
    os.chdir(original_working_dir)
    return True
