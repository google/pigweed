# Copyright 2024 The Pigweed Authors
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
"""Bazel Tests."""

import pathlib
import unittest
import xml.etree.ElementTree

from pw_build_mcuxpresso.bazel import (
    BazelVariable,
    generate_bazel_targets,
    headers_cc_library,
    import_targets,
    target_to_bazel,
)
from pw_build_mcuxpresso.common import BuildTarget
from pw_build_mcuxpresso.components import Project

# pylint: disable=missing-function-docstring
# pylint: disable=line-too-long


def setup_test_project() -> Project:
    test_manifest_xml = '''
    <manifest>
      <components>
        <component id="test">
          <dependencies>
            <component_dependency value="foo"/>
            <component_dependency value="bar"/>
          </dependencies>
        </component>
        <component id="foo" package_base_path="foo">
          <defines>
            <define name="FOO"/>
          </defines>
          <source relative_path="include" type="c_include">
            <files mask="foo.h"/>
          </source>
          <source relative_path="src" type="src">
            <files mask="foo.cc"/>
          </source>
          <include_paths>
            <include_path relative_path="include" type="c_include"/>
          </include_paths>
        </component>
        <component id="bar" package_base_path="bar">
          <defines>
            <define name="BAR"/>
          </defines>
          <source relative_path="include" type="c_include">
            <files mask="bar.h"/>
          </source>
          <source relative_path="src" type="src">
            <files mask="bar.cc"/>
          </source>
          <include_paths>
            <include_path relative_path="include" type="c_include"/>
          </include_paths>
        </component>
        <component id="frodo" package_base_path="frodo">
          <dependencies>
            <component_dependency value="bilbo"/>
            <component_dependency value="smeagol"/>
          </dependencies>
          <defines>
            <define name="FRODO"/>
          </defines>
          <source relative_path="include" type="c_include">
            <files mask="frodo.h"/>
          </source>
          <source relative_path="src" type="src">
            <files mask="frodo.cc"/>
          </source>
          <source toolchain="armgcc" relative_path="./" type="lib">
            <files mask="libonering.a"/>
          </source>
          <include_paths>
            <include_path relative_path="include" type="c_include"/>
          </include_paths>
        </component>
        <component id="bilbo" package_base_path="bilbo">
          <defines>
            <define name="BILBO"/>
          </defines>
          <source relative_path="include" type="c_include">
            <files mask="bilbo.h"/>
          </source>
          <source relative_path="src" type="src">
            <files mask="bilbo.cc"/>
          </source>
          <include_paths>
            <include_path relative_path="include" type="c_include"/>
          </include_paths>
        </component>
        <component id="smeagol" package_base_path="smeagol">
          <dependencies>
            <component_dependency value="gollum"/>
          </dependencies>
          <source toolchain="armgcc" relative_path="./" type="lib">
            <files mask="libonering.a"/>
          </source>
        </component>
        <component id="gollum" package_base_path="gollum">
          <dependencies>
            <component_dependency value="smeagol"/>
          </dependencies>
          <source toolchain="armgcc" relative_path="./" type="lib">
            <files mask="libonering.a"/>
          </source>
        </component>
      </components>
    </manifest>
    '''
    manifest = (
        xml.etree.ElementTree.fromstring(test_manifest_xml),
        pathlib.Path.cwd() / "manifest.xml",
    )
    return Project([manifest], pathlib.Path.cwd(), ["test", "frodo"])


class TargetToBazelTest(unittest.TestCase):
    """target_to_bazel tests."""

    def test_str_attrib(self):
        expected_format = r'''
test_rule(
    name = "foo",
    bar = "baz",
)
'''.strip()

        target = BuildTarget("test_rule", "foo", {"bar": "baz"})

        self.assertEqual(target_to_bazel(target), expected_format)

    def test_num_attr(self):
        expected_format = r'''
test_rule(
    name = "foo",
    bar = 13,
)
'''.strip()

        target = BuildTarget(
            "test_rule",
            "foo",
            {
                "bar": 13,
            },
        )

        self.assertEqual(target_to_bazel(target), expected_format)

    def test_bool_attr(self):
        expected_format = r'''
test_rule(
    name = "foo",
    bar = True,
    baz = False,
)
'''.strip()

        target = BuildTarget(
            "test_rule",
            "foo",
            {
                "bar": True,
                "baz": False,
            },
        )

        self.assertEqual(target_to_bazel(target), expected_format)

    def test_target_attr(self):
        expected_format = r'''
test_rule(
    name = "foo",
    bar = ":baz",
)
'''.strip()

        target = BuildTarget(
            "test_rule",
            "foo",
            {"bar": BuildTarget("test_rule", "baz")},
        )

        self.assertEqual(target_to_bazel(target), expected_format)

    def test_bazel_var_attr(self):
        expected_format = r'''
test_rule(
    name = "foo",
    bar = MY_VAR,
)
'''.strip()

        target = BuildTarget(
            "test_rule",
            "foo",
            {"bar": BazelVariable("MY_VAR", "")},
        )

        self.assertEqual(target_to_bazel(target), expected_format)

    def test_list_attr(self):
        expected_format = r'''
test_rule(
    name = "foo",
    bar = ['baz', 13, True, ':quox'],
)
'''.strip()

        target = BuildTarget(
            "test_rule",
            "foo",
            {
                "bar": [
                    "baz",
                    13,
                    True,
                    BuildTarget("test_rule", "quox"),
                ],
            },
        )

        self.assertEqual(target_to_bazel(target), expected_format)

    def test_full_rule(self):
        expected_format = r'''
lore(
    name = "gollum",
    age = 600,
    is_hobbit = True,
    needs = [':fish', ':ring'],
    other_name = "smeagol",
)
'''.strip()

        target = BuildTarget(
            "lore",
            "gollum",
            {
                "other_name": "smeagol",
                "age": 600,
                "is_hobbit": True,
                "needs": [
                    BuildTarget("test_rule", "fish"),
                    BuildTarget("test_rule", "ring"),
                ],
            },
        )

        self.assertEqual(target_to_bazel(target), expected_format)

    def test_indent(self):
        expected_format = r'''
test_rule(
  name = "foo",
  indent = 2,
)
'''.strip()

        target = BuildTarget("test_rule", "foo", {"indent": 2})

        self.assertEqual(target_to_bazel(target, 2), expected_format)


class ImportTargetsTest(unittest.TestCase):
    """import_targets tests."""

    def test_single_lib(self):
        library = [pathlib.Path("testlib.a")]

        targets = import_targets(library)

        self.assertEqual(len(targets), 1)
        self.assertListEqual(
            targets,
            [
                BuildTarget(
                    "cc_import",
                    "testlib.a",
                    {
                        "static_library": "testlib.a",
                    },
                )
            ],
        )

    def test_nested_lib(self):
        library = [pathlib.Path("some/path/to/testlib.a")]

        targets = import_targets(library)

        self.assertEqual(len(targets), 1)
        self.assertListEqual(
            targets,
            [
                BuildTarget(
                    "cc_import",
                    "some.path.to.testlib.a",
                    {
                        "static_library": "some/path/to/testlib.a",
                    },
                )
            ],
        )

    def test_multi_libs(self):
        libraries = [
            pathlib.Path("some/path/to/testlib.a"),
            pathlib.Path("testlib.a"),
            pathlib.Path("libonering.a"),
        ]

        targets = import_targets(libraries)

        self.assertEqual(len(targets), len(libraries))
        self.assertListEqual(
            targets,
            [
                BuildTarget(
                    "cc_import",
                    "libonering.a",
                    {
                        "static_library": "libonering.a",
                    },
                ),
                BuildTarget(
                    "cc_import",
                    "some.path.to.testlib.a",
                    {
                        "static_library": "some/path/to/testlib.a",
                    },
                ),
                BuildTarget(
                    "cc_import",
                    "testlib.a",
                    {
                        "static_library": "testlib.a",
                    },
                ),
            ],
        )


class HeadersCommonTest(unittest.TestCase):
    """headers_cc_library tests."""

    def test_common_component(self):
        expected_format = '''
cc_library(
    name = "commons",
    copts = COPTS,
    defines = ['BAR', 'BILBO', 'FOO', 'FRODO'],
    deps = [':user_config'],
    hdrs = ['bar/include/bar.h', 'bilbo/include/bilbo.h', 'foo/include/foo.h', 'frodo/include/frodo.h'],
    includes = ['bar/include', 'bilbo/include', 'foo/include', 'frodo/include'],
)
'''.strip()

        project = setup_test_project()

        commons = headers_cc_library(project)

        self.assertIsNotNone(commons.attrs.get("defines"))
        self.assertListEqual(
            commons.attrs["defines"], ["BAR", "BILBO", "FOO", "FRODO"]
        )

        self.assertIsNotNone(commons.attrs.get("hdrs"))
        self.assertListEqual(
            commons.attrs["hdrs"],
            [
                'bar/include/bar.h',
                'bilbo/include/bilbo.h',
                'foo/include/foo.h',
                'frodo/include/frodo.h',
            ],
        )

        self.assertIsNotNone(commons.attrs.get("includes"))
        self.assertListEqual(
            commons.attrs["includes"],
            ['bar/include', 'bilbo/include', 'foo/include', 'frodo/include'],
        )

        self.assertEqual(target_to_bazel(commons), expected_format)


class ProjectTargetsTest(unittest.TestCase):
    """generate_project_targets tests."""

    def test_project(self):
        expected_format = '''
label_flag(
    name = "user_config",
    build_setting_default = "@pigweed//pw_build:empty_cc_library",
)
cc_library(
    name = "commons",
    copts = COPTS,
    defines = ['BAR', 'BILBO', 'FOO', 'FRODO'],
    deps = [':user_config'],
    hdrs = ['bar/include/bar.h', 'bilbo/include/bilbo.h', 'foo/include/foo.h', 'frodo/include/frodo.h'],
    includes = ['bar/include', 'bilbo/include', 'foo/include', 'frodo/include'],
)
cc_import(
    name = "frodo.libonering.a",
    static_library = "frodo/libonering.a",
)
cc_import(
    name = "gollum.libonering.a",
    static_library = "gollum/libonering.a",
)
cc_import(
    name = "smeagol.libonering.a",
    static_library = "smeagol/libonering.a",
)
cc_library(
    name = "bar",
    copts = COPTS,
    deps = [':commons'],
    srcs = ['bar/src/bar.cc'],
)
cc_library(
    name = "bilbo",
    copts = COPTS,
    deps = [':commons'],
    srcs = ['bilbo/src/bilbo.cc'],
)
cc_library(
    name = "foo",
    copts = COPTS,
    deps = [':commons'],
    srcs = ['foo/src/foo.cc'],
)
cc_library(
    name = "frodo",
    copts = COPTS,
    deps = [':bilbo', ':commons', ':frodo.libonering.a', ':smeagol'],
    srcs = ['frodo/src/frodo.cc'],
)
cc_library(
    name = "gollum",
    copts = COPTS,
    deps = [':commons', ':gollum.libonering.a', ':smeagol.libonering.a'],
)
cc_library(
    name = "smeagol",
    copts = COPTS,
    deps = [':commons', ':gollum.libonering.a', ':smeagol.libonering.a'],
)
cc_library(
    name = "test",
    copts = COPTS,
    deps = [':bar', ':commons', ':foo'],
)
'''.strip()

        project = setup_test_project()

        targets = list(generate_bazel_targets(project))
        self.assertEqual(len(targets), 12)

        targets = "\n".join(target_to_bazel(target) for target in targets)
        self.assertEqual(targets, expected_format)


if __name__ == '__main__':
    unittest.main()
