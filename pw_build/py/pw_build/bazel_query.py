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

from pathlib import PurePath, PurePosixPath
from typing import (
    Any,
    Callable,
    Iterable,
)

BazelValue = bool | int | str | list[str] | dict[str, str]

LABEL_PAT = re.compile(r'^(?:(?:@([a-zA-Z0-9_]*))?//([^:]*))?(?::([^:]+))?$')


class ParseError(Exception):
    """Raised when a Bazel query returns data that can't be parsed."""


class BazelLabel:
    """Represents a Bazel target identifier."""

    def __init__(
        self,
        label_str: str,
        repo: str | None = None,
        package: str | None = None,
    ) -> None:
        """Creates a Bazel label.

        This method will attempt to parse the repo, package, and target portion
        of the given label string. If the repo portion of the label is omitted,
        it will use the given repo, if provided. If the repo and the package
        portions of the label are omitted, it will use the given package, if
        provided. If the target portion is omitted, the last segment of the
        package will be used.

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
            self._repo = match.group(1)
        else:
            assert repo
            self._repo = repo
        self._package = match.group(2) if match.group(2) else package
        self._target = (
            match.group(3)
            if match.group(3)
            else PurePosixPath(self._package).name
        )

    def __str__(self) -> str:
        """Canonical representation of a Bazel label."""
        return f'@{self._repo}//{self._package}:{self._target}'

    def repo(self) -> str:
        """Returns the repository identifier associated with this label."""
        return self._repo

    def package(self) -> str:
        """Returns the package path associated with this label."""
        return self._package

    def target(self) -> str:
        """Returns the target name associated with this label."""
        return self._target


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

    def generate(self) -> Iterable[str]:
        """Yields a sequence of strings describing the rule in Bazel."""
        yield f'{self._kind}('
        yield f'    name = "{self._label.target()}",'
        for name in sorted(self._attrs.keys()):
            if name == 'name':
                continue
            attr_type = self._types[name]
            if attr_type == 'boolean':
                yield f'    {name} = {self.get_bool(name)},'
            elif attr_type == 'integer':
                yield f'    {name} = {self.get_int(name)},'
            elif attr_type == 'string':
                yield f'    {name} = "{self.get_str(name)}",'
            elif attr_type == 'string_list' or attr_type == 'label_list':
                strs = self.get_list(name)
                if len(strs) == 1:
                    yield f'    {name} = ["{strs[0]}"],'
                elif len(strs) > 1:
                    yield f'    {name} = ['
                    for s in strs:
                        yield f'        "{s}",'
                    yield '    ],'
            elif attr_type == 'string_dict':
                str_dict = self.get_dict(name)
                yield f'    {name} = {{'
                for k, v in str_dict.items():
                    yield f'        {k} = "{v}",'
                yield '    },'
        yield ')'


class BazelWorkspace:
    """Represents a local instance of a Bazel repository.

    Attributes:
        defaults: Attributes automatically applied to every rule in the
                  workspace, which may be ignored when parsing.
        generate: Indicates whether GN should be automatically generated for
                  this workspace or not.
        targets:  A list of the Bazel targets to parse, along with their
                  dependencies.
    """

    def __init__(
        self, repo: str, source_dir: PurePath | None, fetch: bool = True
    ) -> None:
        """Creates an object representing a Bazel workspace at the given path.

        Args:
            repo: The Bazel repository name, like "com_google_pigweed".
            source_dir: Path to the local instance of a Bazel workspace.
        """
        self.defaults: dict[str, list[str]] = {}
        self.generate = True
        self.targets: list[str] = []
        self.options: dict[str, Any] = {}
        self._fetched = False
        self._repo: str = repo
        self._revisions: dict[str, str] = {}
        self._rules: dict[str, BazelRule] = {}
        self._source_dir = source_dir

        # Make sure the workspace has up-to-date objects and refs.
        if fetch:
            self._git('fetch')

    def repo(self) -> str:
        """Returns the Bazel repository name for this workspace."""
        return self._repo

    def get_http_archives(self) -> Iterable[BazelRule]:
        """Returns the http_archive rules from a workspace's WORKSPACE file."""
        if not self.generate:
            return
        for result in self._query('kind(http_archive, //external:*)'):
            if result['type'] != 'RULE':
                continue
            yield self._make_rule(result['rule'])

    def get_rules(self, labels: list[BazelLabel]) -> Iterable[BazelRule]:
        """Returns a rule matching the given label."""
        if not self.generate:
            return
        needed: list[str] = []
        for label in labels:
            print(f'Examining [{label}]                             ')
            short = f'//{label.package()}:{label.target()}'
            rule = self._rules.get(short, None)
            if rule:
                yield rule
                continue
            needed.append(short)
        flags = [f'--{label}={value}' for label, value in self.options.items()]
        results = list(self._cquery('+'.join(needed), flags))
        for result in results:
            rule_data = result['target']['rule']
            yield self._make_rule(rule_data)

    def revision(self, commitish: str = 'HEAD') -> str:
        """Returns the revision digest of the workspace's git commit-ish."""
        try:
            return self._git('rev-parse', commitish)
        except ParseError:
            pass
        tags = self._git('tag', '--list').split()
        tag = min([tag for tag in tags if commitish in tag], key=len)
        return self._git('rev-parse', tag)

    def timestamp(self, revision: str) -> str:
        """Returns the timestamp of the workspace's git commit-ish."""
        return self._git('show', '--no-patch', '--format=%ci', revision)

    def url(self) -> str:
        """Returns the git URL of the workspace."""
        return self._git('remote', 'get-url', 'origin')

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
        short = rule_data['name']
        label = BazelLabel(short, repo=self._repo)
        rule = BazelRule(rule_data['ruleClass'], label)
        rule.parse_attrs(rule_data['attribute'])
        for attr_name, values in self.defaults.items():
            rule.filter_attr(attr_name, values)
        self._rules[short] = rule
        return rule

    def _bazel(self, *args: str) -> str:
        """Execute a Bazel command in the workspace."""
        return self._exec('bazel', *args, '--noshow_progress', check=False)

    def _git(self, *args: str) -> str:
        """Execute a git command in the workspace."""
        return self._exec('git', *args, check=True)

    def _exec(self, *args: str, check=True) -> str:
        """Execute a command in the workspace."""
        try:
            result = subprocess.run(
                list(args),
                cwd=self._source_dir,
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
            f'{self._repo} failed to exec '
            + f'`cd {self._source_dir} && {cmdline}`: {errmsg}'
        )
