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
"""Outputs the contents of blobs as a hard-coded arrays in a C++ library."""

import argparse
import itertools
import json
from pathlib import Path
from string import Template
import textwrap
from typing import Any, Generator, Iterable, NamedTuple, Optional, Tuple

HEADER_PREFIX = textwrap.dedent("""\
    // This file is auto-generated; Do not hand-modify!
    // See https://pigweed.dev/pw_build/#pw-cc-blob-library for details.

    #pragma once

    #include <array>
    #include <cstddef>
    """)

SOURCE_PREFIX_TEMPLATE = Template(
    textwrap.dedent("""\
        // This file is auto-generated; Do not hand-modify!
        // See https://pigweed.dev/pw_build/#pw-cc-blob-library for details.

        #include "$header_path"

        #include <array>
        #include <cstddef>

        #include "pw_preprocessor/compiler.h"
        """))

NAMESPACE_OPEN_TEMPLATE = Template('namespace ${namespace} {\n')

NAMESPACE_CLOSE_TEMPLATE = Template('}  // namespace ${namespace}\n')

BLOB_DECLARATION_TEMPLATE = Template(
    'extern const std::array<std::byte, ${size_bytes}> ${symbol_name};')

LINKER_SECTION_TEMPLATE = Template('PW_PLACE_IN_SECTION("${linker_section}")')

BLOB_DEFINITION_SINGLE_LINE = Template(
    'const std::array<std::byte, ${size_bytes}> ${symbol_name}'
    ' = { $bytes };')

BLOB_DEFINITION_MULTI_LINE = Template(
    'const std::array<std::byte, ${size_bytes}> ${symbol_name}'
    ' = {\n${bytes_lines}\n};')

BYTES_PER_LINE = 4


class Blob(NamedTuple):
    symbol_name: str
    file_path: Path
    linker_section: Optional[str]


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--blob-file',
                        type=Path,
                        required=True,
                        help=('Path to json file containing the list of blobs '
                              'to generate.'))
    parser.add_argument('--out-source',
                        type=Path,
                        required=True,
                        help='Path at which to write .cpp file')
    parser.add_argument('--out-header',
                        type=Path,
                        required=True,
                        help='Path at which to write .h file')
    parser.add_argument('--namespace',
                        type=str,
                        required=False,
                        help=('Optional namespace that blobs will be scoped'
                              'within.'))

    return parser.parse_args()


def split_into_chunks(
        data: Iterable[Any],
        chunk_size: int) -> Generator[Tuple[Any, ...], None, None]:
    """Splits an iterable into chunks of a given size."""
    data_iterator = iter(data)
    chunk = tuple(itertools.islice(data_iterator, chunk_size))
    while chunk:
        yield chunk
        chunk = tuple(itertools.islice(data_iterator, chunk_size))


def header_from_blobs(blobs: Iterable[Blob],
                      namespace: Optional[str] = None) -> str:
    """Generate the contents of a C++ header file from blobs."""
    lines = [HEADER_PREFIX]
    if namespace:
        lines.append(NAMESPACE_OPEN_TEMPLATE.substitute(namespace=namespace))
    for blob in blobs:
        data = blob.file_path.read_bytes()
        lines.append(
            BLOB_DECLARATION_TEMPLATE.substitute(symbol_name=blob.symbol_name,
                                                 size_bytes=len(data)))
        lines.append('')
    if namespace:
        lines.append(NAMESPACE_CLOSE_TEMPLATE.substitute(namespace=namespace))

    return '\n'.join(lines)


def array_def_from_blob_data(symbol_name: str, blob_data: bytes) -> str:
    """Generates an array definition for the given blob data."""
    byte_strs = ['std::byte{{0x{:02X}}}'.format(b) for b in blob_data]

    # Try to fit the blob definition on a single line first.
    single_line_def = BLOB_DEFINITION_SINGLE_LINE.substitute(
        symbol_name=symbol_name,
        size_bytes=len(blob_data),
        bytes=', '.join(byte_strs))
    if len(single_line_def) <= 80:
        return single_line_def

    # Blob definition didn't fit on a single line; do multi-line.
    lines = []
    for byte_strs_for_line in split_into_chunks(byte_strs, BYTES_PER_LINE):
        bytes_segment = ', '.join(byte_strs_for_line)
        lines.append(f'  {bytes_segment},')
    # Removing the trailing comma from the final line of bytes.
    lines[-1] = lines[-1].rstrip(',')

    return BLOB_DEFINITION_MULTI_LINE.substitute(symbol_name=symbol_name,
                                                 size_bytes=len(blob_data),
                                                 bytes_lines='\n'.join(lines))


def source_from_blobs(blobs: Iterable[Blob],
                      header_path: Path,
                      namespace: Optional[str] = None) -> str:
    """Generate the contents of a C++ source file from blobs."""
    lines = [SOURCE_PREFIX_TEMPLATE.substitute(header_path=header_path)]
    if namespace:
        lines.append(NAMESPACE_OPEN_TEMPLATE.substitute(namespace=namespace))
    for blob in blobs:
        if blob.linker_section:
            lines.append(
                LINKER_SECTION_TEMPLATE.substitute(
                    linker_section=blob.linker_section))
        data = blob.file_path.read_bytes()
        lines.append(array_def_from_blob_data(blob.symbol_name, data))
        lines.append('')
    if namespace:
        lines.append(NAMESPACE_CLOSE_TEMPLATE.substitute(namespace=namespace))

    return '\n'.join(lines)


def load_blobs(blob_file: Path) -> Iterable[Blob]:
    with blob_file.open() as blob_fp:
        return [
            Blob(b["symbol_name"], Path(b["file_path"]),
                 b.get("linker_section")) for b in json.load(blob_fp)
        ]


def main(blob_file: Path,
         out_source: Path,
         out_header: Path,
         namespace: Optional[str] = None) -> None:
    blobs = load_blobs(blob_file)
    out_header.write_text(header_from_blobs(blobs, namespace))
    out_source.write_text(source_from_blobs(blobs, out_header, namespace))


if __name__ == '__main__':
    main(**vars(parse_args()))
