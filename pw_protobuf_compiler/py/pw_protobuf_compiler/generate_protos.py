# Copyright 2019 The Pigweed Authors
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
"""Script that invokes protoc to generate code for .proto files."""

import argparse
import logging
import os
import shutil
import sys

from typing import Callable, Dict, List, Optional

import pw_cli.log
import pw_cli.process

_LOG = logging.getLogger(__name__)


def argument_parser(
    parser: Optional[argparse.ArgumentParser] = None
) -> argparse.ArgumentParser:
    """Registers the script's arguments on an argument parser."""

    if parser is None:
        parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument('--language', default='cc', help='Output language')
    parser.add_argument('--custom-plugin', help='Custom protoc plugin')
    parser.add_argument('--module-path',
                        required=True,
                        help='Path to the module containing the .proto files')
    parser.add_argument('--include-paths',
                        default=[],
                        type=lambda arg: arg.split(';'),
                        help='protoc include paths')
    parser.add_argument('--include-file',
                        type=argparse.FileType('r'),
                        help='File containing additional protoc include paths')
    parser.add_argument('--out-dir',
                        required=True,
                        help='Output directory for generated code')
    parser.add_argument('protos',
                        metavar='PROTO',
                        nargs='+',
                        help='Input protobuf files')

    return parser


def protoc_cc_args(args: argparse.Namespace) -> List[str]:
    return [
        '--plugin', f'protoc-gen-custom={shutil.which("pw_protobuf_codegen")}',
        '--custom_out', args.out_dir
    ]


def protoc_go_args(args: argparse.Namespace) -> List[str]:
    return ['--go_out', f'plugins=grpc:{args.out_dir}']


def protoc_nanopb_args(args: argparse.Namespace) -> List[str]:
    # nanopb needs to know of the include path to parse *.options files
    return [
        '--plugin', f'protoc-gen-nanopb={args.custom_plugin}',
        f'--nanopb_out=-I{args.module_path}:{args.out_dir}'
    ]


# Default additional protoc arguments for each supported language.
# TODO(frolv): Make these overridable with a command-line argument.
DEFAULT_PROTOC_ARGS: Dict[str, Callable[[argparse.Namespace], List[str]]] = {
    'cc': protoc_cc_args,
    'go': protoc_go_args,
    'nanopb': protoc_nanopb_args,
}


def main() -> int:
    """Runs protoc as configured by command-line arguments."""

    args = argument_parser().parse_args()
    os.makedirs(args.out_dir, exist_ok=True)

    try:
        lang_args = DEFAULT_PROTOC_ARGS[args.language](args)
    except KeyError:
        _LOG.error('Unsupported language: %s', args.language)
        return 1

    include_paths = [f'-I{path}' for path in args.include_paths]
    include_paths += [f'-I{line.strip()}' for line in args.include_file]

    process = pw_cli.process.run(
        'protoc',
        '-I',
        args.module_path,
        '-I',
        args.out_dir,
        *include_paths,
        *lang_args,
        *args.protos,
    )

    if process.returncode != 0:
        print(process.output.decode(), file=sys.stderr)

    return process.returncode


if __name__ == '__main__':
    pw_cli.log.install()
    sys.exit(main())
