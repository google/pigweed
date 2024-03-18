# Copyright 2023 The Pigweed Authors
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
"""Parses Bazel rules from a local Bazel workspace."""

import json
import logging
import os
import subprocess

from io import StringIO
from pathlib import Path, PurePosixPath
from typing import (
    Any,
    Callable,
    IO,
    Iterable,
    Iterator,
    Optional,
    Union,
)
from xml.etree import ElementTree

_LOG = logging.getLogger(__name__)


BazelValue = Union[bool, int, str, list[str], dict[str, str]]


class ParseError(Exception):
    """Raised when a Bazel query returns data that can't be parsed."""


def parse_invalid(attr: dict[str, Any]) -> BazelValue:
    """Raises an error that a type is unrecognized."""
    attr_type = attr['type']
    raise ParseError(f'unknown type: {attr_type}, expected one of {BazelValue}')


class BazelRule:
    """Represents a Bazel rule as parsed from the query results."""

    def __init__(self, label: str, kind: str) -> None:
        """Create a Bazel rule.

        Args:
            label: An absolute Bazel label corresponding to this rule.
            kind: The type of Bazel rule, e.g. cc_library.
        """
        if not label.startswith('//'):
            raise ParseError(f'invalid label: {label}')
        if ':' in label:
            parts = label.split(':')
            if len(parts) != 2:
                raise ParseError(f'invalid label: {label}')
            self._package = parts[0][2:]
            self._target = parts[1]
        else:
            self._package = str(label)[2:]
            self._target = PurePosixPath(label).name
        self._kind = kind

        self._attrs: dict[str, BazelValue] = {}

    def package(self) -> str:
        """Returns the package portion of this rule's name."""
        return self._package

    def label(self) -> str:
        """Returns this rule's full target name."""
        return f'//{self._package}:{self._target}'

    def kind(self) -> str:
        """Returns this rule's target type."""
        return self._kind

    def parse(self, attrs: Iterable[dict[str, Any]]) -> None:
        """Maps JSON data from a bazel query into this object.

        Args:
            attrs: A dictionary of attribute names and values for the Bazel
                rule. These should match the output of
                `bazel cquery ... --output=jsonproto`.
        """
        attr_parsers: dict[str, Callable[[dict[str, Any]], BazelValue]] = {
            'boolean': lambda attr: attr.get('booleanValue', False),
            'integer': lambda attr: int(attr.get('intValue', '0')),
            'string': lambda attr: attr.get('stringValue', ''),
            'string_list': lambda attr: attr.get('stringListValue', []),
            'label_list': lambda attr: attr.get('stringListValue', []),
            'string_dict': lambda attr: {
                p['key']: p['value'] for p in attr.get('stringDictValue', [])
            },
        }
        for attr in attrs:
            if 'explicitlySpecified' not in attr:
                continue
            if not attr['explicitlySpecified']:
                continue
            try:
                attr_name = attr['name']
            except KeyError:
                raise ParseError(
                    f'missing "name" in {json.dumps(attr, indent=2)}'
                )
            try:
                attr_type = attr['type'].lower()
            except KeyError:
                raise ParseError(
                    f'missing "type" in {json.dumps(attr, indent=2)}'
                )

            attr_parser = attr_parsers.get(attr_type, parse_invalid)
            self._attrs[attr_name] = attr_parser(attr)

    def has_attr(self, attr_name: str) -> bool:
        """Returns whether the rule has an attribute of the given name.

        Args:
            attr_name: The name of the attribute.
        """
        return attr_name in self._attrs

    def get_bool(self, attr_name: str) -> bool:
        """Gets the value of a boolean attribute.

        Args:
            attr_name: The name of the boolean attribute.
        """
        val = self._attrs.get(attr_name, False)
        assert isinstance(val, bool)
        return val

    def get_int(self, attr_name: str) -> int:
        """Gets the value of an integer attribute.

        Args:
            attr_name: The name of the integer attribute.
        """
        val = self._attrs.get(attr_name, 0)
        assert isinstance(val, int)
        return val

    def get_str(self, attr_name: str) -> str:
        """Gets the value of a string attribute.

        Args:
            attr_name: The name of the string attribute.
        """
        val = self._attrs.get(attr_name, '')
        assert isinstance(val, str)
        return val

    def get_list(self, attr_name: str) -> list[str]:
        """Gets the value of a string list attribute.

        Args:
            attr_name: The name of the string list attribute.
        """
        val = self._attrs.get(attr_name, [])
        assert isinstance(val, list)
        return val

    def get_dict(self, attr_name: str) -> dict[str, str]:
        """Gets the value of a string list attribute.

        Args:
            attr_name: The name of the string list attribute.
        """
        val = self._attrs.get(attr_name, {})
        assert isinstance(val, dict)
        return val

    def set_attr(self, attr_name: str, value: BazelValue) -> None:
        """Sets the value of an attribute.

        Args:
            attr_name: The name of the attribute.
            value: The value to set.
        """
        self._attrs[attr_name] = value


class BazelWorkspace:
    """Represents a local instance of a Bazel repository.

    Attributes:
        root: Path to the local instance of a Bazel workspace.
        packages: Bazel packages mapped to their default visibility.
        repo: The name of Bazel workspace.
    """

    def __init__(self, pathname: Path) -> None:
        """Creates an object representing a Bazel workspace at the given path.

        Args:
            pathname: Path to the local instance of a Bazel workspace.
        """
        self.root: Path = pathname
        self.packages: dict[str, str] = {}
        self.repo: str = os.path.basename(pathname)

    def get_rules(self, kind: str) -> Iterator[BazelRule]:
        """Returns rules matching the given kind, e.g. 'cc_library'."""
        self._load_packages()
        results = self._query(
            'cquery', f'kind({kind}, //...)', '--output=jsonproto'
        )
        json_data = json.loads(results)
        for result in json_data.get('results', []):
            rule_data = result['target']['rule']
            target = rule_data['name']
            rule = BazelRule(target, kind)
            default_visibility = self.packages[rule.package()]
            rule.set_attr('visibility', [default_visibility])
            rule.parse(rule_data['attribute'])
            yield rule

    def _load_packages(self) -> None:
        """Scans the workspace for packages and their default visibilities."""
        if self.packages:
            return
        packages = self._query('query', '//...', '--output=package').split('\n')
        for package in packages:
            results = self._query(
                'query', f'buildfiles(//{package}:*)', '--output=xml'
            )
            xml_data = ElementTree.fromstring(results)
            for pkg_elem in xml_data:
                if not pkg_elem.attrib['name'].startswith(f'//{package}:'):
                    continue
                for elem in pkg_elem:
                    if elem.tag == 'visibility-label':
                        self.packages[package] = elem.attrib['name']

    def _query(self, *args: str) -> str:
        """Invokes `bazel cquery` with the given selector."""
        output = StringIO()
        self._exec(*args, '--noshow_progress', output=output)
        return output.getvalue()

    def _exec(self, *args: str, output: Optional[IO] = None) -> None:
        """Execute a Bazel command in the workspace."""
        cmdline = ['bazel'] + list(args) + ['--noshow_progress']
        result = subprocess.run(
            cmdline,
            cwd=self.root,
            capture_output=True,
        )
        if not result.stdout:
            _LOG.error(result.stderr.decode('utf-8'))
            raise ParseError(f'Failed to query Bazel workspace: {self.root}')
        if output:
            output.write(result.stdout.decode('utf-8').strip())

    def run(self, label: str, *args, output: Optional[IO] = None) -> None:
        """Invokes `bazel run` on the given label.

        Args:
            label: A Bazel target, e.g. "@repo//package:target".
            args: Additional options to pass to `bazel`.
            output: Optional destination for the output of the command.
        """
        self._exec('run', label, *args, output=output)

    def revision(self) -> str:
        try:
            result = subprocess.run(
                ['git', 'rev-parse', 'HEAD'],
                cwd=self.root,
                check=True,
                capture_output=True,
            )
        except subprocess.CalledProcessError as error:
            print(error.stderr.decode('utf-8'))
            raise
        return result.stdout.decode('utf-8').strip()

    def url(self) -> str:
        try:
            result = subprocess.run(
                ['git', 'remote', 'get-url', 'origin'],
                cwd=self.root,
                check=True,
                capture_output=True,
            )
        except subprocess.CalledProcessError as error:
            print(error.stderr.decode('utf-8'))
            raise
        return result.stdout.decode('utf-8').strip()
