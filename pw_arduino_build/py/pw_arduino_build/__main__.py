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
"""Command line interface for arduino_builder."""

import argparse
import glob
import logging
import os
import pprint
import shlex
import subprocess
import sys
from typing import List

from pw_arduino_build import core_installer, log
from pw_arduino_build.builder import ArduinoBuilder

_LOG = logging.getLogger(__name__)

_pretty_print = pprint.PrettyPrinter(indent=1, width=120).pprint
_pretty_format = pprint.PrettyPrinter(indent=1, width=120).pformat


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
        command_line_args = shlex.split(command_line)
        process = subprocess.run(command_line_args,
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT)
        if process.returncode != 0:
            _LOG.error('Command failed with exit code %d.', process.returncode)
            _LOG.error('Full command:')
            _LOG.error('')
            _LOG.error('  %s', command_line)
            _LOG.error('')
            _LOG.error('Process output:')
            print(flush=True)
            sys.stdout.buffer.write(process.stdout)
            print(flush=True)
            _LOG.error('')


def run_command(args, builder):
    """Run sub command function.

    Runs Arduino recipes.
    """

    if args.run_prebuilds:
        run_command_lines(args, builder.get_prebuild_steps())

    if args.run_link:
        line = builder.get_link_line()
        archive_file_path = args.run_link[0]  # pylint: disable=unused-variable
        object_files = args.run_link[1:]
        line = line.replace("{object_files}", " ".join(object_files), 1)
        run_command_lines(args, [line])

    if args.run_objcopy:
        run_command_lines(args, builder.get_objcopy_steps())

    if args.run_postbuilds:
        run_command_lines(args, builder.get_postbuild_steps())

    if args.run_upload_command:
        command = builder.get_upload_line(args.run_upload_command,
                                          args.serial_port)
        run_command_lines(args, [command])


# pylint: disable=too-many-branches
def show_command(args, builder):
    """Show sub command function.

    Prints compiler info and flags.
    """
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

    elif args.c_compile:
        print(builder.get_c_compile_line())

    elif args.cpp_compile:
        print(builder.get_cpp_compile_line())

    elif args.link:
        print(builder.get_link_line())

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

    elif args.postbuild:
        print(builder.get_postbuild_line(args.postbuild))

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


def add_common_options(parser, serial_port, build_path, build_project_name,
                       project_path, project_source_path):
    """Add command line options common to the run and show commands."""
    parser.add_argument(
        "--serial-port",
        default=serial_port,
        help="Serial port for flashing flash. Default: '{}'".format(
            serial_port))
    parser.add_argument(
        "--build-path",
        default=build_path,
        help="Build directory. Default: '{}'".format(build_path))
    parser.add_argument(
        "--project-path",
        default=project_path,
        help="Project directory. Default: '{}'".format(project_path))
    parser.add_argument(
        "--project-source-path",
        default=project_source_path,
        help="Project directory. Default: '{}'".format(project_source_path))
    parser.add_argument(
        "--build-project-name",
        default=build_project_name,
        help="Project name. Default: '{}'".format(build_project_name))
    parser.add_argument("--board",
                        required=True,
                        help="Name of the board to use.")
    # nargs="+" is one or more args, e.g:
    #   --menu-options menu.usb.serialhid menu.speed.150
    parser.add_argument("--menu-options", nargs="+", type=str)


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
    install_core_parser.set_defaults(func=core_installer.install_core_command)
    install_core_parser.add_argument("--prefix",
                                     required=True,
                                     help="Path to install core files.")
    install_core_parser.add_argument(
        "--core-name",
        required=True,
        choices=core_installer.supported_cores(),
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
    add_common_options(show_parser, serial_port, build_path,
                       build_project_name, project_path, project_source_path)
    show_parser.add_argument("--delimit-with-newlines",
                             help="Separate flag output with newlines.",
                             action="store_true")

    output_group = show_parser.add_mutually_exclusive_group(required=True)
    output_group.add_argument("--c-compile", action="store_true")
    output_group.add_argument("--cpp-compile", action="store_true")
    output_group.add_argument("--link", action="store_true")
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
    output_group.add_argument("--library-includes", action="store_true")
    output_group.add_argument("--library-c-files", action="store_true")
    output_group.add_argument("--library-cpp-files", action="store_true")
    output_group.add_argument("--core-c-files", action="store_true")
    output_group.add_argument("--core-s-files", action="store_true")
    output_group.add_argument("--core-cpp-files", action="store_true")
    output_group.add_argument("--variant-c-files", action="store_true")
    output_group.add_argument("--variant-s-files", action="store_true")
    output_group.add_argument("--variant-cpp-files", action="store_true")

    show_parser.set_defaults(func=show_command)

    # run command
    run_parser = subparsers.add_parser("run", help="Run Arduino recipes.")
    add_common_options(run_parser, serial_port, build_path, build_project_name,
                       project_path, project_source_path)
    run_parser.add_argument("--run-link",
                            nargs="+",
                            type=str,
                            help="Run the link command. Expected arguments: "
                            "the archive file followed by all obj files.")
    run_parser.add_argument("--run-objcopy", action="store_true")
    run_parser.add_argument("--run-prebuilds", action="store_true")
    run_parser.add_argument("--run-postbuilds", action="store_true")
    run_parser.add_argument("--run-upload-command")

    run_parser.set_defaults(func=run_command)

    args = parser.parse_args()

    log.install()
    log.set_level(args.loglevel)

    _LOG.debug(_pretty_format(args))

    # Check for and set alternate compiler path.
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
        builder.select_board(args.board, args.menu_options)
        args.func(args, builder)

    sys.exit(0)


if __name__ == '__main__':
    main()
