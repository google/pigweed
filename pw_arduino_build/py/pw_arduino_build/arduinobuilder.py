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
"""Extracts build information from Arduino cores."""

import argparse
import glob
import hashlib
import logging
import os
import platform
import pprint
import re
import shutil
import shlex
import stat
import sys
import tarfile
import time
import urllib.request
import zipfile
from collections import OrderedDict
from pathlib import Path
from typing import List, Dict

_LOG = logging.getLogger(__name__)
_STDERR_HANDLER = logging.StreamHandler()

_pretty_print = pprint.PrettyPrinter(indent=1, width=120).pprint
_pretty_format = pprint.PrettyPrinter(indent=1, width=120).pformat
# yapf: disable
_ARDUINO_CORE_ARTIFACTS = {
    # pylint: disable=line-too-long
    "teensy": {
        "Linux": {
            "arduino-ide": {
                "url": "https://downloads.arduino.cc/arduino-1.8.13-linux64.tar.xz",
                "file_name": "arduino-1.8.13-linux64.tar.xz",
                "sha256": "1b20d0ec850a2a63488009518725f058668bb6cb48c321f82dcf47dc4299b4ad",
            },
            "teensyduino": {
                "url": "https://www.pjrc.com/teensy/td_153/TeensyduinoInstall.linux64",
                "file_name": "TeensyduinoInstall.linux64",
                "sha256": "2e6cd99a757bc80593ea3de006de4cc934bcb0a6ec74cad8ec327f0289d40f0b",
            },
        },
        # TODO(tonymd): Handle 32-bit Linux Install?
        "Linux32": {
            "arduino-ide": {
                "url": "https://downloads.arduino.cc/arduino-1.8.13-linux32.tar.xz",
                "file_name": "arduino-1.8.13-linux32.tar.xz",
                "sha256": "",
            },
            "teensyduino": {
                "url": "https://www.pjrc.com/teensy/td_153/TeensyduinoInstall.linux32",
                "file_name": "TeensyduinoInstall.linux32",
                "sha256": "",
            },
        },
        # TODO(tonymd): Handle ARM32 (Raspberry Pi) Install?
        "LinuxARM32": {
            "arduino-ide": {
                "url": "https://downloads.arduino.cc/arduino-1.8.13-linuxarm.tar.xz",
                "file_name": "arduino-1.8.13-linuxarm.tar.xz",
                "sha256": "",
            },
            "teensyduino": {
                "url": "https://www.pjrc.com/teensy/td_153/TeensyduinoInstall.linuxarm",
                "file_name": "TeensyduinoInstall.linuxarm",
                "sha256": "",
            },
        },
        # TODO(tonymd): Handle ARM64 Install?
        "LinuxARM64": {
            "arduino-ide": {
                "url": "https://downloads.arduino.cc/arduino-1.8.13-linuxaarch64.tar.xz",
                "file_name": "arduino-1.8.13-linuxaarch64.tar.xz",
                "sha256": "",
            },
            "teensyduino": {
                "url": "https://www.pjrc.com/teensy/td_153/TeensyduinoInstall.linuxaarch64",
                "file_name": "TeensyduinoInstall.linuxaarch64",
                "sha256": "",
            },
        },
        "Darwin": {
            "teensyduino": {
                "url": "https://www.pjrc.com/teensy/td_153/Teensyduino_MacOS_Catalina.zip",
                "file_name": "Teensyduino_MacOS_Catalina.zip",
                "sha256": "401ef42c6e83e621cdda20191a4ef9b7db8a214bede5a94a9e26b45f79c64fe2",
            },
        },
        "Windows": {
            "arduino-ide": {
                "url": "https://downloads.arduino.cc/arduino-1.8.13-windows.zip",
                "file_name": "arduino-1.8.13-windows.zip",
                "sha256": "78d3e96827b9e9b31b43e516e601c38d670d29f12483e88cbf6d91a0f89ef524",
            },
            "teensyduino": {
                "url": "https://www.pjrc.com/teensy/td_153/TeensyduinoInstall.exe",
                "file_name": "TeensyduinoInstall.exe",
                "sha256": "88f58681e5c4772c54e462bc88280320e4276e5b316dcab592fe38d96db990a1",
            },
        }
    },
    "adafruit-samd": {
        "all": {
            "core": {
                "url": "https://github.com/adafruit/ArduinoCore-samd/archive/1.6.2.tar.gz",
                "sha256": "5875f5bc05904c10e6313f02653f28f2f716db639d3d43f5a1d8a83d15339d64",
            }
        },
        "Linux": {},
        "Darwin": {},
        "Windows": {},
    },
    "stm32duino": {
        "all": {
            "core": {
                "url": "https://github.com/stm32duino/Arduino_Core_STM32/archive/1.9.0.tar.gz",
                "sha256": "4f75ba7a117d90392e8f67c58d31d22393749b9cdd3279bc21e7261ec06c62bf",
            }
        },
        "Linux": {},
        "Darwin": {},
        "Windows": {},
    },
} # type: Dict[str, Dict]
# yapf: enable


def arduino_runtime_os_string():
    arduno_platform = {
        "Linux": "linux",
        "Windows": "windows",
        "Darwin": "macosx"
    }
    return arduno_platform[platform.system()]


class FileOperations:
    """File helper functions."""
    @staticmethod
    def find_files(starting_dir: str,
                   patterns: List[str],
                   directories_only=False) -> List[str]:
        # ["**/*.S", "**/*.ino", "**/*.h", "**/*.c", "**/*.cpp"]

        original_working_dir = os.getcwd()
        if not (os.path.exists(starting_dir) and os.path.isdir(starting_dir)):
            _LOG.error("Directory '%s' does not exist.", starting_dir)
            raise FileNotFoundError

        os.chdir(starting_dir)
        files = []
        for pattern in patterns:
            for file_path in glob.glob(pattern, recursive=True):
                if not directories_only or (directories_only
                                            and os.path.isdir(file_path)):
                    files.append(file_path)
        os.chdir(original_working_dir)
        return sorted(files)

    @staticmethod
    def sha256_sum(file_name):
        hash_sha256 = hashlib.sha256()
        with open(file_name, "rb") as file_handle:
            for chunk in iter(lambda: file_handle.read(4096), b""):
                hash_sha256.update(chunk)
        return hash_sha256.hexdigest()

    @staticmethod
    def md5_sum(file_name):
        hash_md5 = hashlib.md5()
        with open(file_name, "rb") as file_handle:
            for chunk in iter(lambda: file_handle.read(4096), b""):
                hash_md5.update(chunk)
        return hash_md5.hexdigest()

    @staticmethod
    def verify_file_checksum(file_path,
                             expected_checksum,
                             sum_function=sha256_sum):
        downloaded_checksum = sum_function(file_path)
        if downloaded_checksum != expected_checksum:
            _LOG.error("Error: Invalid %s", sum_function.__name__)
            _LOG.error("%s %s", downloaded_checksum,
                       os.path.basename(file_path))
            _LOG.error("%s (expected)", expected_checksum)
            return sys.exit(1)

        _LOG.info("  %s:", sum_function.__name__)
        _LOG.info("  %s %s", downloaded_checksum, os.path.basename(file_path))
        return True

    @staticmethod
    def download_to_cache(url: str,
                          expected_md5sum=None,
                          expected_sha256sum=None,
                          cache_directory=".cache") -> str:

        cache_dir = os.path.realpath(
            os.path.expanduser(os.path.expandvars(cache_directory)))
        downloaded_file = os.path.join(cache_dir, url.split("/")[-1])

        if not os.path.exists(downloaded_file):
            _LOG.info("Downloading: %s", url)
            urllib.request.urlretrieve(url, filename=downloaded_file)

        if os.path.exists(downloaded_file):
            _LOG.info("Downloaded: %s", downloaded_file)
            if expected_sha256sum:
                FileOperations.verify_file_checksum(
                    downloaded_file,
                    expected_sha256sum,
                    sum_function=FileOperations.sha256_sum)
            elif expected_md5sum:
                FileOperations.verify_file_checksum(
                    downloaded_file,
                    expected_md5sum,
                    sum_function=FileOperations.md5_sum)

        return downloaded_file

    @staticmethod
    def extract_zipfile(archive_file: str, dest_dir: str):
        with zipfile.ZipFile(archive_file) as archive:
            archive.extractall(path=dest_dir)

    @staticmethod
    def extract_tarfile(archive_file: str, dest_dir: str):
        with tarfile.open(archive_file, 'r') as archive:
            archive.extractall(path=dest_dir)

    @staticmethod
    def extract_archive(archive_file: str,
                        dest_dir: str,
                        cache_dir: str,
                        remove_single_toplevel_folder=True):
        """Extract a tar or zip file.

        Args:
            archive_file (str): Absolute path to the archive file.
            dest_dir (str): Extraction destination directory.
            cache_dir (str): Directory where temp files can be created.
            remove_single_toplevel_folder (bool): If the archive contains only a
                single folder move the contents of that into the destination
                directory.
        """
        # Make a temporary directory to extract files into
        temp_extract_dir = os.path.join(cache_dir,
                                        "." + os.path.basename(archive_file))
        os.makedirs(temp_extract_dir, exist_ok=True)

        _LOG.info("Extracting: %s", archive_file)
        if zipfile.is_zipfile(archive_file):
            FileOperations.extract_zipfile(archive_file, temp_extract_dir)
        elif tarfile.is_tarfile(archive_file):
            FileOperations.extract_tarfile(archive_file, temp_extract_dir)
        else:
            _LOG.error("Unknown archive format: %s", archive_file)
            return sys.exit(1)

        _LOG.info("Installing into: %s", dest_dir)
        path_to_extracted_files = temp_extract_dir

        extracted_top_level_files = os.listdir(temp_extract_dir)
        # Check if tarfile has only one folder
        # If yes, make that the new path_to_extracted_files
        if remove_single_toplevel_folder and len(
                extracted_top_level_files) == 1:
            path_to_extracted_files = os.path.join(
                temp_extract_dir, extracted_top_level_files[0])

        # Move extracted files to dest_dir
        extracted_files = os.listdir(path_to_extracted_files)
        for file_name in extracted_files:
            source_file = os.path.join(path_to_extracted_files, file_name)
            dest_file = os.path.join(dest_dir, file_name)
            shutil.move(source_file, dest_file)

        # rm -rf temp_extract_dir
        shutil.rmtree(temp_extract_dir, ignore_errors=True)

        # Return List of extracted files
        return list(Path(dest_dir).rglob("*"))

    @staticmethod
    def remove_empty_directories(directory):
        """Recursively remove empty directories."""

        for path in sorted(Path(directory).rglob("*"), reverse=True):
            # If broken symlink
            if path.is_symlink() and not path.exists():
                path.unlink()
            # if empty directory
            elif path.is_dir() and len(os.listdir(path)) == 0:
                path.rmdir()


class ArduinoCoreInstaller:
    """Simple Arduino core installer."""
    @staticmethod
    def install_teensy_core_windows(install_prefix, install_dir, cache_dir):
        """Download and install Teensyduino artifacts for Windows."""
        teensy_artifacts = _ARDUINO_CORE_ARTIFACTS["teensy"][platform.system()]

        arduino_artifact = teensy_artifacts["arduino-ide"]
        arduino_zipfile = FileOperations.download_to_cache(
            url=arduino_artifact["url"],
            expected_sha256sum=arduino_artifact["sha256"],
            cache_directory=cache_dir)

        teensyduino_artifact = teensy_artifacts["teensyduino"]
        teensyduino_installer = FileOperations.download_to_cache(
            url=teensyduino_artifact["url"],
            expected_sha256sum=teensyduino_artifact["sha256"],
            cache_directory=cache_dir)

        FileOperations.extract_archive(arduino_zipfile, install_dir, cache_dir)

        # "teensy" here should match args.core_name
        teensy_core_dir = os.path.join(install_prefix, "teensy")

        # Change working directory for installation
        original_working_dir = os.getcwd()
        os.chdir(install_prefix)

        _LOG.info("Installing Teensyduino to: %s", teensy_core_dir)

        install_command = "{} \"--dir={}\"".format(teensyduino_installer,
                                                   "teensy")
        _LOG.info("  Running: %s", install_command)
        _LOG.info("  Please click yes on the Windows 'User Account Control' "
                  "dialog.")
        _LOG.info("  You should see: 'Verified publisher: PRJC.COM LLC'")

        os.system(install_command)
        if not os.path.exists(
                os.path.join(teensy_core_dir, "hardware", "teensy")):
            _LOG.error(
                "Error: Installation Failed.\n"
                "Please try again and ensure Teensyduino is installed in "
                "the folder:\n"
                "%s", teensy_core_dir)
            sys.exit(1)
        else:
            _LOG.info("  Install complete!")

        FileOperations.remove_empty_directories(install_dir)
        os.chdir(original_working_dir)

    @staticmethod
    def install_teensy_core_mac(unused_install_prefix, install_dir, cache_dir):
        """Download and install Teensyduino artifacts for Mac."""
        teensy_artifacts = _ARDUINO_CORE_ARTIFACTS["teensy"][platform.system()]

        teensyduino_artifact = teensy_artifacts["teensyduino"]
        teensyduino_zip = FileOperations.download_to_cache(
            url=teensyduino_artifact["url"],
            expected_sha256sum=teensyduino_artifact["sha256"],
            cache_directory=cache_dir)

        extracted_files = FileOperations.extract_archive(
            teensyduino_zip,
            install_dir,
            cache_dir,
            remove_single_toplevel_folder=False)
        toplevel_folder = sorted(extracted_files)[0]
        os.symlink(os.path.join(toplevel_folder, "Contents", "Java",
                                "hardware"),
                   os.path.join(install_dir, "hardware"),
                   target_is_directory=True)

    @staticmethod
    def install_teensy_core_linux(install_prefix, install_dir, cache_dir):
        """Download and install Teensyduino artifacts for Windows."""
        teensy_artifacts = _ARDUINO_CORE_ARTIFACTS["teensy"][platform.system()]

        arduino_artifact = teensy_artifacts["arduino-ide"]
        arduino_tarfile = FileOperations.download_to_cache(
            url=arduino_artifact["url"],
            expected_sha256sum=arduino_artifact["sha256"],
            cache_directory=cache_dir)

        teensyduino_artifact = teensy_artifacts["teensyduino"]
        teensyduino_installer = FileOperations.download_to_cache(
            url=teensyduino_artifact["url"],
            expected_sha256sum=teensyduino_artifact["sha256"],
            cache_directory=cache_dir)

        extracted_files = FileOperations.extract_archive(
            arduino_tarfile, install_dir, cache_dir)
        os.chmod(teensyduino_installer,
                 os.stat(teensyduino_installer).st_mode | stat.S_IEXEC)

        original_working_dir = os.getcwd()
        os.chdir(install_prefix)
        # "teensy" here should match args.core_name
        os.system("{} --dir={}".format(teensyduino_installer, "teensy"))

        # Remove original arduino IDE files
        for efile in extracted_files:
            if efile.is_file():
                efile.unlink()

        FileOperations.remove_empty_directories(install_dir)
        os.chdir(original_working_dir)

    @staticmethod
    def install_arduino_samd_core(install_prefix: str, install_dir: str,
                                  cache_dir: str):
        # TODO(tonymd): Fetch core/tools as specified by:
        # http://downloads.arduino.cc/packages/package_index.json
        pass

    @staticmethod
    def install_adafruit_samd_core(install_prefix: str, install_dir: str,
                                   cache_dir: str):
        artifacts = _ARDUINO_CORE_ARTIFACTS["adafruit-samd"]["all"]["core"]
        core_tarfile = FileOperations.download_to_cache(
            url=artifacts["url"],
            expected_sha256sum=artifacts["sha256"],
            cache_directory=cache_dir)

        package_path = os.path.join(
            install_dir, "hardware", "samd",
            os.path.basename(core_tarfile).replace(".tar.gz", ""))
        os.makedirs(package_path, exist_ok=True)
        FileOperations.extract_archive(core_tarfile, package_path, cache_dir)

        original_working_dir = os.getcwd()
        os.chdir(install_prefix)
        # TODO(tonymd): Fetch platform specific tools as specified by:
        # https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
        # Specifically:
        #   https://github.com/ARM-software/CMSIS_5/archive/5.4.0.tar.gz
        os.chdir(original_working_dir)
        return True

    @staticmethod
    def install_stm32duino_core(install_prefix, install_dir, cache_dir):
        artifacts = _ARDUINO_CORE_ARTIFACTS["stm32duino"]["all"]["core"]
        core_tarfile = FileOperations.download_to_cache(
            url=artifacts["url"],
            expected_sha256sum=artifacts["sha256"],
            cache_directory=cache_dir)

        package_path = os.path.join(
            install_dir, "hardware", "stm32",
            os.path.basename(core_tarfile).replace(".tar.gz", ""))
        os.makedirs(package_path, exist_ok=True)
        FileOperations.extract_archive(core_tarfile, package_path, cache_dir)
        original_working_dir = os.getcwd()
        os.chdir(install_prefix)
        # TODO(tonymd): Fetch platform specific tools as specified by:
        # https://github.com/stm32duino/BoardManagerFiles/raw/master/STM32/package_stm_index.json
        os.chdir(original_working_dir)
        return True


class ArduinoBuilder:
    """Used to interpret arduino boards.txt and platform.txt files."""
    # pylint: disable=too-many-instance-attributes,too-many-public-methods

    board_menu_regex = re.compile(
        r"^(?P<name>menu\.[^#=]+)=(?P<description>.*)$", re.MULTILINE)

    board_name_regex = re.compile(
        r"^(?P<name>[^\s#\.]+)\.name=(?P<description>.*)$", re.MULTILINE)

    variable_regex = re.compile(r"^(?P<name>[^\s#=]+)=(?P<value>.*)$",
                                re.MULTILINE)

    menu_option_regex = re.compile(
        r"^menu\."  # starts with "menu"
        r"(?P<menu_option_name>[^.]+)\."  # first token after .
        r"(?P<menu_option_value>[^.]+)$")  # second (final) token after .

    tool_name_regex = re.compile(
        r"^tools\."  # starts with "tools"
        r"(?P<tool_name>[^.]+)\.")  # first token after .

    interpolated_variable_regex = re.compile(r"{[^}]+}", re.MULTILINE)

    objcopy_step_name_regex = re.compile(r"^recipe.objcopy.([^.]+).pattern$")

    def __init__(self,
                 arduino_path,
                 package_name,
                 build_path=None,
                 project_path=None,
                 project_source_path=None,
                 build_project_name=None,
                 compiler_path_override=False):
        self.arduino_path = arduino_path
        self.arduino_package_name = package_name
        self.selected_board = None
        self.build_path = build_path
        self.project_path = project_path
        self.project_source_path = project_source_path
        self.build_project_name = build_project_name
        self.compiler_path_override = compiler_path_override
        self.variant_includes = ""
        self.build_variant_path = False

        self.compiler_path_override_binaries = []
        if self.compiler_path_override:
            self.compiler_path_override_binaries = FileOperations.find_files(
                self.compiler_path_override, "*")

        # Container dicts for boards.txt and platform.txt file data.
        self.board = OrderedDict()
        self.platform = OrderedDict()
        self.menu_options = OrderedDict({
            "global_options": {},
            "default_board_values": {},
            "selected": {}
        })
        self.tools_variables = {}

        # Set and check for valid hardware folder.
        self.hardware_path = os.path.join(self.arduino_path, "hardware")

        if not os.path.exists(self.hardware_path):
            _LOG.error("Error: Arduino package path '%s' does not exist.",
                       self.arduino_path)
            raise FileNotFoundError

        # Set and check for valid package name
        self.package_path = os.path.join(self.arduino_path, "hardware",
                                         package_name)
        # {build.arch} is the first folder name of the package (upcased)
        self.build_arch = os.path.split(package_name)[0].upper()

        if not os.path.exists(self.package_path):
            _LOG.error("Error: Arduino package name '%s' does not exist.",
                       package_name)
            _LOG.error("Did you mean:\n")
            # TODO(tonymd): On Windows concatenating "/" may not work
            possible_alternatives = [
                d.replace(self.hardware_path + os.sep, "", 1)
                for d in glob.glob(self.hardware_path + "/*/*")
            ]
            _LOG.error("\n".join(possible_alternatives))
            sys.exit(1)

        # Grab all folder names in the cores directory. These are typically
        # sub-core source files.
        self.sub_core_folders = os.listdir(
            os.path.join(self.package_path, "cores"))

        self._find_tools_variables()

        self.boards_txt = os.path.join(self.package_path, "boards.txt")
        self.platform_txt = os.path.join(self.package_path, "platform.txt")

    def select_board(self, board_name, menu_option_overrides=False):
        self.selected_board = board_name

        # Load default menu options for a selected board.
        if not self.selected_board in self.board.keys():
            _LOG.error("Error board: '%s' not supported.", self.selected_board)
            # TODO(tonymd): Print supported boards here
            sys.exit(1)

        # Override default menu options if any are specified.
        if menu_option_overrides:
            for moption in menu_option_overrides:
                if not self.set_menu_option(moption):
                    # TODO(tonymd): Print supported menu options here
                    sys.exit(1)

        self._copy_default_menu_options_to_build_variables()
        self._apply_recipe_overrides()
        self._substitute_variables()

    def _apply_recipe_overrides(self):
        # Override link recipes with per-core exceptions
        # Teensyduino cores
        if self.build_arch == 'TEENSY':
            # Change {build.path}/{archive_file}
            # To {archive_file_path} (which should contain the core.a file)
            new_link_line = self.platform["recipe.c.combine.pattern"].replace(
                "{object_files} \"{build.path}/{archive_file}\"",
                "{object_files} {archive_file_path}", 1)
            # Add the teensy provided toolchain lib folder for link access to
            # libarm_cortexM*_math.a
            new_link_line = new_link_line.replace(
                "\"-L{build.path}\"",
                "\"-L{build.path}\" -L{compiler.path}/arm/arm-none-eabi/lib",
                1)
            self.platform["recipe.c.combine.pattern"] = new_link_line

        # Adafruit-samd core
        # TODO(tonymd): This build_arch may clash with Arduino-SAMD core
        elif self.build_arch == 'SAMD':
            new_link_line = self.platform["recipe.c.combine.pattern"].replace(
                "\"{build.path}/{archive_file}\" -Wl,--end-group",
                "{archive_file_path} -Wl,--end-group", 1)
            self.platform["recipe.c.combine.pattern"] = new_link_line

        # STM32L4 Core:
        # https://github.com/GrumpyOldPizza/arduino-STM32L4
        elif self.build_arch == 'STM32L4':
            # TODO(tonymd): {build.path}/{archive_file} for the link step always
            # seems to be core.a (except STM32 core)
            line_to_delete = "-Wl,--start-group \"{build.path}/{archive_file}\""
            new_link_line = self.platform["recipe.c.combine.pattern"].replace(
                line_to_delete, "-Wl,--start-group {archive_file_path}", 1)
            self.platform["recipe.c.combine.pattern"] = new_link_line

        # stm32duino core
        elif self.build_arch == 'STM32':
            pass

    def _copy_default_menu_options_to_build_variables(self):
        # Clear existing options
        self.menu_options["selected"] = {}
        # Set default menu options for selected board
        for menu_key, menu_dict in self.menu_options["default_board_values"][
                self.selected_board].items():
            for name, var in self.board[self.selected_board].items():
                starting_key = "{}.{}.".format(menu_key, menu_dict["name"])
                if name.startswith(starting_key):
                    new_var_name = name.replace(starting_key, "", 1)
                    self.menu_options["selected"][new_var_name] = var

    def set_menu_option(self, moption):
        if moption not in self.board[self.selected_board]:
            _LOG.error("Error: '%s' is not a valid menu option.", moption)
            return False

        # Override default menu option with new value.
        menu_match_result = self.menu_option_regex.match(moption)
        if menu_match_result:
            menu_match = menu_match_result.groupdict()
            menu_value = menu_match["menu_option_value"]
            menu_key = "menu.{}".format(menu_match["menu_option_name"])
            self.menu_options["default_board_values"][
                self.selected_board][menu_key]["name"] = menu_value

        # Update build variables
        self._copy_default_menu_options_to_build_variables()
        return True

    def _set_global_arduino_variables(self):
        """Set some global variables defined by the Arduino-IDE.

        See Docs:
        https://arduino.github.io/arduino-cli/platform-specification/#global-predefined-properties
        """

        for current_board_name in self.board.keys():
            if self.build_path:
                self.board[current_board_name]["build.path"] = self.build_path
            if self.build_project_name:
                self.board[current_board_name][
                    "build.project_name"] = self.build_project_name
                # {archive_file} is the final *.elf
                archive_file = "{}.elf".format(self.build_project_name)
                self.board[current_board_name]["archive_file"] = archive_file
                # {archive_file_path} is the final core.a archive
                if self.build_path:
                    self.board[current_board_name][
                        "archive_file_path"] = os.path.join(
                            self.build_path, "core.a")
            if self.project_source_path:
                self.board[current_board_name][
                    "build.source.path"] = self.project_source_path

            self.board[current_board_name]["extra.time.local"] = str(
                int(time.time()))
            self.board[current_board_name]["runtime.ide.version"] = "10812"
            self.board[current_board_name][
                "runtime.hardware.path"] = self.hardware_path

            # Copy {runtime.tools.TOOL_NAME.path} vars
            self._set_tools_variables(self.board[current_board_name])

            self.board[current_board_name][
                "runtime.platform.path"] = self.package_path
            if self.platform["name"] == "Teensyduino":
                # Teensyduino is installed into the arduino IDE folder
                # rather than ~/.arduino15/packages/
                self.board[current_board_name][
                    "runtime.hardware.path"] = os.path.join(
                        self.hardware_path, "teensy")

            self.board[current_board_name]["build.system.path"] = os.path.join(
                self.package_path, "system")

            # Set the {build.core.path} variable that pointing to a sub-core
            # folder. For Teensys this is:
            # 'teensy/hardware/teensy/avr/cores/teensy{3,4}'. For other cores
            # it's typically just the 'arduino' folder. For example:
            # 'arduino-samd/hardware/samd/1.8.8/cores/arduino'
            core_path = os.path.join(
                self.package_path, "cores",
                self.board[current_board_name].get("build.core",
                                                   self.sub_core_folders[0]))
            self.board[current_board_name]["build.core.path"] = core_path

            self.board[current_board_name]["build.arch"] = self.build_arch

            for name, var in self.board[current_board_name].items():
                self.board[current_board_name][name] = var.replace(
                    "{build.core.path}", core_path)

    def load_board_definitions(self):
        """Loads Arduino boards.txt and platform.txt files into dictionaries.

        Populates the following dictionaries:
            self.menu_options
            self.boards
            self.platform
        """
        # Load platform.txt
        with open(self.platform_txt, "r") as pfile:
            platform_file = pfile.read()
            platform_var_matches = self.variable_regex.finditer(platform_file)
            for p_match in [m.groupdict() for m in platform_var_matches]:
                self.platform[p_match["name"]] = p_match["value"]

        # Load boards.txt
        with open(self.boards_txt, "r") as bfile:
            board_file = bfile.read()
            # Get all top-level menu options, e.g. menu.usb=USB Type
            board_menu_matches = self.board_menu_regex.finditer(board_file)
            for menuitem in [m.groupdict() for m in board_menu_matches]:
                self.menu_options["global_options"][menuitem["name"]] = {
                    "description": menuitem["description"]
                }

            # Get all board names, e.g. teensy40.name=Teensy 4.0
            board_name_matches = self.board_name_regex.finditer(board_file)
            for b_match in [m.groupdict() for m in board_name_matches]:
                self.board[b_match["name"]] = OrderedDict()
                self.menu_options["default_board_values"][
                    b_match["name"]] = OrderedDict()

            # Get all board variables, e.g. teensy40.*
            for current_board_name in self.board.keys():
                board_line_matches = re.finditer(
                    fr"^\s*{current_board_name}\."
                    fr"(?P<key>[^#=]+)=(?P<value>.*)$", board_file,
                    re.MULTILINE)
                for b_match in [m.groupdict() for m in board_line_matches]:
                    # Check if this line is a menu option
                    # (e.g. 'menu.usb.serial') and save as default if it's the
                    # first one seen.
                    ArduinoBuilder.save_default_menu_option(
                        current_board_name, b_match["key"], b_match["value"],
                        self.menu_options)
                    self.board[current_board_name][
                        b_match["key"]] = b_match["value"].strip()

            self._set_global_arduino_variables()

    @staticmethod
    def save_default_menu_option(current_board_name, key, value, menu_options):
        """Save a given menu option as the default.

        Saves the key and value into menu_options["default_board_values"]
        if it doesn't already exist. Assumes menu options are added in the order
        specified in boards.txt. The first value for a menu key is the default.
        """
        # Check if key is a menu option
        # e.g. menu.usb.serial
        #      menu.usb.serial.build.usbtype
        menu_match_result = re.match(
            r'^menu\.'  # starts with "menu"
            r'(?P<menu_option_name>[^.]+)\.'  # first token after .
            r'(?P<menu_option_value>[^.]+)'  # second token after .
            r'(\.(?P<rest>.+))?',  # optionally any trailing tokens after a .
            key)
        if menu_match_result:
            menu_match = menu_match_result.groupdict()
            current_menu_key = "menu.{}".format(menu_match["menu_option_name"])
            # If this is the first menu option seen for current_board_name, save
            # as the default.
            if current_menu_key not in menu_options["default_board_values"][
                    current_board_name]:
                menu_options["default_board_values"][current_board_name][
                    current_menu_key] = {
                        "name": menu_match["menu_option_value"],
                        "description": value
                    }

    def _replace_variables(self, line, variable_lookup_source):
        """Replace {variables} from loaded boards.txt or platform.txt.

        Replace interpolated variables surrounded by curly braces in line with
        definitions from variable_lookup_source.
        """
        new_line = line
        for current_var_match in self.interpolated_variable_regex.findall(
                line):
            # {build.flags.c} --> build.flags.c
            current_var = current_var_match.strip("{}")

            # check for matches in board definition
            if current_var in variable_lookup_source:
                replacement = variable_lookup_source.get(current_var, "")
                new_line = new_line.replace(current_var_match, replacement)
        return new_line

    def _find_tools_variables(self):
        # Gather tool directories in order of increasing precedence
        runtime_tool_paths = []

        # Check for tools installed in ~/.arduino15/packages/arduino/tools/
        # TODO(tonymd): Is this Mac & Linux specific?
        runtime_tool_paths += glob.glob(
            os.path.join(
                os.path.realpath(os.path.expanduser(os.path.expandvars("~"))),
                ".arduino15", "packages", "arduino", "tools", "*"))

        # <ARDUINO_PATH>/tools/<OS_STRING>/<TOOL_NAMES>
        runtime_tool_paths += glob.glob(
            os.path.join(self.arduino_path, "tools",
                         arduino_runtime_os_string(), "*"))
        # <ARDUINO_PATH>/tools/<TOOL_NAMES>
        # This will grab linux/windows/macosx/share as <TOOL_NAMES>.
        runtime_tool_paths += glob.glob(
            os.path.join(self.arduino_path, "tools", "*"))

        # Process package tools after arduino tools.
        # They should overwrite vars & take precedence.

        # <PACKAGE_PATH>/tools/<OS_STRING>/<TOOL_NAMES>
        runtime_tool_paths += glob.glob(
            os.path.join(self.package_path, "tools",
                         arduino_runtime_os_string(), "*"))
        # <PACKAGE_PATH>/tools/<TOOL_NAMES>
        # This will grab linux/windows/macosx/share as <TOOL_NAMES>.
        runtime_tool_paths += glob.glob(
            os.path.join(self.package_path, "tools", "*"))

        for path in runtime_tool_paths:
            # Make sure TOOL_NAME is not an OS string
            if not (path.endswith("linux") or path.endswith("windows")
                    or path.endswith("macosx") or path.endswith("share")):
                # TODO(tonymd): Check if a file & do nothing?

                # Check if it's a directory with subdir(s) as a version string
                #   create all 'runtime.tools.{tool_folder}-{version.path}'
                #     (for each version)
                #   create 'runtime.tools.{tool_folder}.path'
                #     (with latest version)
                if os.path.isdir(path):
                    # Grab the tool name (folder) by itself.
                    tool_folder = os.path.basename(path)
                    # Sort so that [-1] is the latest version.
                    version_paths = sorted(glob.glob(os.path.join(path, "*")))
                    # Check if all sub folders start with a version string.
                    if len(version_paths) == sum(
                            bool(re.match(r"^[0-9.]+", os.path.basename(vp)))
                            for vp in version_paths):
                        for version_path in version_paths:
                            version_string = os.path.basename(version_path)
                            var_name = "runtime.tools.{}-{}.path".format(
                                tool_folder, version_string)
                            self.tools_variables[var_name] = os.path.join(
                                path, version_string)
                        var_name = "runtime.tools.{}.path".format(tool_folder)
                        self.tools_variables[var_name] = os.path.join(
                            path, os.path.basename(version_paths[-1]))
                    # Else set toolpath to path.
                    else:
                        var_name = "runtime.tools.{}.path".format(tool_folder)
                        self.tools_variables[var_name] = path

        _LOG.debug("TOOL VARIABLES: %s", _pretty_format(self.tools_variables))

    # Copy self.tools_variables into destination
    def _set_tools_variables(self, destination):
        for key, value in self.tools_variables.items():
            destination[key] = value

    def _substitute_variables(self):
        """Perform variable substitution in board and platform metadata."""

        # menu -> board
        # Copy selected menu variables into board definiton
        for name, value in self.menu_options["selected"].items():
            self.board[self.selected_board][name] = value

        # board -> board
        # Replace any {vars} in the selected board with values defined within
        # (and from copied in menu options).
        for var, value in self.board[self.selected_board].items():
            self.board[self.selected_board][var] = self._replace_variables(
                value, self.board[self.selected_board])

        # Check for build.variant variable
        # This will be set in selected board after menu options substitution
        build_variant = self.board[self.selected_board].get(
            "build.variant", None)
        if build_variant:
            # Set build.variant.path
            bvp = os.path.join(self.package_path, "variants", build_variant)
            self.build_variant_path = bvp
            self.board[self.selected_board]["build.variant.path"] = bvp
            # Add the variant folder as an include directory
            # (used in stm32l4 core)
            self.variant_includes = "-I{}".format(bvp)

        _LOG.debug("PLATFORM INITIAL: %s", _pretty_format(self.platform))

        # board -> platform
        # Replace {vars} in platform from the selected board definition
        for var, value in self.platform.items():
            self.platform[var] = self._replace_variables(
                value, self.board[self.selected_board])

        # platform -> platform
        # Replace any remaining {vars} in platform from platform
        for var, value in self.platform.items():
            self.platform[var] = self._replace_variables(value, self.platform)

        # Repeat platform -> platform for any lingering variables
        # Example: {build.opt.name} in STM32 core
        for var, value in self.platform.items():
            self.platform[var] = self._replace_variables(value, self.platform)

        _LOG.debug("MENU_OPTIONS: %s", _pretty_format(self.menu_options))
        _LOG.debug("SELECTED_BOARD: %s",
                   _pretty_format(self.board[self.selected_board]))
        _LOG.debug("PLATFORM: %s", _pretty_format(self.platform))

    def selected_board_spec(self):
        return self.board[self.selected_board]

    def get_menu_options(self):
        all_options = []
        max_string_length = [0, 0]

        for key_name, description in self.board[self.selected_board].items():
            menu_match_result = self.menu_option_regex.match(key_name)
            if menu_match_result:
                menu_match = menu_match_result.groupdict()
                name = "menu.{}.{}".format(menu_match["menu_option_name"],
                                           menu_match["menu_option_value"])
                if len(name) > max_string_length[0]:
                    max_string_length[0] = len(name)
                if len(description) > max_string_length[1]:
                    max_string_length[1] = len(description)
                all_options.append((name, description))

        return all_options, max_string_length

    def get_default_menu_options(self):
        default_options = []
        max_string_length = [0, 0]

        for key_name, value in self.menu_options["default_board_values"][
                self.selected_board].items():
            full_key = key_name + "." + value["name"]
            if len(full_key) > max_string_length[0]:
                max_string_length[0] = len(full_key)
            if len(value["description"]) > max_string_length[1]:
                max_string_length[1] = len(value["description"])
            default_options.append((full_key, value["description"]))

        return default_options, max_string_length

    @staticmethod
    def split_binary_from_arguments(compile_line):
        compile_binary = None
        rest_of_line = compile_line

        compile_binary_match = re.search(r'^("[^"]+") ', compile_line)
        if compile_binary_match:
            compile_binary = compile_binary_match[1]
            rest_of_line = compile_line.replace(compile_binary_match[0], "", 1)

        return compile_binary, rest_of_line

    def _strip_includes_source_file_object_file_vars(self, compile_line):
        line = compile_line
        if self.variant_includes:
            line = compile_line.replace(
                "{includes} \"{source_file}\" -o \"{object_file}\"",
                self.variant_includes, 1)
        else:
            line = compile_line.replace(
                "{includes} \"{source_file}\" -o \"{object_file}\"", "", 1)
        return line

    def _get_tool_name(self, line):
        tool_match_result = self.tool_name_regex.match(line)
        if tool_match_result:
            return tool_match_result[1]
        return False

    def get_upload_tool_names(self):
        return [
            self._get_tool_name(t) for t in self.platform.keys()
            if self.tool_name_regex.match(t) and 'upload.pattern' in t
        ]

    # TODO(tonymd): Use these getters in _replace_variables() or
    # _substitute_variables()

    def _get_platform_variable(self, variable):
        # TODO(tonymd): Check for '.macos' '.linux' '.windows' in variable name,
        # compare with platform.system() and return that instead.
        return self.platform.get(variable, False)

    def _get_platform_variable_with_substitutions(self, variable, namespace):
        line = self.platform.get(variable, False)
        # Get all unique variables used in this line in line.
        unique_vars = sorted(
            set(self.interpolated_variable_regex.findall(line)))
        # Search for each unique_vars in namespace and global.
        for var in unique_vars:
            v_raw_name = var.strip("{}")

            # Check for namespace.variable
            #   eg: 'tools.stm32CubeProg.cmd'
            possible_var_name = "{}.{}".format(namespace, v_raw_name)
            result = self._get_platform_variable(possible_var_name)
            # Check for os overriden variable
            #   eg:
            #     ('tools.stm32CubeProg.cmd', 'stm32CubeProg.sh'),
            #     ('tools.stm32CubeProg.cmd.windows', 'stm32CubeProg.bat'),
            possible_var_name = "{}.{}.{}".format(namespace, v_raw_name,
                                                  arduino_runtime_os_string())
            os_override_result = self._get_platform_variable(possible_var_name)

            if os_override_result:
                line = line.replace(var, os_override_result)
            elif result:
                line = line.replace(var, result)
            # Check for variable at top level?
            # elif self._get_platform_variable(v_raw_name):
            #     line = line.replace(self._get_platform_variable(v_raw_name),
            #                         result)
        return line

    def get_upload_line(self, tool_name, serial_port=False):
        # TODO(tonymd): Error if tool_name does not exist
        tool_namespace = "tools.{}".format(tool_name)
        pattern = "tools.{}.upload.pattern".format(tool_name)

        if not self._get_platform_variable(pattern):
            _LOG.error("Error: upload tool '%s' does not exist.", tool_name)
            tools = self.get_upload_tool_names()
            _LOG.error("Valid tools: %s", ", ".join(tools))
            return sys.exit(1)

        line = self._get_platform_variable_with_substitutions(
            pattern, tool_namespace)

        # TODO(tonymd): Teensy specific tool overrides.
        if tool_name == "teensyloader":
            # Remove un-necessary lines
            # {serial.port.label} and {serial.port.protocol} are returned by
            # the teensy_ports binary.
            line = line.replace("\"-portlabel={serial.port.label}\"", "", 1)
            line = line.replace("\"-portprotocol={serial.port.protocol}\"", "",
                                1)

        if serial_port:
            line = line.replace("{serial.port}", serial_port, 1)

        return line

    def _get_binary_path(self, variable_pattern):
        compile_line = self.replace_compile_binary_with_override_path(
            self._get_platform_variable(variable_pattern))
        compile_binary, _ = ArduinoBuilder.split_binary_from_arguments(
            compile_line)
        return compile_binary

    def get_cc_binary(self):
        return self._get_binary_path("recipe.c.o.pattern")

    def get_cxx_binary(self):
        return self._get_binary_path("recipe.cpp.o.pattern")

    def get_objcopy_binary(self):
        objcopy_step_name = self.get_objcopy_step_names()[0]
        objcopy_binary = self._get_binary_path(objcopy_step_name)
        return objcopy_binary

    def get_ar_binary(self):
        return self._get_binary_path("recipe.ar.pattern")

    def get_size_binary(self):
        return self._get_binary_path("recipe.size.pattern")

    def replace_command_args_with_compiler_override_path(self, compile_line):
        if not self.compiler_path_override:
            return compile_line
        replacement_line = compile_line
        replacement_line_args = compile_line.split()
        for arg in replacement_line_args:
            compile_binary_basename = os.path.basename(arg.strip("\""))
            if compile_binary_basename in self.compiler_path_override_binaries:
                new_compiler = os.path.join(self.compiler_path_override,
                                            compile_binary_basename)
                replacement_line = replacement_line.replace(
                    arg, new_compiler, 1)
        return replacement_line

    def replace_compile_binary_with_override_path(self, compile_line):
        replacement_compile_line = compile_line

        # Change the compiler path if there's an override path set
        if self.compiler_path_override:
            compile_binary, line = ArduinoBuilder.split_binary_from_arguments(
                compile_line)
            compile_binary_basename = os.path.basename(
                compile_binary.strip("\""))
            new_compiler = os.path.join(self.compiler_path_override,
                                        compile_binary_basename)
            if platform.system() == "Windows" and not re.match(
                    r".*\.exe$", new_compiler, flags=re.IGNORECASE):
                new_compiler += ".exe"

            if os.path.isfile(new_compiler):
                replacement_compile_line = "\"{}\" {}".format(
                    new_compiler, line)

        return replacement_compile_line

    def get_c_compile_line(self):
        _LOG.debug("ARDUINO_C_COMPILE: %s",
                   _pretty_format(self.platform["recipe.c.o.pattern"]))

        compile_line = self.platform["recipe.c.o.pattern"]
        compile_line = self._strip_includes_source_file_object_file_vars(
            compile_line)
        compile_line += " -I{}".format(
            self.board[self.selected_board]["build.core.path"])

        compile_line = self.replace_compile_binary_with_override_path(
            compile_line)
        return compile_line

    def get_s_compile_line(self):
        _LOG.debug("ARDUINO_S_COMPILE %s",
                   _pretty_format(self.platform["recipe.S.o.pattern"]))

        compile_line = self.platform["recipe.S.o.pattern"]
        compile_line = self._strip_includes_source_file_object_file_vars(
            compile_line)
        compile_line += " -I{}".format(
            self.board[self.selected_board]["build.core.path"])

        compile_line = self.replace_compile_binary_with_override_path(
            compile_line)
        return compile_line

    def get_ar_compile_line(self):
        _LOG.debug("ARDUINO_AR_COMPILE: %s",
                   _pretty_format(self.platform["recipe.ar.pattern"]))

        compile_line = self.platform["recipe.ar.pattern"].replace(
            "\"{object_file}\"", "", 1)

        compile_line = self.replace_compile_binary_with_override_path(
            compile_line)
        return compile_line

    def get_cpp_compile_line(self):
        _LOG.debug("ARDUINO_CPP_COMPILE: %s",
                   _pretty_format(self.platform["recipe.cpp.o.pattern"]))

        compile_line = self.platform["recipe.cpp.o.pattern"]
        compile_line = self._strip_includes_source_file_object_file_vars(
            compile_line)
        compile_line += " -I{}".format(
            self.board[self.selected_board]["build.core.path"])

        compile_line = self.replace_compile_binary_with_override_path(
            compile_line)
        return compile_line

    def get_link_line(self):
        _LOG.debug("ARDUINO_LINK: %s",
                   _pretty_format(self.platform["recipe.c.combine.pattern"]))

        compile_line = self.platform["recipe.c.combine.pattern"]

        compile_line = self.replace_compile_binary_with_override_path(
            compile_line)
        return compile_line

    def get_objcopy_step_names(self):
        names = [
            name for name, line in self.platform.items()
            if self.objcopy_step_name_regex.match(name)
        ]
        return names

    def get_objcopy_steps(self) -> List[str]:
        lines = [
            line for name, line in self.platform.items()
            if self.objcopy_step_name_regex.match(name)
        ]
        lines = [
            self.replace_compile_binary_with_override_path(line)
            for line in lines
        ]
        return lines

    # TODO(tonymd): These recipes are probably run in sorted order
    def get_objcopy(self, suffix):
        # Expected vars:
        # teensy:
        #   recipe.objcopy.eep.pattern
        #   recipe.objcopy.hex.pattern

        pattern = "recipe.objcopy.{}.pattern".format(suffix)
        objcopy_step_names = self.get_objcopy_step_names()

        objcopy_suffixes = [
            m[1] for m in [
                self.objcopy_step_name_regex.match(line)
                for line in objcopy_step_names
            ] if m
        ]
        if pattern not in objcopy_step_names:
            _LOG.error("Error: objcopy suffix '%s' does not exist.", suffix)
            _LOG.error("Valid suffixes: %s", ", ".join(objcopy_suffixes))
            return sys.exit(1)

        line = self._get_platform_variable(pattern)

        _LOG.debug("ARDUINO_OBJCOPY_%s: %s", suffix, line)

        line = self.replace_compile_binary_with_override_path(line)

        return line

    def get_objcopy_flags(self, suffix):
        # TODO(tonymd): Possibly teensy specific variables.
        flags = ""
        if suffix == "hex":
            flags = self.platform.get("compiler.elf2hex.flags", "")
        elif suffix == "bin":
            flags = self.platform.get("compiler.elf2bin.flags", "")
        elif suffix == "eep":
            flags = self.platform.get("compiler.objcopy.eep.flags", "")
        return flags

    # TODO(tonymd): There are more recipe hooks besides postbuild.
    #   They are run in sorted order.
    # TODO(tonymd): Rename this to get_hooks(hook_name, step).
    # TODO(tonymd): Add a list-hooks and or run-hooks command
    def get_postbuild_line(self, step_number):
        line = self.platform["recipe.hooks.postbuild.{}.pattern".format(
            step_number)]
        line = self.replace_command_args_with_compiler_override_path(line)
        return line

    def get_prebuild_steps(self) -> List[str]:
        # Teensy core uses recipe.hooks.sketch.prebuild.1.pattern
        # stm32 core uses recipe.hooks.prebuild.1.pattern
        # TODO(tonymd): STM32 core uses recipe.hooks.prebuild.1.pattern.windows
        #   (should override non-windows key)
        lines = [
            line for name, line in self.platform.items() if re.match(
                r"^recipe.hooks.(?:sketch.)?prebuild.[^.]+.pattern$", name)
        ]
        # TODO(tonymd): Write a function to fetch/replace OS specific patterns
        #   (ending in an OS string)
        lines = [
            self.replace_compile_binary_with_override_path(line)
            for line in lines
        ]
        return lines

    def get_postbuild_steps(self) -> List[str]:
        lines = [
            line for name, line in self.platform.items()
            if re.match(r"^recipe.hooks.postbuild.[^.]+.pattern$", name)
        ]

        lines = [
            self.replace_command_args_with_compiler_override_path(line)
            for line in lines
        ]
        return lines

    def get_s_flags(self):
        compile_line = self.get_s_compile_line()
        _, compile_line = ArduinoBuilder.split_binary_from_arguments(
            compile_line)
        compile_line = compile_line.replace("-c", "", 1)
        return compile_line.strip()

    def get_c_flags(self):
        compile_line = self.get_c_compile_line()
        _, compile_line = ArduinoBuilder.split_binary_from_arguments(
            compile_line)
        compile_line = compile_line.replace("-c", "", 1)
        return compile_line.strip()

    def get_cpp_flags(self):
        compile_line = self.get_cpp_compile_line()
        _, compile_line = ArduinoBuilder.split_binary_from_arguments(
            compile_line)
        compile_line = compile_line.replace("-c", "", 1)
        return compile_line.strip()

    def get_ar_flags(self):
        compile_line = self.get_ar_compile_line()
        _, compile_line = ArduinoBuilder.split_binary_from_arguments(
            compile_line)
        return compile_line.strip()

    def get_ld_flags(self):
        compile_line = self.get_link_line()
        _, compile_line = ArduinoBuilder.split_binary_from_arguments(
            compile_line)

        # TODO(tonymd): This is teensy specific
        line_to_delete = "-o \"{build.path}/{build.project_name}.elf\" " \
            "{object_files} \"-L{build.path}\""
        if self.build_path:
            line_to_delete = line_to_delete.replace("{build.path}",
                                                    self.build_path)
        if self.build_project_name:
            line_to_delete = line_to_delete.replace("{build.project_name}",
                                                    self.build_project_name)

        compile_line = compile_line.replace(line_to_delete, "", 1)
        libs = re.findall(r'(-l[^ ]+ ?)', compile_line)
        for lib in libs:
            compile_line = compile_line.replace(lib, "", 1)
        libs = [lib.strip() for lib in libs]

        return compile_line.strip()

    def get_ld_libs(self):
        compile_line = self.get_link_line()
        _, compile_line = ArduinoBuilder.split_binary_from_arguments(
            compile_line)
        # TODO(tonymd): This replacement is teensy specific
        compile_line = compile_line.replace(
            "-o \"{build.path}/{build.project_name}.elf\" "
            "{object_files} \"-L{build.path}\"", "", 1)
        libs = re.findall(r'(-l[^ ]+ ?)', compile_line)
        for lib in libs:
            compile_line = compile_line.replace(lib, "", 1)
        libs = [lib.strip() for lib in libs]

        return " ".join(libs)

    def library_folders(self):
        # Arduino library format documentation:
        # https://arduino.github.io/arduino-cli/library-specification/#layout-of-folders-and-files
        # - If src folder exists,
        #   use that as the root include directory -Ilibraries/libname/src
        # - Else lib folder as root include -Ilibraries/libname
        #   (exclude source files in the examples folder in this case)

        library_path = os.path.join(self.project_path, "libraries")

        library_folders = FileOperations.find_files(library_path, ["*"],
                                                    directories_only=True)
        library_source_root_folders = []
        for lib in library_folders:
            lib_dir = os.path.join(library_path, lib)
            src_dir = os.path.join(lib_dir, "src")
            if os.path.exists(src_dir) and os.path.isdir(src_dir):
                library_source_root_folders.append(src_dir)
            else:
                library_source_root_folders.append(lib_dir)

        return library_source_root_folders

    def library_includes(self):
        include_args = []
        library_folders = self.library_folders()
        for lib_dir in library_folders:
            include_args.append("-I{}".format(os.path.relpath(lib_dir)))
        return include_args

    def library_files(self, pattern):
        sources = []
        library_folders = self.library_folders()
        for lib_dir in library_folders:
            for file_path in FileOperations.find_files(lib_dir, [pattern]):
                if not file_path.startswith("examples"):
                    sources.append(
                        os.path.relpath(os.path.join(lib_dir, file_path)))
        return sources

    def library_c_files(self):
        return self.library_files("**/*.c")

    def library_cpp_files(self):
        return self.library_files("**/*.cpp")

    def get_core_path(self):
        return self.board[self.selected_board]["build.core.path"]

    def core_files(self, pattern):
        sources = []
        for file_path in FileOperations.find_files(self.get_core_path(),
                                                   [pattern]):
            sources.append(os.path.join(self.get_core_path(), file_path))
        return sources

    def core_c_files(self):
        return self.core_files("**/*.c")

    def core_s_files(self):
        return self.core_files("**/*.S")

    def core_cpp_files(self):
        return self.core_files("**/*.cpp")

    def get_variant_path(self):
        return self.build_variant_path

    def variant_files(self, pattern):
        sources = []
        if self.build_variant_path:
            for file_path in FileOperations.find_files(self.get_variant_path(),
                                                       [pattern]):
                sources.append(os.path.join(self.get_variant_path(),
                                            file_path))
        return sources

    def variant_c_files(self):
        return self.variant_files("**/*.c")

    def variant_s_files(self):
        return self.variant_files("**/*.S")

    def variant_cpp_files(self):
        return self.variant_files("**/*.cpp")

    def project_files(self, pattern):
        sources = []
        for file_path in FileOperations.find_files(self.project_path,
                                                   [pattern]):
            if not file_path.startswith(
                    "examples") and not file_path.startswith("libraries"):
                sources.append(file_path)
        return sources

    def project_c_files(self):
        return self.project_files("**/*.c")

    def project_cpp_files(self):
        return self.project_files("**/*.cpp")

    def project_ino_files(self):
        return self.project_files("**/*.ino")


def install_core_command(args):
    install_prefix = os.path.realpath(
        os.path.expanduser(os.path.expandvars(args.prefix)))
    install_dir = os.path.join(install_prefix, args.core_name)
    cache_dir = os.path.join(install_prefix, ".cache", args.core_name)

    shutil.rmtree(install_dir, ignore_errors=True)
    os.makedirs(install_dir, exist_ok=True)
    os.makedirs(cache_dir, exist_ok=True)

    if args.core_name == "teensy":
        if platform.system() == "Linux":
            ArduinoCoreInstaller.install_teensy_core_linux(
                install_prefix, install_dir, cache_dir)
        elif platform.system() == "Darwin":
            ArduinoCoreInstaller.install_teensy_core_mac(
                install_prefix, install_dir, cache_dir)
        elif platform.system() == "Windows":
            ArduinoCoreInstaller.install_teensy_core_windows(
                install_prefix, install_dir, cache_dir)
    elif args.core_name == "adafruit-samd":
        ArduinoCoreInstaller.install_adafruit_samd_core(
            install_prefix, install_dir, cache_dir)
    elif args.core_name == "stm32duino":
        ArduinoCoreInstaller.install_stm32duino_core(install_prefix,
                                                     install_dir, cache_dir)

    sys.exit(0)


def list_boards_command(unused_args, builder):
    # list-boards subcommand
    # (does not need a selected board or default menu options)

    # TODO(tonymd): Print this sorted with auto-ljust columns
    longest_name_length = 0
    for board_name, board_dict in builder.board.items():
        if len(board_name) > longest_name_length:
            longest_name_length = len(board_name)
    longest_name_length += 2

    print("Board Name".ljust(longest_name_length), "Description")
    for board_name, board_dict in builder.board.items():
        print(board_name.ljust(longest_name_length), board_dict['name'])
    sys.exit(0)


def list_menu_options_command(args, builder):
    # List all menu options for the selected board.
    builder.select_board(args.board)

    print("All Options")
    all_options, all_column_widths = builder.get_menu_options()
    separator = "-" * (all_column_widths[0] + all_column_widths[1] + 2)
    print(separator)

    for name, description in all_options:
        print(name.ljust(all_column_widths[0] + 1), description)

    print("\nDefault Options")
    print(separator)

    default_options, unused_column_widths = builder.get_default_menu_options()
    for name, description in default_options:
        print(name.ljust(all_column_widths[0] + 1), description)


def show_command_print_string_list(args, string_list: List[str]):
    join_token = " "
    if args.delimit_with_newlines:
        join_token = "\n"
    print(join_token.join(string_list))


def show_command_print_flag_string(args, flag_string):
    if args.delimit_with_newlines:
        flag_string_with_newlines = shlex.split(flag_string)
        print("\n".join(flag_string_with_newlines))
    else:
        print(flag_string)


def run_command_lines(args, command_lines: List[str]):
    for command_line in command_lines:
        if not args.quiet:
            print(command_line)
        # TODO(tonymd): Exit with sub command exit code.
        os.system(command_line)


# pylint: disable=too-many-branches
def show_command(args, builder):
    """Show sub command function.

    Prints compile flags and runs Arduino recipes.
    """
    builder.select_board(args.board, args.menu_options)

    if args.cc_binary:
        print(builder.get_cc_binary())

    elif args.cxx_binary:
        print(builder.get_cxx_binary())

    elif args.objcopy_binary:
        print(builder.get_objcopy_binary())

    elif args.ar_binary:
        print(builder.get_ar_binary())

    elif args.size_binary:
        print(builder.get_size_binary())

    elif args.run_c_compile:
        print(builder.get_c_compile_line())

    elif args.run_cpp_compile:
        print(builder.get_cpp_compile_line())

    elif args.run_link:
        line = builder.get_link_line()
        archive_file_path = args.run_link[0]  # pylint: disable=unused-variable
        object_files = args.run_link[1:]
        line = line.replace("{object_files}", " ".join(object_files), 1)
        run_command_lines(args, [line])

    elif args.run_objcopy:
        run_command_lines(args, builder.get_objcopy_steps())

    elif args.objcopy:
        print(builder.get_objcopy(args.objcopy))

    elif args.objcopy_flags:
        objcopy_flags = builder.get_objcopy_flags(args.objcopy_flags)
        show_command_print_flag_string(args, objcopy_flags)

    elif args.c_flags:
        cflags = builder.get_c_flags()
        show_command_print_flag_string(args, cflags)

    elif args.s_flags:
        sflags = builder.get_s_flags()
        show_command_print_flag_string(args, sflags)

    elif args.cpp_flags:
        cppflags = builder.get_cpp_flags()
        show_command_print_flag_string(args, cppflags)

    elif args.ld_flags:
        ldflags = builder.get_ld_flags()
        show_command_print_flag_string(args, ldflags)

    elif args.ld_libs:
        print(builder.get_ld_libs())

    elif args.ar_flags:
        ar_flags = builder.get_ar_flags()
        show_command_print_flag_string(args, ar_flags)

    elif args.core_path:
        print(builder.get_core_path())

    elif args.run_prebuilds:
        run_command_lines(args, builder.get_prebuild_steps())

    elif args.run_postbuilds:
        run_command_lines(args, builder.get_postbuild_steps())

    elif args.postbuild:
        print(builder.get_postbuild_line(args.postbuild))

    elif args.run_upload_command:
        command = builder.get_upload_line(args.run_upload_command,
                                          args.serial_port)
        run_command_lines(args, [command])

    elif args.upload_command:
        print(builder.get_upload_line(args.upload_command, args.serial_port))

    elif args.upload_tools:
        tools = builder.get_upload_tool_names()
        for tool_name in tools:
            print(tool_name)

    elif args.library_includes:
        show_command_print_string_list(args, builder.library_includes())

    elif args.library_c_files:
        show_command_print_string_list(args, builder.library_c_files())

    elif args.library_cpp_files:
        show_command_print_string_list(args, builder.library_cpp_files())

    elif args.core_c_files:
        show_command_print_string_list(args, builder.core_c_files())

    elif args.core_s_files:
        show_command_print_string_list(args, builder.core_s_files())

    elif args.core_cpp_files:
        show_command_print_string_list(args, builder.core_cpp_files())

    elif args.variant_c_files:
        vfiles = builder.variant_c_files()
        if vfiles:
            show_command_print_string_list(args, vfiles)

    elif args.variant_s_files:
        vfiles = builder.variant_s_files()
        if vfiles:
            show_command_print_string_list(args, vfiles)

    elif args.variant_cpp_files:
        vfiles = builder.variant_cpp_files()
        if vfiles:
            show_command_print_string_list(args, vfiles)


def main():
    """Main command line function.

    Parses command line args and dispatches to sub `*_command()` functions.
    """
    def log_level(arg: str) -> int:
        try:
            return getattr(logging, arg.upper())
        except AttributeError:
            raise argparse.ArgumentTypeError(
                f'{arg.upper()} is not a valid log level')

    parser = argparse.ArgumentParser()
    parser.add_argument("-q",
                        "--quiet",
                        help="hide run command output",
                        action="store_true")
    parser.add_argument('-l',
                        '--loglevel',
                        type=log_level,
                        default=logging.INFO,
                        help='Set the log level '
                        '(debug, info, warning, error, critical)')

    build_path = os.path.realpath(
        os.path.expanduser(os.path.expandvars("./build")))
    project_path = os.path.realpath(
        os.path.expanduser(os.path.expandvars("./")))
    project_source_path = os.path.join(project_path, "src")
    build_project_name = os.path.basename(project_path)

    serial_port = "UNKNOWN"
    # TODO(tonymd): Temp solution to passing in serial port. It should use
    # arduino core discovery tools.
    possible_serial_ports = glob.glob("/dev/ttyACM*") + glob.glob(
        "/dev/ttyUSB*")
    if possible_serial_ports:
        serial_port = possible_serial_ports[0]

    project_name = os.path.basename(
        os.path.realpath(os.path.expanduser(os.path.expandvars("./"))))

    # Global command line options
    parser.add_argument("--arduino-package-path",
                        help="Path to the arduino IDE install location.")
    parser.add_argument("--arduino-package-name",
                        help="Name of the Arduino board package to use.")
    parser.add_argument("--compiler-path-override",
                        help="Path to arm-none-eabi-gcc bin folder. "
                        "Default: Arduino core specified gcc")

    # Subcommands
    subparsers = parser.add_subparsers(title="subcommand",
                                       description="valid subcommands",
                                       help="sub-command help",
                                       dest="subcommand",
                                       required=True)

    # install-core command
    install_core_parser = subparsers.add_parser(
        "install-core", help="Download and install arduino cores")
    install_core_parser.set_defaults(func=install_core_command)
    install_core_parser.add_argument("--prefix",
                                     required=True,
                                     help="Path to install core files.")
    install_core_parser.add_argument(
        "--core-name",
        required=True,
        choices=[
            "teensy",
            "stm32duino",
            "adafruit-samd",
        ],
        help="Name of the arduino core to install.")

    # list-boards command
    list_boards_parser = subparsers.add_parser("list-boards",
                                               help="show supported boards")
    list_boards_parser.set_defaults(func=list_boards_command)

    # list-menu-options command
    list_menu_options_parser = subparsers.add_parser(
        "list-menu-options",
        help="show available menu options for selected board")
    list_menu_options_parser.set_defaults(func=list_menu_options_command)
    list_menu_options_parser.add_argument("--board",
                                          required=True,
                                          help="Name of the board to use.")

    # show command
    show_parser = subparsers.add_parser("show",
                                        help="Return compiler information.")
    show_parser.add_argument(
        "--serial-port",
        default=serial_port,
        help="Serial port for flashing flash. Default: '{}'".format(
            serial_port))
    show_parser.add_argument(
        "--build-path",
        default=build_path,
        help="Build directory. Default: '{}'".format(build_path))
    show_parser.add_argument(
        "--project-path",
        default=build_path,
        help="Project directory. Default: '{}'".format(project_path))
    show_parser.add_argument(
        "--project-source-path",
        default=project_source_path,
        help="Project directory. Default: '{}'".format(project_source_path))
    show_parser.add_argument(
        "--build-project-name",
        default=project_name,
        help="Project name. Default: '{}'".format(build_project_name))
    show_parser.add_argument("--board",
                             required=True,
                             help="Name of the board to use.")
    show_parser.add_argument("--delimit-with-newlines",
                             help="Separate flags output with newlines.",
                             action="store_true")

    output_group = show_parser.add_mutually_exclusive_group(required=True)
    output_group.add_argument("--run-c-compile", action="store_true")
    output_group.add_argument("--run-cpp-compile", action="store_true")
    output_group.add_argument("--run-link",
                              nargs="+",
                              type=str,
                              help="Run the link command. Expected arguments: "
                              "the archive file followed by all obj files.")
    output_group.add_argument("--run-objcopy", action="store_true")
    output_group.add_argument("--run-prebuilds", action="store_true")
    output_group.add_argument("--run-postbuilds", action="store_true")
    output_group.add_argument("--c-flags", action="store_true")
    output_group.add_argument("--s-flags", action="store_true")
    output_group.add_argument("--cpp-flags", action="store_true")
    output_group.add_argument("--ld-flags", action="store_true")
    output_group.add_argument("--ar-flags", action="store_true")
    output_group.add_argument("--ld-libs", action="store_true")
    output_group.add_argument("--objcopy", help="objcopy step for SUFFIX")
    output_group.add_argument("--objcopy-flags",
                              help="objcopy flags for SUFFIX")
    output_group.add_argument("--core-path", action="store_true")
    output_group.add_argument("--cc-binary", action="store_true")
    output_group.add_argument("--cxx-binary", action="store_true")
    output_group.add_argument("--ar-binary", action="store_true")
    output_group.add_argument("--objcopy-binary", action="store_true")
    output_group.add_argument("--size-binary", action="store_true")
    output_group.add_argument("--postbuild",
                              help="Show recipe.hooks.postbuild.*.pattern")
    output_group.add_argument("--upload-tools", action="store_true")
    output_group.add_argument("--upload-command")
    output_group.add_argument("--run-upload-command")
    output_group.add_argument("--library-includes", action="store_true")
    output_group.add_argument("--library-c-files", action="store_true")
    output_group.add_argument("--library-cpp-files", action="store_true")
    output_group.add_argument("--core-c-files", action="store_true")
    output_group.add_argument("--core-s-files", action="store_true")
    output_group.add_argument("--core-cpp-files", action="store_true")
    output_group.add_argument("--variant-c-files", action="store_true")
    output_group.add_argument("--variant-s-files", action="store_true")
    output_group.add_argument("--variant-cpp-files", action="store_true")

    # nargs="+" is one or more args, e.g:
    #   --menu-options menu.usb.serialhid menu.speed.150
    show_parser.add_argument("--menu-options", nargs="+", type=str)
    show_parser.set_defaults(func=show_command)

    args = parser.parse_args()

    # Set logging level and output handler
    _LOG.setLevel(args.loglevel)
    _STDERR_HANDLER.setLevel(args.loglevel)
    _STDERR_HANDLER.setFormatter(
        logging.Formatter("[%(asctime)s] "
                          "%(levelname)s %(message)s", "%Y%m%d %H:%M:%S"))
    _LOG.addHandler(_STDERR_HANDLER)

    _LOG.debug(_pretty_format(args))

    compiler_path_override = False
    if args.compiler_path_override:
        compiler_path_override = os.path.realpath(
            os.path.expanduser(os.path.expandvars(
                args.compiler_path_override)))

    if args.subcommand == "install-core":
        args.func(args)
    elif args.subcommand in ["list-boards", "list-menu-options"]:
        builder = ArduinoBuilder(args.arduino_package_path,
                                 args.arduino_package_name)
        builder.load_board_definitions()
        args.func(args, builder)
    else:
        builder = ArduinoBuilder(args.arduino_package_path,
                                 args.arduino_package_name,
                                 build_path=args.build_path,
                                 build_project_name=args.build_project_name,
                                 project_path=args.project_path,
                                 project_source_path=args.project_source_path,
                                 compiler_path_override=compiler_path_override)
        builder.load_board_definitions()
        args.func(args, builder)

    sys.exit(0)


if __name__ == '__main__':
    main()
