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
"""Builds and manages databases of tokenized strings."""

from abc import abstractmethod
import collections
import csv
from dataclasses import dataclass
from datetime import datetime
import io
import logging
from pathlib import Path
import re
import struct
import subprocess
from typing import (BinaryIO, Callable, Dict, Iterable, Iterator, List,
                    NamedTuple, Optional, Pattern, TextIO, Tuple, Union,
                    ValuesView)
from uuid import uuid4

DATE_FORMAT = '%Y-%m-%d'
DEFAULT_DOMAIN = ''

# The default hash length to use. This value only applies when hashing strings
# from a legacy-style ELF with plain strings. New tokenized string entries
# include the token alongside the string.
#
# This MUST match the default value of PW_TOKENIZER_CFG_C_HASH_LENGTH in
# pw_tokenizer/public/pw_tokenizer/config.h.
DEFAULT_C_HASH_LENGTH = 128

TOKENIZER_HASH_CONSTANT = 65599

_LOG = logging.getLogger('pw_tokenizer')


def _value(char: Union[int, str]) -> int:
    return char if isinstance(char, int) else ord(char)


def pw_tokenizer_65599_hash(string: Union[str, bytes],
                            hash_length: int = None) -> int:
    """Hashes the provided string.

    This hash function is only used when adding tokens from legacy-style
    tokenized strings in an ELF, which do not include the token.
    """
    hash_value = len(string)
    coefficient = TOKENIZER_HASH_CONSTANT

    for char in string[:hash_length]:
        hash_value = (hash_value + coefficient * _value(char)) % 2**32
        coefficient = (coefficient * TOKENIZER_HASH_CONSTANT) % 2**32

    return hash_value


def default_hash(string: Union[str, bytes]) -> int:
    return pw_tokenizer_65599_hash(string, DEFAULT_C_HASH_LENGTH)


class _EntryKey(NamedTuple):
    """Uniquely refers to an entry."""
    token: int
    string: str


@dataclass(eq=True, order=False)
class TokenizedStringEntry:
    """A tokenized string with its metadata."""
    token: int
    string: str
    domain: str = DEFAULT_DOMAIN
    date_removed: Optional[datetime] = None

    def key(self) -> _EntryKey:
        """The key determines uniqueness for a tokenized string."""
        return _EntryKey(self.token, self.string)

    def update_date_removed(self,
                            new_date_removed: Optional[datetime]) -> None:
        """Sets self.date_removed if the other date is newer."""
        # No removal date (None) is treated as the newest date.
        if self.date_removed is None:
            return

        if new_date_removed is None or new_date_removed > self.date_removed:
            self.date_removed = new_date_removed

    def __lt__(self, other) -> bool:
        """Sorts the entry by token, date removed, then string."""
        if self.token != other.token:
            return self.token < other.token

        # Sort removal dates in reverse, so the most recently removed (or still
        # present) entry appears first.
        if self.date_removed != other.date_removed:
            return (other.date_removed or datetime.max) < (self.date_removed
                                                           or datetime.max)

        return self.string < other.string

    def __str__(self) -> str:
        return self.string


class Database:
    """Database of tokenized strings stored as TokenizedStringEntry objects."""
    def __init__(self, entries: Iterable[TokenizedStringEntry] = ()):
        """Creates a token database."""
        # The database dict stores each unique (token, string) entry.
        self._database: Dict[_EntryKey, TokenizedStringEntry] = {}

        # This is a cache for fast token lookup that is built as needed.
        self._cache: Optional[Dict[int, List[TokenizedStringEntry]]] = None

        self.add(entries)

    @classmethod
    def from_strings(
            cls,
            strings: Iterable[str],
            domain: str = DEFAULT_DOMAIN,
            tokenize: Callable[[str], int] = default_hash) -> 'Database':
        """Creates a Database from an iterable of strings."""
        return cls((TokenizedStringEntry(tokenize(string), string, domain)
                    for string in strings))

    @classmethod
    def merged(cls, *databases: 'Database') -> 'Database':
        """Creates a TokenDatabase from one or more other databases."""
        db = cls()
        db.merge(*databases)
        return db

    @property
    def token_to_entries(self) -> Dict[int, List[TokenizedStringEntry]]:
        """Returns a dict that maps tokens to a list of TokenizedStringEntry."""
        if self._cache is None:  # build cache token -> entry cache
            self._cache = collections.defaultdict(list)
            for entry in self._database.values():
                self._cache[entry.token].append(entry)

        return self._cache

    def entries(self) -> ValuesView[TokenizedStringEntry]:
        """Returns iterable over all TokenizedStringEntries in the database."""
        return self._database.values()

    def collisions(self) -> Iterator[Tuple[int, List[TokenizedStringEntry]]]:
        """Returns tuple of (token, entries_list)) for all colliding tokens."""
        for token, entries in self.token_to_entries.items():
            if len(entries) > 1:
                yield token, entries

    def mark_removed(
            self,
            all_entries: Iterable[TokenizedStringEntry],
            removal_date: Optional[datetime] = None
    ) -> List[TokenizedStringEntry]:
        """Marks entries missing from all_entries as having been removed.

        The entries are assumed to represent the complete set of entries for the
        database. Entries currently in the database not present in the provided
        entries are marked with a removal date but remain in the database.
        Entries in all_entries missing from the database are NOT added; call the
        add function to add these.

        Args:
          all_entries: the complete set of strings present in the database
          removal_date: the datetime for removed entries; today by default

        Returns:
          A list of entries marked as removed.
        """
        self._cache = None

        if removal_date is None:
            removal_date = datetime.now()

        all_keys = frozenset(entry.key() for entry in all_entries)

        removed = []

        for entry in self._database.values():
            if (entry.key() not in all_keys
                    and (entry.date_removed is None
                         or removal_date < entry.date_removed)):
                # Add a removal date, or update it to the oldest date.
                entry.date_removed = removal_date
                removed.append(entry)

        return removed

    def add(self, entries: Iterable[TokenizedStringEntry]) -> None:
        """Adds new entries and updates date_removed for existing entries.

        If the added tokens have removal dates, the newest date is used.
        """
        self._cache = None

        for new_entry in entries:
            # Update an existing entry or create a new one.
            try:
                entry = self._database[new_entry.key()]
                entry.domain = new_entry.domain

                # Keep the latest removal date between the two entries.
                if new_entry.date_removed is None:
                    entry.date_removed = None
                elif (entry.date_removed
                      and entry.date_removed < new_entry.date_removed):
                    entry.date_removed = new_entry.date_removed
            except KeyError:
                # Make a copy to avoid unintentially updating the database.
                self._database[new_entry.key()] = TokenizedStringEntry(
                    **vars(new_entry))

    def purge(
        self,
        date_removed_cutoff: Optional[datetime] = None
    ) -> List[TokenizedStringEntry]:
        """Removes and returns entries removed on/before date_removed_cutoff."""
        self._cache = None

        if date_removed_cutoff is None:
            date_removed_cutoff = datetime.max

        to_delete = [
            entry for _, entry in self._database.items()
            if entry.date_removed and entry.date_removed <= date_removed_cutoff
        ]

        for entry in to_delete:
            del self._database[entry.key()]

        return to_delete

    def merge(self, *databases: 'Database') -> None:
        """Merges two or more databases together, keeping the newest dates."""
        self._cache = None

        for other_db in databases:
            for entry in other_db.entries():
                key = entry.key()

                if key in self._database:
                    self._database[key].update_date_removed(entry.date_removed)
                else:
                    self._database[key] = entry

    def filter(
        self,
        include: Iterable[Union[str, Pattern[str]]] = (),
        exclude: Iterable[Union[str, Pattern[str]]] = (),
        replace: Iterable[Tuple[Union[str, Pattern[str]], str]] = ()
    ) -> None:
        """Filters the database using regular expressions (strings or compiled).

        Args:
          include: regexes; only entries matching at least one are kept
          exclude: regexes; entries matching any of these are removed
          replace: (regex, str) tuples; replaces matching terms in all entries
        """
        self._cache = None

        to_delete: List[_EntryKey] = []

        if include:
            include_re = [re.compile(pattern) for pattern in include]
            to_delete.extend(
                key for key, val in self._database.items()
                if not any(rgx.search(val.string) for rgx in include_re))

        if exclude:
            exclude_re = [re.compile(pattern) for pattern in exclude]
            to_delete.extend(key for key, val in self._database.items() if any(
                rgx.search(val.string) for rgx in exclude_re))

        for key in to_delete:
            del self._database[key]

        for search, replacement in replace:
            search = re.compile(search)

            for value in self._database.values():
                value.string = search.sub(replacement, value.string)

    def __len__(self) -> int:
        """Returns the number of entries in the database."""
        return len(self.entries())

    def __str__(self) -> str:
        """Outputs the database as CSV."""
        csv_output = io.BytesIO()
        write_csv(self, csv_output)
        return csv_output.getvalue().decode()


def parse_csv(fd: TextIO) -> Iterable[TokenizedStringEntry]:
    """Parses TokenizedStringEntries from a CSV token database file."""
    for line in csv.reader(fd):
        try:
            token_str, date_str, string_literal = line

            token = int(token_str, 16)
            date = (datetime.strptime(date_str, DATE_FORMAT)
                    if date_str.strip() else None)

            yield TokenizedStringEntry(token, string_literal, DEFAULT_DOMAIN,
                                       date)
        except (ValueError, UnicodeDecodeError) as err:
            _LOG.error('Failed to parse tokenized string entry %s: %s', line,
                       err)


def write_csv(database: Database, fd: BinaryIO) -> None:
    """Writes the database as CSV to the provided binary file."""
    for entry in sorted(database.entries()):
        _write_csv_line(fd, entry)


def _write_csv_line(fd: BinaryIO, entry: TokenizedStringEntry):
    """Write a line in CSV format to the provided binary file."""
    # Align the CSV output to 10-character columns for improved readability.
    # Use \n instead of RFC 4180's \r\n.
    fd.write('{:08x},{:10},"{}"\n'.format(
        entry.token,
        entry.date_removed.strftime(DATE_FORMAT) if entry.date_removed else '',
        entry.string.replace('"', '""')).encode())  # escape " as ""


class _BinaryFileFormat(NamedTuple):
    """Attributes of the binary token database file format."""

    magic: bytes = b'TOKENS\0\0'
    header: struct.Struct = struct.Struct('<8sI4x')
    entry: struct.Struct = struct.Struct('<IBBH')


BINARY_FORMAT = _BinaryFileFormat()


class DatabaseFormatError(Exception):
    """Failed to parse a token database file."""


def file_is_binary_database(fd: BinaryIO) -> bool:
    """True if the file starts with the binary token database magic string."""
    try:
        fd.seek(0)
        magic = fd.read(len(BINARY_FORMAT.magic))
        fd.seek(0)
        return BINARY_FORMAT.magic == magic
    except IOError:
        return False


def _check_that_file_is_csv_database(path: Path) -> None:
    """Raises an error unless the path appears to be a CSV token database."""
    try:
        with path.open('rb') as fd:
            data = fd.read(8)  # Read 8 bytes, which should be the first token.

        if not data:
            return  # File is empty, which is valid CSV.

        if len(data) != 8:
            raise DatabaseFormatError(
                f'Attempted to read {path} as a CSV token database, but the '
                f'file is too short ({len(data)} B)')

        # Make sure the first 8 chars are a valid hexadecimal number.
        _ = int(data.decode(), 16)
    except (IOError, UnicodeDecodeError, ValueError) as err:
        raise DatabaseFormatError(
            f'Encountered error while reading {path} as a CSV token database'
        ) from err


def parse_binary(fd: BinaryIO) -> Iterable[TokenizedStringEntry]:
    """Parses TokenizedStringEntries from a binary token database file."""
    magic, entry_count = BINARY_FORMAT.header.unpack(
        fd.read(BINARY_FORMAT.header.size))

    if magic != BINARY_FORMAT.magic:
        raise DatabaseFormatError(
            f'Binary token database magic number mismatch (found {magic!r}, '
            f'expected {BINARY_FORMAT.magic!r}) while reading from {fd}')

    entries = []

    for _ in range(entry_count):
        token, day, month, year = BINARY_FORMAT.entry.unpack(
            fd.read(BINARY_FORMAT.entry.size))

        try:
            date_removed: Optional[datetime] = datetime(year, month, day)
        except ValueError:
            date_removed = None

        entries.append((token, date_removed))

    # Read the entire string table and define a function for looking up strings.
    string_table = fd.read()

    def read_string(start):
        end = string_table.find(b'\0', start)
        return string_table[start:string_table.find(b'\0', start)].decode(
        ), end + 1

    offset = 0
    for token, removed in entries:
        string, offset = read_string(offset)
        yield TokenizedStringEntry(token, string, DEFAULT_DOMAIN, removed)


def write_binary(database: Database, fd: BinaryIO) -> None:
    """Writes the database as packed binary to the provided binary file."""
    entries = sorted(database.entries())

    fd.write(BINARY_FORMAT.header.pack(BINARY_FORMAT.magic, len(entries)))

    string_table = bytearray()

    for entry in entries:
        if entry.date_removed:
            removed_day = entry.date_removed.day
            removed_month = entry.date_removed.month
            removed_year = entry.date_removed.year
        else:
            # If there is no removal date, use the special value 0xffffffff for
            # the day/month/year. That ensures that still-present tokens appear
            # as the newest tokens when sorted by removal date.
            removed_day = 0xff
            removed_month = 0xff
            removed_year = 0xffff

        string_table += entry.string.encode()
        string_table.append(0)

        fd.write(
            BINARY_FORMAT.entry.pack(entry.token, removed_day, removed_month,
                                     removed_year))

    fd.write(string_table)


class DatabaseFile(Database):
    """A token database that is associated with a particular file.

    This class adds the write_to_file() method that writes to file from which it
    was created in the correct format (CSV or binary).
    """
    def __init__(self, path: Path,
                 entries: Iterable[TokenizedStringEntry]) -> None:
        super().__init__(entries)
        self.path = path

    @staticmethod
    def create(path: Path) -> 'DatabaseFile':
        """Creates a DatabaseFile that coincides to the file type."""
        if path.is_dir():
            return _DirectoryDatabase(path)

        # Read the path as a packed binary file.
        with path.open('rb') as fd:
            if file_is_binary_database(fd):
                return _BinaryDatabase(path, fd)

        # Read the path as a CSV file.
        _check_that_file_is_csv_database(path)
        with path.open('r', newline='', encoding='utf-8') as csv_fd:
            return _CSVDatabase(path, csv_fd)

    @abstractmethod
    def write_to_file(self) -> None:
        """Exports in the original format to the original path."""

    @abstractmethod
    def add_and_discard_temporary(self,
                                  entries: Iterable[TokenizedStringEntry],
                                  commit: str) -> None:
        """Discards and adds entries to export in the original format.

        Adds entries after removing temporary entries from the Database
        to exclusively write re-occurring entries into memory and disk.
        """


class _BinaryDatabase(DatabaseFile):
    def __init__(self, path: Path, fd: BinaryIO) -> None:
        super().__init__(path, parse_binary(fd))

    def write_to_file(self) -> None:
        """Exports in the binary format to the original path."""
        with self.path.open('wb') as fd:
            write_binary(self, fd)

    def add_and_discard_temporary(self,
                                  entries: Iterable[TokenizedStringEntry],
                                  commit: str) -> None:
        # TODO(b/241471465): Implement adding new tokens and removing
        # temporary entries for binary databases.
        raise NotImplementedError


class _CSVDatabase(DatabaseFile):
    def __init__(self, path: Path, fd: TextIO) -> None:
        super().__init__(path, parse_csv(fd))

    def write_to_file(self) -> None:
        """Exports in the CSV format to the original path."""
        with self.path.open('wb') as fd:
            write_csv(self, fd)

    def add_and_discard_temporary(self,
                                  entries: Iterable[TokenizedStringEntry],
                                  commit: str) -> None:
        # TODO(b/241471465): Implement adding new tokens and removing
        # temporary entries for CSV databases.
        raise NotImplementedError


def _parse_directory(paths: Iterable[Path]) -> Iterable[TokenizedStringEntry]:
    """Parses TokenizedStringEntries from files in the directory as a CSV."""
    for path in paths:
        if path.suffix == '.csv':
            with path.open() as fd:
                yield from parse_csv(fd)


def _git_stdout(commands: List, cwd: Path) -> str:
    """Runs the git commands in the provided cwd and captures the output."""
    return subprocess.run(['git'] + commands,
                          capture_output=True,
                          check=True,
                          cwd=cwd,
                          text=True).stdout.strip()


def _new_entries(
    entries: Iterable[TokenizedStringEntry],
    database: Dict[_EntryKey,
                   TokenizedStringEntry]) -> List[TokenizedStringEntry]:
    """Retrieves the entries not in the database."""
    return [entry for entry in entries if entry.key() not in database]


class _DirectoryDatabase(DatabaseFile):
    def __init__(self, directory: Path) -> None:
        super().__init__(directory, _parse_directory(directory.iterdir()))

    def write_to_file(self) -> None:
        """Exports the database in CSV format to the original path."""
        database = Database(_parse_directory(self.path.iterdir()))._database  # pylint: disable=protected-access
        new_entries = _new_entries(self.entries(), database)
        if new_entries:
            with self._create_filename().open('wb') as fd:
                write_csv(Database(new_entries), fd)

    def _path_to_repo(self) -> Path:
        """Creates a path to the repository the database is within."""
        return Path(_git_stdout(['rev-parse', '--show-toplevel'],
                                self.path)).resolve()

    def _git_paths(self, commands: List) -> List[Path]:
        """Creates a list of complete paths to the repo."""
        try:
            changes = _git_stdout(commands, self.path).splitlines()
            files = [self.path / path for path in changes]
            return [path for path in files if path.suffix == '.csv']
        except subprocess.CalledProcessError:
            return []

    def _find_latest_csv(self, commit: str) -> Path:
        """Finds or creates a CSV to utilize for new entries.

        - Searches for untracked files in the repo to re-use.
        - When no untracked files exists, a difference between HEAD~
          and HEAD searches for a filename in the commit to re-use.
        - The filename is reused if HEAD~ is not merged with the provided
          commit.
        - When nothing is yielded from the previous searches or no git repo
          exists, then a new filename is created.
        """

        # Find untracked_changes in directory.
        untracked_changes = self._git_paths(
            ['ls-files', '--others', '--exclude-standard'])

        if untracked_changes:
            # TODO(b/243040287): Handle multiple paths in the commit.
            assert len(untracked_changes) < 2
            return untracked_changes.pop()
        # Find differences between HEAD~ and HEAD commit in directory.
        tracked_changes_from_latest_commit = self._git_paths(
            ['diff', '--name-only', '--diff-filter=A', '--relative', 'HEAD~'])

        if tracked_changes_from_latest_commit:
            # If changes exists in the comparison between HEAD~ and HEAD
            # and the filename is unmerged with the provided commit, the
            # retrieved filename will be reused. Otherwise, create a CSV.
            # Find differences between commit and HEAD~ in directory.
            returncode = subprocess.run(
                ['git', 'merge-base', '--is-ancestor', 'HEAD~', commit],
                cwd=self.path,
                stderr=subprocess.DEVNULL).returncode
            # A zero return code indicates that HEAD~ is in the commit,
            # while a non-zero return code indicates that HEAD~ is not
            # in commit.
            if returncode != 0:
                return tracked_changes_from_latest_commit.pop()

        return self._create_filename()

    def _create_filename(self) -> Path:
        """Generates a unique filename not in the directory."""
        # Tracked and untracked files do not exist in the repo.
        path = self.path / f'{uuid4().hex}.csv'
        while path.exists():
            path = self.path / f'{uuid4().hex}.csv'
        return path

    def add_and_discard_temporary(self,
                                  entries: Iterable[TokenizedStringEntry],
                                  commit: str) -> None:
        """Adds and updates entries to export in the CSV format.

        - New entries are collected from the incoming entries.
        - When no CSV exists, the new entries are loaded into a database and
          written to disk.
        - When a CSV exists and incoming entries do not exist within the CSV
          database, the temporary entries are removed from the directory and
          CSV database.
        - New entries are added to the CSV database and re-written to disk.
        """
        db = Database(entries)._database  # pylint: disable=protected-access
        new_entries = _new_entries(db.values(), self._database)
        csv_path = self._find_latest_csv(commit)
        if csv_path.exists():
            # Loading the CSV as a DatabaseFile.
            csv_db = DatabaseFile.create(csv_path)
            # Collects the keys to delete from the CSV database.
            keys_to_delete = (e.key()
                              for e in _new_entries(csv_db.entries(), db))
            for key in keys_to_delete:
                del self._database[key]
                del csv_db._database[key]  # pylint: disable=protected-access
            csv_db.add(new_entries)
            csv_db.write_to_file()
        elif new_entries:
            with csv_path.open('wb') as fd:
                write_csv(Database(new_entries), fd)

        self.add(new_entries)
