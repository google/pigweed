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
import re
import subprocess

from pathlib import PurePosixPath
from typing import (
    Any,
    Callable,
    Iterable,
)

BazelValue = bool | int | str | list[str] | dict[str, str]

LABEL_PAT = re.compile(
    r'^(?:(?:@(?P<repo>[^/]*))?//(?P<package>[^:]*))?' r'(?::(?P<name>.+))?$'
)


class ParseError(Exception):
    """Raised when a Bazel query returns data that can't be parsed."""


class BazelLabel:
    """Represents a Bazel target identifier."""

    def __init__(
        self,
        repo_name: str,
        package: str,
        name: str,
    ) -> None:
        self._repo_name = repo_name
        self._package = package
        self._name = name

    def __str__(self) -> str:
        """Canonical representation of a Bazel label."""
        return f'@{self._repo_name}//{self._package}:{self._name}'

    def repo_name(self) -> str:
        """Returns the repository identifier associated with this label."""
        return self._repo_name

    def package(self) -> str:
        """Returns the package path associated with this label."""
        return self._package

    def name(self) -> str:
        """Returns the target name associated with this label."""
        return self._name

    @classmethod
    def from_string(
        cls,
        label_str: str,
        repo_name: str | None = None,
        package: str | None = None,
    ) -> 'BazelLabel':
        """Creates a Bazel label.

        This method will attempt to parse the repo, package, and target portion
        of the given label string. If the repo portion of the label is omitted,
        it will use the given repo, if provided. If the repo and the package
        portions of the label are omitted, it will use the given package, if
        provided. If the target portion is omitted, the last segment of the
        package will be used.

        Note: This script currently does not accept labels with canonical repo
        names, i.e. labels starting with '@@' such as '@@foo//bar:baz.

        Args:
            label_str: Bazel label string, like "@repo//pkg:target".
            repo: Repo to use if omitted from label, e.g. as in "//pkg:target".
            package: Package to use if omitted from label, e.g. as in ":target".
        """
        match = re.match(LABEL_PAT, label_str)
        if not label_str or not match:
            raise ParseError(f'invalid label: "{label_str}"')
        if match.group(1) or not package:
            package = ''
        if match.group(1):
            repo_name = match.group(1)
        elif not repo_name:
            raise ParseError(f'unable to determine repo name: "{label_str}"')
        package = match.group(2) if match.group(2) else package
        if match.group(3):
            name = match.group(3)
        else:
            name = PurePosixPath(package).name
        return cls(repo_name, package, name)


def parse_invalid(attr: dict[str, Any]) -> BazelValue:
    """Raises an error that a type is unrecognized."""
    attr_type = attr['type']
    raise ParseError(f'unknown type: {attr_type}, expected one of {BazelValue}')


class BazelRule:
    """Represents a Bazel rule as parsed from the query results."""

    def __init__(self, kind: str, label: BazelLabel) -> None:
        """Create a Bazel rule.

        Args:
            kind: The type of Bazel rule, e.g. cc_library.
            label: A Bazel label corresponding to this rule.
        """
        self._kind = kind
        self._label = label
        self._attrs: dict[str, BazelValue] = {}
        self._types: dict[str, str] = {}

    def kind(self) -> str:
        """Returns this rule's target type."""
        return self._kind

    def label(self) -> BazelLabel:
        """Returns this rule's Bazel label."""
        return self._label

    def parse_attrs(self, attrs: Iterable[dict[str, Any]]) -> None:
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
            'label': lambda attr: attr.get('stringValue', ''),
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
            self._types[attr_name] = attr_type

    def has_attr(self, attr_name: str) -> bool:
        """Returns whether the rule has an attribute of the given name.

        Args:
            attr_name: The name of the attribute.
        """
        return attr_name in self._attrs

    def attr_type(self, attr_name: str) -> str:
        """Returns the type of an attribute according to Bazel.

        Args:
            attr_name: The name of the attribute.
        """
        return self._types[attr_name]

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

    def filter_attr(self, attr_name: str, remove: Iterable[str]) -> None:
        """Removes values from a list attribute.

        Args:
            attr_name: The name of the attribute.
            remove: The values to remove.
        """
        values = self.get_list(attr_name)
        self._attrs[attr_name] = [v for v in values if v not in remove]


class BazelRepo:
    """Represents a local instance of a Bazel repository.

    Attributes:
        generate: Indicates whether GN should be automatically generated from
                  Bazel rules or not.
        targets:  A list of the Bazel targets to parse, along with their
                  dependencies.
        options:  Command-line options to use when invoking `bazel cquery`.
    """

    def __init__(self, repo_name: str) -> None:
        """Creates an object representing a Bazel workspace at the given path.

        Args:
            repo_name: The Bazel repository name, like "pigweed".
        """
        self.generate = False
        self.targets: list[str] = []
        self.options: dict[str, Any] = {}
        self._fetched = False
        self._repo_name: str = repo_name
        self._rules: dict[str, BazelRule] = {}

    def get_rules(self, labels: list[BazelLabel]) -> Iterable[BazelRule]:
        """Returns a rule matching the given label."""
        if not self.generate:
            return
        needed: list[str] = []
        for label in labels:
            label_str = str(label)
            rule = self._rules.get(label_str, None)
            if rule:
                yield rule
                continue
            needed.append(label_str)
        flags = [f'--{label}={value}' for label, value in self.options.items()]
        results = list(self._cquery('+'.join(needed), flags))
        for result in results:
            rule_data = result['target']['rule']
            yield self._make_rule(rule_data)

    def revision(self) -> str:
        """Returns the git revision of the repo.

        TODO: https://pwbug.dev/398015969 - Return HTTP archives directly.
        """
        result = self._bazel('mod', 'show_repo', self._repo_name)

        # This is a bit brittle, but works for all the current deps.
        m1 = re.search(r'urls = \["([^"]+)"\]', result)
        if not m1:
            raise ParseError(
                f'Failed to find URL for "{self._repo_name}" in:\n', result
            )
        url = m1.group(1)
        m2 = re.match(
            r'(https?://github.com/[^/]+/[^/]+)/releases/download/([^/]+)', url
        )
        if not m2:
            raise ParseError(f'Unsupported URL: {url}')
        tag = m2.group(2)

        result = self._git('ls-remote', m2.group(1), '-t', tag)
        if not result:
            raise ParseError(f'Tag not found for {self._repo_name}: {tag}')
        return result.split()[0]

    def _cquery(self, expr: str, flags: list[str]) -> Iterable[Any]:
        """Invokes `bazel cquery` with the given selector."""
        result = self._bazel('cquery', expr, *flags, '--output=jsonproto')
        return json.loads(result)['results']

    def _query(self, expr: str) -> Iterable[Any]:
        """Invokes `bazel query` with the given selector."""
        results = self._bazel('query', expr, '--output=streamed_jsonproto')
        return [json.loads(result) for result in results.split('\n')]

    def _make_rule(self, rule_data: Any) -> BazelRule:
        """Make a BazelRule from JSON data returned by query or cquery."""
        label_str = rule_data['name']
        label = BazelLabel.from_string(label_str)
        rule = BazelRule(rule_data['ruleClass'], label)
        rule.parse_attrs(rule_data['attribute'])
        self._rules[label_str] = rule
        return rule

    def _bazel(self, *args: str) -> str:
        """Execute a Bazel command in the workspace."""
        return self._exec('bazelisk', *args, '--noshow_progress', check=False)

    def _git(self, *args: str) -> str:
        """Execute a git command in the workspace."""
        return self._exec('git', *args, check=True)

    def _exec(self, *args: str, check=True) -> str:
        """Execute a command in the workspace."""
        try:
            result = subprocess.run(
                list(args),
                check=check,
                capture_output=True,
            )
            if result.stdout:
                # The extra 'str()' facilitates MagicMock.
                return str(result.stdout.decode('utf-8')).strip()
            if check:
                return ''
            errmsg = result.stderr.decode('utf-8')
        except subprocess.CalledProcessError as error:
            errmsg = error.stderr.decode('utf-8')
        cmdline = ' '.join(list(args))
        raise ParseError(
            f'{self._repo_name} failed to exec `{cmdline}`: {errmsg}'
        )
