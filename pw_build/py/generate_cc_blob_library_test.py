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
"""Unit tests for generate_cc_blob_library.py"""

from pathlib import Path
import tempfile
import textwrap
import unittest

from pw_build import generate_cc_blob_library


class TestSplitIntoChunks(unittest.TestCase):
    """Unit tests for the split_into_chunks() function."""
    def test_perfect_split(self):
        """Tests basic splitting where the iterable divides perfectly."""
        data = (1, 7, 0, 1)
        self.assertEqual(
            ((1, 7), (0, 1)),
            tuple(generate_cc_blob_library.split_into_chunks(data, 2)))

    def test_split_with_remainder(self):
        """Tests basic splitting where there is a remainder."""
        data = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10)
        self.assertEqual(
            ((1, 2, 3), (4, 5, 6), (7, 8, 9), (10, )),
            tuple(generate_cc_blob_library.split_into_chunks(data, 3)))


class TestHeaderFromBlobs(unittest.TestCase):
    """Unit tests for the header_from_blobs() function."""
    def test_single_blob_header(self):
        """Tests the generation of a header for a single blob."""
        foo_blob = Path(tempfile.NamedTemporaryFile().name)
        foo_blob.write_bytes(bytes((1, 2, 3, 4, 5, 6)))
        blobs = [generate_cc_blob_library.Blob('fooBlob', foo_blob, None)]

        header = generate_cc_blob_library.header_from_blobs(blobs)
        expected_header = textwrap.dedent("""\
            // This file is auto-generated; Do not hand-modify!
            // See https://pigweed.dev/pw_build/#pw-cc-blob-library for details.

            #pragma once

            #include <array>
            #include <cstddef>

            extern const std::array<std::byte, 6> fooBlob;
            """)

        self.assertEqual(expected_header, header)

    def test_multi_blob_header(self):
        """Tests the generation of a header for multiple blobs."""
        foo_blob = Path(tempfile.NamedTemporaryFile().name)
        foo_blob.write_bytes(bytes((1, 2, 3, 4, 5, 6)))
        bar_blob = Path(tempfile.NamedTemporaryFile().name)
        bar_blob.write_bytes(bytes((10, 9, 8, 7, 6)))
        blobs = [
            generate_cc_blob_library.Blob('fooBlob', foo_blob, None),
            generate_cc_blob_library.Blob('barBlob', bar_blob, None),
        ]

        header = generate_cc_blob_library.header_from_blobs(blobs)
        expected_header = textwrap.dedent("""\
            // This file is auto-generated; Do not hand-modify!
            // See https://pigweed.dev/pw_build/#pw-cc-blob-library for details.

            #pragma once

            #include <array>
            #include <cstddef>

            extern const std::array<std::byte, 6> fooBlob;

            extern const std::array<std::byte, 5> barBlob;
            """)

        self.assertEqual(expected_header, header)

    def test_header_with_namespace(self):
        """Tests the header generation of namespace definitions."""
        foo_blob = Path(tempfile.NamedTemporaryFile().name)
        foo_blob.write_bytes(bytes((1, 2, 3, 4, 5, 6)))
        blobs = [generate_cc_blob_library.Blob('fooBlob', foo_blob, None)]

        header = generate_cc_blob_library.header_from_blobs(blobs, 'pw::foo')
        expected_header = textwrap.dedent("""\
            // This file is auto-generated; Do not hand-modify!
            // See https://pigweed.dev/pw_build/#pw-cc-blob-library for details.

            #pragma once

            #include <array>
            #include <cstddef>

            namespace pw::foo {

            extern const std::array<std::byte, 6> fooBlob;

            }  // namespace pw::foo
            """)

        self.assertEqual(expected_header, header)


class TestArrayDefFromBlobData(unittest.TestCase):
    """Unit tests for the array_def_from_blob_data() function."""
    def test_single_line(self):
        """Tests the generation of single-line array definitions."""
        foo_data = bytes((1, 2))

        foo_definition = generate_cc_blob_library.array_def_from_blob_data(
            'fooBlob', foo_data)
        expected_definition = ('const std::array<std::byte, 2> fooBlob'
                               ' = { std::byte{0x01}, std::byte{0x02} };')

        self.assertEqual(expected_definition, foo_definition)

    def test_multi_line(self):
        """Tests the generation of multi-line array definitions."""
        foo_data = bytes((1, 2, 3, 4, 5, 6, 7, 8, 9, 10))

        foo_definition = generate_cc_blob_library.array_def_from_blob_data(
            'fooBlob', foo_data)
        expected_definition = ('const std::array<std::byte, 10> fooBlob = {\n'
                               '  std::byte{0x01}, std::byte{0x02}, '
                               'std::byte{0x03}, std::byte{0x04},\n'
                               '  std::byte{0x05}, std::byte{0x06}, '
                               'std::byte{0x07}, std::byte{0x08},\n'
                               '  std::byte{0x09}, std::byte{0x0A}\n'
                               '};')

        self.assertEqual(expected_definition, foo_definition)


class TestSourceFromBlobs(unittest.TestCase):
    """Unit tests for the source_from_blobs() function."""
    def test_source_with_mixed_blobs(self):
        """Tests generation of a source file with single- and multi-liners."""
        foo_blob = Path(tempfile.NamedTemporaryFile().name)
        foo_blob.write_bytes(bytes((1, 2)))
        bar_blob = Path(tempfile.NamedTemporaryFile().name)
        bar_blob.write_bytes(bytes((1, 2, 3, 4, 5, 6, 7, 8, 9, 10)))
        blobs = [
            generate_cc_blob_library.Blob('fooBlob', foo_blob, None),
            generate_cc_blob_library.Blob('barBlob', bar_blob, None),
        ]

        source = generate_cc_blob_library.source_from_blobs(
            blobs, 'path/to/header.h')
        expected_source = textwrap.dedent("""\
            // This file is auto-generated; Do not hand-modify!
            // See https://pigweed.dev/pw_build/#pw-cc-blob-library for details.

            #include "path/to/header.h"

            #include <array>
            #include <cstddef>

            #include "pw_preprocessor/compiler.h"

            """)
        expected_source += ('const std::array<std::byte, 2> fooBlob'
                            ' = { std::byte{0x01}, std::byte{0x02} };\n\n')
        expected_source += ('const std::array<std::byte, 10> barBlob = {\n'
                            '  std::byte{0x01}, std::byte{0x02}, '
                            'std::byte{0x03}, std::byte{0x04},\n'
                            '  std::byte{0x05}, std::byte{0x06}, '
                            'std::byte{0x07}, std::byte{0x08},\n'
                            '  std::byte{0x09}, std::byte{0x0A}\n'
                            '};\n')

        self.assertEqual(expected_source, source)

    def test_source_with_namespace(self):
        """Tests the source generation of namespace definitions."""
        foo_blob = Path(tempfile.NamedTemporaryFile().name)
        foo_blob.write_bytes(bytes((1, 2)))
        blobs = [generate_cc_blob_library.Blob('fooBlob', foo_blob, None)]

        source = generate_cc_blob_library.source_from_blobs(
            blobs, 'path/to/header.h', 'pw::foo')
        expected_source = textwrap.dedent("""\
            // This file is auto-generated; Do not hand-modify!
            // See https://pigweed.dev/pw_build/#pw-cc-blob-library for details.

            #include "path/to/header.h"

            #include <array>
            #include <cstddef>

            #include "pw_preprocessor/compiler.h"

            namespace pw::foo {

            const std::array<std::byte, 2> fooBlob = { std::byte{0x01}, std::byte{0x02} };

            }  // namespace pw::foo
            """)

        self.assertEqual(expected_source, source)

    def test_source_with_linker_sections(self):
        """Tests generation of a source file with defined linker sections"""
        foo_blob = Path(tempfile.NamedTemporaryFile().name)
        foo_blob.write_bytes(bytes((1, 2)))
        bar_blob = Path(tempfile.NamedTemporaryFile().name)
        bar_blob.write_bytes(bytes((1, 2, 3, 4, 5, 6, 7, 8, 9, 10)))
        blobs = [
            generate_cc_blob_library.Blob('fooBlob', foo_blob, ".foo_section"),
            generate_cc_blob_library.Blob('barBlob', bar_blob, ".bar_section"),
        ]

        source = generate_cc_blob_library.source_from_blobs(
            blobs, 'path/to/header.h')
        expected_source = textwrap.dedent("""\
            // This file is auto-generated; Do not hand-modify!
            // See https://pigweed.dev/pw_build/#pw-cc-blob-library for details.

            #include "path/to/header.h"

            #include <array>
            #include <cstddef>

            #include "pw_preprocessor/compiler.h"

            """)
        expected_source += ('PW_PLACE_IN_SECTION(".foo_section")\n'
                            'const std::array<std::byte, 2> fooBlob'
                            ' = { std::byte{0x01}, std::byte{0x02} };\n\n')
        expected_source += ('PW_PLACE_IN_SECTION(".bar_section")\n'
                            'const std::array<std::byte, 10> barBlob = {\n'
                            '  std::byte{0x01}, std::byte{0x02}, '
                            'std::byte{0x03}, std::byte{0x04},\n'
                            '  std::byte{0x05}, std::byte{0x06}, '
                            'std::byte{0x07}, std::byte{0x08},\n'
                            '  std::byte{0x09}, std::byte{0x0A}\n'
                            '};\n')

        self.assertEqual(expected_source, source)


if __name__ == '__main__':
    unittest.main()
