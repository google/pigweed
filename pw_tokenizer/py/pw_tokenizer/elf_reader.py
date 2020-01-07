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
"""Reads data from ELF sections.

This module provides tools for dumping the contents of an ELF section. It can
also be used to read values at a particular address. A command line interface
for both of these features is provided.
"""

import argparse
import collections
import re
import struct
import sys
from typing import BinaryIO, Dict, Iterable, NamedTuple, Optional, Tuple, Union


class Field(NamedTuple):
    """A field in an ELF file.

    Fields refer to a particular piece of data in an ELF file or section header.
    """

    name: str
    offset_32: int
    offset_64: int
    size_32: int
    size_64: int


class _FileHeader(NamedTuple):
    """Fields in the ELF file header."""

    section_header_offset: Field = Field('e_shoff', 0x20, 0x28, 4, 8)
    section_count: Field = Field('e_shnum', 0x30, 0x3C, 2, 2)
    section_names_index: Field = Field('e_shstrndx', 0x32, 0x3E, 2, 2)


FILE_HEADER = _FileHeader()


class _SectionHeader(NamedTuple):
    """Fields in an ELF section header."""

    section_name_offset: Field = Field('sh_name', 0x00, 0x00, 4, 4)
    section_address: Field = Field('sh_addr', 0x0C, 0x10, 4, 8)
    section_offset: Field = Field('sh_offset', 0x10, 0x18, 4, 8)
    section_size: Field = Field('sh_size', 0x14, 0x20, 4, 8)

    # section_header_end records the size of the header.
    section_header_end: Field = Field('section end', 0x28, 0x40, 0, 0)


SECTION_HEADER = _SectionHeader()


def read_c_string(fd: BinaryIO) -> bytes:
    """Reads a null-terminated string from the provided file descriptor."""
    string = bytearray()
    while True:
        byte = fd.read(1)
        if not byte or byte == b'\0':
            return bytes(string)
        string += byte


def file_is_elf(fd: BinaryIO) -> bool:
    """Returns true if the provided file starts with the ELF magic number."""
    try:
        fd.seek(0)
        magic_number = fd.read(4)
        fd.seek(0)
        return magic_number == b'\x7fELF'
    except IOError:
        return False


class ElfDecodeError(Exception):
    """Invalid data was read from an ELF file."""


class FieldReader:
    """Reads ELF fields defined with a Field tuple from an ELF file."""
    def __init__(self, elf: BinaryIO):
        if not file_is_elf(elf):
            raise ElfDecodeError(r"ELF files must start with b'\x7fELF'")

        self._elf = elf

        int_unpacker = self._determine_integer_format()

        # Set up decoding based on the address size
        self._elf.seek(0x04)  # e_ident[EI_CLASS] (address size)
        size_field = self._elf.read(1)

        if size_field == b'\x01':
            self.offset = lambda field: field.offset_32
            self._size = lambda field: field.size_32
            self._decode = lambda f, d: int_unpacker[f.size_32].unpack(d)[0]
        elif size_field == b'\x02':
            self.offset = lambda field: field.offset_64
            self._size = lambda field: field.size_64
            self._decode = lambda f, d: int_unpacker[f.size_64].unpack(d)[0]
        else:
            raise ElfDecodeError('Unknown size {!r}'.format(size_field))

    def _determine_integer_format(self) -> Dict[int, struct.Struct]:
        """Returns a dict of structs used for converting bytes to integers."""
        self._elf.seek(0x05)  # e_ident[EI_DATA] (endianness)
        endianness_byte = self._elf.read(1)
        if endianness_byte == b'\x01':
            endianness = '<'
        elif endianness_byte == b'\x02':
            endianness = '>'
        else:
            raise ElfDecodeError(
                'Unknown endianness {!r}'.format(endianness_byte))

        return {
            1: struct.Struct(endianness + 'B'),
            2: struct.Struct(endianness + 'H'),
            4: struct.Struct(endianness + 'I'),
            8: struct.Struct(endianness + 'Q'),
        }

    def read(self, field: Field, base: int = 0) -> int:
        self._elf.seek(base + self.offset(field))
        data = self._elf.read(self._size(field))
        return self._decode(field, data)

    def read_string(self, address: int) -> str:
        self._elf.seek(address)
        return read_c_string(self._elf).decode()


class Elf:
    """Represents an ELF file and the sections in it."""
    class Section:
        """Info about a section in an ELF file."""
        def __init__(self, name: str, address: int, offset: int, size: int):
            self.name = name
            self.address = address
            self.offset = offset
            self.size = size

        def range(self) -> range:
            return range(self.address, self.address + self.size)

        def __lt__(self, other) -> bool:
            return self.address < other.address

        def __str__(self) -> str:
            return ('Section(name={self.name}, address=0x{self.address:08x} '
                    'offset=0x{self.offset:x} size=0x{self.size:x})').format(
                        self=self)

        def __repr__(self) -> str:
            return str(self)

    def __init__(self, elf: BinaryIO):
        self._elf = elf
        self.sections: Tuple[Elf.Section, ...] = tuple(self._list_sections())
        self.sections_by_name: Dict[str,
                                    Elf.Section] = collections.OrderedDict(
                                        (section.name, section)
                                        for section in self.sections)

    def _list_sections(self) -> Iterable['Elf.Section']:
        """Reads the section headers to enumerate all ELF sections."""
        reader = FieldReader(self._elf)
        base = reader.read(FILE_HEADER.section_header_offset)
        section_header_size = reader.offset(SECTION_HEADER.section_header_end)

        # Find the section with the section names in it
        names_section_header_base = base + section_header_size * reader.read(
            FILE_HEADER.section_names_index)
        names_table_base = reader.read(SECTION_HEADER.section_offset,
                                       names_section_header_base)

        base = reader.read(FILE_HEADER.section_header_offset)
        for _ in range(reader.read(FILE_HEADER.section_count)):
            name_offset = reader.read(SECTION_HEADER.section_name_offset, base)

            yield self.Section(
                reader.read_string(names_table_base + name_offset),
                reader.read(SECTION_HEADER.section_address, base),
                reader.read(SECTION_HEADER.section_offset, base),
                reader.read(SECTION_HEADER.section_size, base))

            base += section_header_size

    def section_by_address(self, address: int) -> Optional['Elf.Section']:
        """Returns the section that contains the provided address, if any."""
        # Iterate in reverse to give priority to sections with nonzero addresses
        for section in sorted(self.sections, reverse=True):
            if address in section.range():
                return section

        return None

    def read_value(self,
                   address: int,
                   size: Optional[int] = None) -> Union[None, bytes, int]:
        """Reads specified bytes or null-terminated string at address."""
        section = self.section_by_address(address)
        if not section:
            return None

        assert section.address <= address
        self._elf.seek(section.offset + address - section.address)

        if size is None:
            return read_c_string(self._elf)

        return self._elf.read(size)

    def dump_section(self, name: str) -> Optional[bytes]:
        """Dumps section contents as a byte string; None if no match."""
        try:
            section = self.sections_by_name[name]
        except KeyError:
            return None

        self._elf.seek(section.offset)
        return self._elf.read(section.size)

    def dump_sections(self, name_regex) -> Optional[bytes]:
        """Dumps a binary string containing the sections matching name_regex."""
        name_regex = re.compile(name_regex)

        sections = []
        for section in self.sections:
            if name_regex.match(section.name):
                self._elf.seek(section.offset)
                sections.append(self._elf.read(section.size))

        return b''.join(sections) if sections else None

    def summary(self) -> str:
        return '\n'.join(
            '[{0:2}] {1.address:08x} {1.offset:08x} {1.size:08x} {1.name}'.
            format(i, section) for i, section in enumerate(self.sections))

    def __str__(self) -> str:
        return 'Elf({}\n)'.format(''.join('\n  {},'.format(s)
                                          for s in self.sections))


def _read_addresses(elf, size: int, output, address: Iterable[int]) -> None:
    for addr in address:
        value = elf.read_value(addr, size)

        if value is None:
            raise ValueError('Invalid address 0x{:08x}'.format(addr))

        output(value)


def _dump_sections(elf: Elf, output, name: str, regex) -> None:
    if not name and not regex:
        output(elf.summary().encode())
        return

    for section in name:
        output(elf.dump_section(section))

    for section_pattern in regex:
        output(elf.dump_sections(section_pattern))


def _parse_args() -> argparse.Namespace:
    """Parses and returns command line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)

    def hex_int(arg):
        return int(arg, 16)

    parser.add_argument('-e',
                        '--elf',
                        type=argparse.FileType('rb'),
                        help='the ELF file to examine',
                        required=True)

    parser.add_argument(
        '-d',
        '--delimiter',
        default=ord('\n'),
        type=int,
        help=r'delimiter to write after each value; \n by default')

    parser.set_defaults(handler=lambda **_: parser.print_help())

    subparsers = parser.add_subparsers(
        help='select whether to work with addresses or whole sections')

    section_parser = subparsers.add_parser('section')
    section_parser.set_defaults(handler=_dump_sections)
    section_parser.add_argument('-n', '--name', default=[], action='append')
    section_parser.add_argument('-r', '--regex', default=[], action='append')

    address_parser = subparsers.add_parser('address')
    address_parser.set_defaults(handler=_read_addresses)
    address_parser.add_argument(
        '--size',
        type=int,
        help='the size to read; reads until a null terminator by default')
    address_parser.add_argument('address',
                                nargs='+',
                                type=hex_int,
                                help='hexadecimal addresses to read')

    return parser.parse_args()


def _main(args):
    """Calls the appropriate handler for the command line options."""
    handler = args.handler
    del args.handler

    delim = args.delimiter
    del args.delimiter

    def output(value):
        if value is not None:
            sys.stdout.buffer.write(value)
            sys.stdout.buffer.write(bytearray([delim]))
            sys.stdout.flush()

    args.output = output
    args.elf = Elf(args.elf)

    handler(**vars(args))


if __name__ == '__main__':
    _main(_parse_args())
