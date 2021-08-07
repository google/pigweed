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
"""Tool for processing and outputting Snapshot protos as text"""

import argparse
import sys
from pathlib import Path
from typing import Optional, BinaryIO, TextIO, Callable
import pw_tokenizer
import pw_cpu_exception_cortex_m
from pw_snapshot_metadata import metadata
from pw_snapshot_protos import snapshot_pb2
from pw_symbolizer import LlvmSymbolizer
from pw_thread import thread_analyzer

_BRANDING = """
        ____ _       __    _____ _   _____    ____  _____ __  ______  ______
       / __ \\ |     / /   / ___// | / /   |  / __ \\/ ___// / / / __ \\/_  __/
      / /_/ / | /| / /    \\__ \\/  |/ / /| | / /_/ /\\__ \\/ /_/ / / / / / /
     / ____/| |/ |/ /    ___/ / /|  / ___ |/ ____/___/ / __  / /_/ / / /
    /_/     |__/|__/____/____/_/ |_/_/  |_/_/    /____/_/ /_/\\____/ /_/
                  /_____/

"""

# ELF files are useful for symbolizing addresses in snapshots. As a single
# snapshot may contain embedded snapshots from multiple devices, there's a need
# to match ELF files to the correct snapshot to correctly symbolize addresses.
#
# An ElfMatcher is a function that takes a snapshot and investigates its
# metadata (often build ID, device name, or the version string) to determine
# whether a suitable ELF file can be provided for symbolization.
ElfMatcher = Callable[[snapshot_pb2.Snapshot], Optional[Path]]


def process_snapshot(serialized_snapshot: bytes,
                     detokenizer: Optional[pw_tokenizer.Detokenizer] = None,
                     elf_matcher: Optional[ElfMatcher] = None) -> str:
    """Processes a single snapshot."""

    output = [_BRANDING]

    captured_metadata = metadata.process_snapshot(serialized_snapshot,
                                                  detokenizer)
    if captured_metadata:
        output.append(captured_metadata)

    # Open a symbolizer.
    snapshot = snapshot_pb2.Snapshot()
    snapshot.ParseFromString(serialized_snapshot)
    if elf_matcher is not None:
        symbolizer = LlvmSymbolizer(elf_matcher(snapshot))
    else:
        symbolizer = LlvmSymbolizer()

    cortex_m_cpu_state = pw_cpu_exception_cortex_m.process_snapshot(
        serialized_snapshot)
    if cortex_m_cpu_state:
        output.append(cortex_m_cpu_state)

    thread_info = thread_analyzer.process_snapshot(serialized_snapshot,
                                                   detokenizer, symbolizer)
    if thread_info:
        output.append(thread_info)

    # Check and emit the number of related snapshots embedded in this snapshot.
    if snapshot.related_snapshots:
        snapshot_count = len(snapshot.related_snapshots)
        plural = 's' if snapshot_count > 1 else ''
        output.extend((
            f'This snapshot contains {snapshot_count} related snapshot{plural}',
            '',
        ))

    return '\n'.join(output)


def process_snapshots(
        serialized_snapshot: bytes,
        detokenizer: Optional[pw_tokenizer.Detokenizer] = None,
        elf_matcher: Optional[ElfMatcher] = None,
        user_processing_callback: Optional[Callable[[bytes],
                                                    str]] = None) -> str:
    """Processes a snapshot that may have multiple embedded snapshots."""
    output = []
    # Process the top-level snapshot.
    output.append(
        process_snapshot(serialized_snapshot, detokenizer, elf_matcher))

    # If the user provided a custom processing callback, call it on each
    # snapshot.
    if user_processing_callback is not None:
        output.append(user_processing_callback(serialized_snapshot))

    # Process any related snapshots that were embedded in this one.
    snapshot = snapshot_pb2.Snapshot()
    snapshot.ParseFromString(serialized_snapshot)
    for nested_snapshot in snapshot.related_snapshots:
        output.append('\n[' + '=' * 78 + ']\n')
        output.append(
            str(
                process_snapshots(nested_snapshot.SerializeToString(),
                                  detokenizer, elf_matcher)))

    return '\n'.join(output)


def _load_and_dump_snapshots(in_file: BinaryIO, out_file: TextIO,
                             token_db: Optional[TextIO]):
    detokenizer = None
    if token_db:
        detokenizer = pw_tokenizer.Detokenizer(token_db)
    out_file.write(process_snapshots(in_file.read(), detokenizer))


def _parse_args():
    parser = argparse.ArgumentParser(description='Decode Pigweed snapshots')
    parser.add_argument('in_file',
                        type=argparse.FileType('rb'),
                        help='Binary snapshot file')
    parser.add_argument(
        '--out-file',
        '-o',
        default='-',
        type=argparse.FileType('wb'),
        help='File to output decoded snapshots to. Defaults to stdout.')
    parser.add_argument(
        '--token-db',
        type=argparse.FileType('r'),
        help='Token database or ELF file to use for detokenization.')
    return parser.parse_args()


if __name__ == '__main__':
    _load_and_dump_snapshots(**vars(_parse_args()))
    sys.exit(0)
