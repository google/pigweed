# Copyright 2022 The Pigweed Authors
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
"""Options file parsing for proto generation."""

from fnmatch import fnmatchcase
from pathlib import Path
import re

from google.protobuf import text_format

from pw_protobuf_codegen_protos.codegen_options_pb2 import CodegenOptions
from pw_protobuf_protos.field_options_pb2 import FieldOptions

_MULTI_LINE_COMMENT_RE = re.compile(r'/\*.*?\*/', flags=re.MULTILINE)
_SINGLE_LINE_COMMENT_RE = re.compile(r'//.*?$', flags=re.MULTILINE)
_SHELL_STYLE_COMMENT_RE = re.compile(r'#.*?$', flags=re.MULTILINE)

# A list of (proto field path, CodegenOptions) tuples.
ParsedOptions = list[tuple[str, CodegenOptions]]


def load_options_from(options: ParsedOptions, options_file_name: Path):
    """Loads a single .options file for the given .proto"""
    with open(options_file_name) as options_file:
        # Read the options file and strip all styles of comments before parsing.
        options_data = options_file.read()
        options_data = _MULTI_LINE_COMMENT_RE.sub('', options_data)
        options_data = _SINGLE_LINE_COMMENT_RE.sub('', options_data)
        options_data = _SHELL_STYLE_COMMENT_RE.sub('', options_data)

        for line in options_data.split('\n'):
            parts = line.strip().split(None, 1)
            if len(parts) < 2:
                continue

            # Parse as a name glob followed by a protobuf text format.
            try:
                opts = CodegenOptions()
                text_format.Merge(parts[1], opts)
                options.append((parts[0], opts))
            except:  # pylint: disable=bare-except
                continue


def load_options(
    include_paths: list[Path], proto_file_name: Path
) -> ParsedOptions:
    """Loads the .options for the given .proto."""
    options: ParsedOptions = []

    for include_path in include_paths:
        options_file_name = include_path / proto_file_name.with_suffix(
            '.options'
        )
        if options_file_name.exists():
            load_options_from(options, options_file_name)

    return options


def match_options(name: str, options: ParsedOptions) -> CodegenOptions:
    """Return the matching options for a name."""
    matched = CodegenOptions()
    for name_glob, mask_options in options:
        if fnmatchcase(name, name_glob):
            matched.MergeFrom(mask_options)

    return matched


def create_from_field_options(
    field_options: FieldOptions,
) -> CodegenOptions:
    """Create a CodegenOptions from a FieldOptions."""
    codegen_options = CodegenOptions()

    if field_options.HasField('max_count'):
        codegen_options.max_count = field_options.max_count

    if field_options.HasField('max_size'):
        codegen_options.max_size = field_options.max_size

    return codegen_options


def merge_field_and_codegen_options(
    field_options: CodegenOptions, codegen_options: CodegenOptions
) -> CodegenOptions:
    """Merge inline field_options and options file codegen_options."""
    # The field options specify protocol-level requirements. Therefore, any
    # codegen options should not violate those protocol-level requirements.
    if field_options.max_count > 0 and codegen_options.max_count > 0:
        assert field_options.max_count == codegen_options.max_count

    if field_options.max_size > 0 and codegen_options.max_size > 0:
        assert field_options.max_size == codegen_options.max_size

    merged_options = CodegenOptions()
    merged_options.CopyFrom(field_options)
    merged_options.MergeFrom(codegen_options)

    return merged_options
