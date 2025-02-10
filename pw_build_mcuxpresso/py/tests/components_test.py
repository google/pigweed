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
"""Components Tests."""

import pathlib
import unittest
import xml.etree.ElementTree
from itertools import chain

from pw_build_mcuxpresso.components import Project

# pylint: disable=missing-function-docstring
# pylint: disable=line-too-long


class ParseDefinesTest(unittest.TestCase):
    """parse_defines tests."""

    def test_parse_defines(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <defines>
                <define name="TEST_WITH_VALUE" value="1"/>
                <define name="TEST_WITHOUT_VALUE"/>
              </defines>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        defines = project.components["test"].defines

        self.assertCountEqual(
            defines, ['TEST_WITH_VALUE=1', 'TEST_WITHOUT_VALUE']
        )

    def test_no_defines(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        defines = project.components["test"].defines

        self.assertCountEqual(defines, [])

    def test_device_cores(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <defines>
                <define name="TEST_WITH_CORE" device_cores="CORE0"/>
                <define name="TEST_WITHOUT_CORE" device_cores="CORE1"/>
                <define name="TEST_WITHOUT_CORES"/>
              </defines>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None, "CORE0")
        defines = project.components["test"].defines

        self.assertCountEqual(defines, ['TEST_WITH_CORE', 'TEST_WITHOUT_CORES'])


class ParseIncludePathsTest(unittest.TestCase):
    """parse_include_paths tests."""

    def test_parse_include_paths(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <include_paths>
                <include_path relative_path="example" type="c_include"/>
                <include_path relative_path="asm" type="asm_include"/>
              </include_paths>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        include_paths = project.components["test"].include_dirs

        self.assertCountEqual(
            include_paths, [pathlib.Path("example"), pathlib.Path("asm")]
        )

    def test_with_base_path(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test" package_base_path="src">
              <include_paths>
                <include_path relative_path="example" type="c_include"/>
                <include_path relative_path="asm" type="asm_include"/>
              </include_paths>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        include_paths = project.components["test"].include_dirs

        self.assertCountEqual(
            include_paths,
            [pathlib.Path('src/example'), pathlib.Path('src/asm')],
        )

    def test_unknown_type(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test" package_base_path="src">
              <include_paths>
                <include_path relative_path="rust" type="rust_include"/>
              </include_paths>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        include_paths = project.components["test"].include_dirs

        self.assertCountEqual(include_paths, [])

    def test_no_include_paths(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        include_paths = project.components["test"].include_dirs

        self.assertCountEqual(include_paths, [])

    def test_device_cores(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <include_paths>
                <include_path relative_path="with_core" type="c_include"
                              device_cores="CORE0"/>
                <include_path relative_path="without_core" type="c_include"
                              device_cores="CORE1"/>
                <include_path relative_path="without_cores" type="c_include"/>
              </include_paths>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None, "CORE0")
        include_paths = project.components["test"].include_dirs

        self.assertCountEqual(
            include_paths,
            [pathlib.Path('with_core'), pathlib.Path('without_cores')],
        )


class ParseHeadersTest(unittest.TestCase):
    """parse_headers tests."""

    def test_parse_headers(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <source relative_path="include" type="c_include">
                <files mask="test.h"/>
                <files mask="test_types.h"/>
              </source>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        headers = project.components["test"].headers

        self.assertCountEqual(
            headers,
            [
                pathlib.Path('include/test.h'),
                pathlib.Path('include/test_types.h'),
            ],
        )

    def test_with_base_path(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test" package_base_path="src">
              <source relative_path="include" type="c_include">
                <files mask="test.h"/>
                <files mask="test_types.h"/>
              </source>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        headers = project.components["test"].headers

        self.assertCountEqual(
            headers,
            [
                pathlib.Path('src/include/test.h'),
                pathlib.Path('src/include/test_types.h'),
            ],
        )

    def test_multiple_sets(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <source relative_path="include" type="c_include">
                <files mask="test.h"/>
              </source>
              <source relative_path="internal" type="c_include">
                <files mask="test_types.h"/>
              </source>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        headers = project.components["test"].headers

        self.assertCountEqual(
            headers,
            [
                pathlib.Path('include/test.h'),
                pathlib.Path('internal/test_types.h'),
            ],
        )

    def test_no_headers(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        headers = project.components["test"].headers

        self.assertCountEqual(headers, [])

    def test_device_cores(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <source relative_path="with_core" type="c_include"
                      device_cores="CORE0">
                <files mask="test.h"/>
              </source>
              <source relative_path="without_core" type="c_include"
                      device_cores="CORE1">
                <files mask="test.h"/>
              </source>
              <source relative_path="without_cores" type="c_include">
                <files mask="test.h"/>
              </source>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None, "CORE0")
        headers = project.components["test"].headers

        self.assertCountEqual(
            headers,
            [
                pathlib.Path('with_core/test.h'),
                pathlib.Path('without_cores/test.h'),
            ],
        )


class ParseSourcesTest(unittest.TestCase):
    """parse_sources tests."""

    def test_parse_sources(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <source relative_path="src" type="src">
                <files mask="main.cc"/>
                <files mask="test.cc"/>
              </source>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        sources = project.components["test"].sources

        self.assertCountEqual(
            sources, [pathlib.Path('src/main.cc'), pathlib.Path('src/test.cc')]
        )

    def test_with_base_path(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test" package_base_path="src">
              <source relative_path="app" type="src">
                <files mask="main.cc"/>
                <files mask="test.cc"/>
              </source>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        sources = project.components["test"].sources

        self.assertCountEqual(
            sources,
            [pathlib.Path('src/app/main.cc'), pathlib.Path('src/app/test.cc')],
        )

    def test_multiple_sets(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <source relative_path="shared" type="src">
                <files mask="test.cc"/>
              </source>
              <source relative_path="lib" type="src_c">
                <files mask="test.c"/>
              </source>
              <source relative_path="app" type="src_cpp">
                <files mask="main.cc"/>
              </source>
              <source relative_path="startup" type="asm_include">
                <files mask="boot.s"/>
              </source>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        sources = project.components["test"].sources

        self.assertCountEqual(
            sources,
            [
                pathlib.Path('shared/test.cc'),
                pathlib.Path('lib/test.c'),
                pathlib.Path('app/main.cc'),
                pathlib.Path('startup/boot.s'),
            ],
        )

    def test_unknown_type(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
            <source relative_path="src" type="rust">
                <files mask="test.rs"/>
              </source>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        sources = project.components["test"].sources

        self.assertCountEqual(sources, [])

    def test_no_sources(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        sources = project.components["test"].sources

        self.assertCountEqual(sources, [])

    def test_device_cores(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <source relative_path="with_core" type="src" device_cores="CORE0">
                <files mask="main.cc"/>
              </source>
              <source relative_path="without_core" type="src"
                      device_cores="CORE1">
                <files mask="main.cc"/>
              </source>
              <source relative_path="without_cores" type="src">
                <files mask="main.cc"/>
              </source>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None, "CORE0")
        sources = project.components["test"].sources

        self.assertCountEqual(
            sources,
            [
                pathlib.Path('with_core/main.cc'),
                pathlib.Path('without_cores/main.cc'),
            ],
        )


class ParseLibsTest(unittest.TestCase):
    """parse_libs tests."""

    def test_parse_libs(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <source toolchain="armgcc" relative_path="gcc" type="lib">
                <files mask="libtest.a"/>
                <files mask="libtest_arm.a"/>
              </source>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        libs = project.components["test"].libs

        self.assertCountEqual(
            libs,
            [pathlib.Path('gcc/libtest.a'), pathlib.Path('gcc/libtest_arm.a')],
        )

    def test_with_base_path(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test" package_base_path="src">
              <source toolchain="armgcc" relative_path="gcc" type="lib">
                <files mask="libtest.a"/>
                <files mask="libtest_arm.a"/>
              </source>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        libs = project.components["test"].libs

        self.assertCountEqual(
            libs,
            [
                pathlib.Path('src/gcc/libtest.a'),
                pathlib.Path('src/gcc/libtest_arm.a'),
            ],
        )

    def test_multiple_sets(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <source toolchain="armgcc" relative_path="gcc" type="lib">
                <files mask="libtest.a"/>
              </source>
              <source toolchain="armgcc" relative_path="arm" type="lib">
                <files mask="libtest_arm.a"/>
              </source>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        libs = project.components["test"].libs

        self.assertCountEqual(
            libs,
            [pathlib.Path('gcc/libtest.a'), pathlib.Path('arm/libtest_arm.a')],
        )

    def test_multiple_toolchains(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <source toolchain="armgcc" relative_path="gcc" type="lib">
                <files mask="libtest.a"/>
              </source>
              <source toolchain="mcuxpresso" relative_path="arm" type="lib">
                <files mask="libtest_mcux.a"/>
              </source>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        libs = project.components["test"].libs

        self.assertCountEqual(
            libs,
            [pathlib.Path('gcc/libtest.a')],
        )

    def test_no_libs(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        libs = project.components["test"].libs

        self.assertCountEqual(libs, [])

    def test_device_cores(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <source toolchain="armgcc" relative_path="with_core" type="lib" device_cores="CORE0">
                <files mask="libtest.a"/>
              </source>
              <source toolchain="armgcc" relative_path="without_core" type="lib"
                      device_cores="CORE1">
                <files mask="libtest.a"/>
              </source>
              <source toolchain="armgcc" relative_path="without_cores" type="lib">
                <files mask="libtest.a"/>
              </source>
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None, "CORE0")
        libs = project.components["test"].libs

        self.assertCountEqual(
            libs,
            [
                pathlib.Path('with_core/libtest.a'),
                pathlib.Path('without_cores/libtest.a'),
            ],
        )


class ParseDependenciesTest(unittest.TestCase):
    """parse_dependencies tests."""

    def test_component_dependency(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <dependencies>
                <component_dependency value="foo"/>
              </dependencies>
            </component>
            <component id="foo">
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        dependencies = project.components["test"].dependencies

        self.assertCountEqual(dependencies, {'foo'})

    def test_all(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <dependencies>
                <all>
                  <component_dependency value="foo"/>
                  <component_dependency value="bar"/>
                  <component_dependency value="baz"/>
                </all>
              </dependencies>
            </component>
            <component id="foo">
            </component>
            <component id="bar">
            </component>
            <component id="baz">
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], None)
        dependencies = project.components["test"].dependencies

        self.assertCountEqual(dependencies, {'foo', 'bar', 'baz'})

    def test_any_of(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <dependencies>
                <any_of>
                  <component_dependency value="foo"/>
                  <component_dependency value="bar"/>
                  <component_dependency value="baz"/>
                </any_of>
              </dependencies>
            </component>
            <component id="foo">
            </component>
            <component id="bar">
            </component>
            <component id="baz">
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["test", "foo"], None)
        dependencies = project.components["test"].dependencies

        self.assertCountEqual(dependencies, {'foo'})

    def test_any_of_inside_all(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <dependencies>
                <all>
                  <component_dependency value="foo"/>
                  <component_dependency value="bar"/>
                  <component_dependency value="baz"/>
                  <any_of>
                    <all>
                      <component_dependency value="frodo"/>
                      <component_dependency value="bilbo"/>
                    </all>
                    <component_dependency value="gandalf"/>
                  </any_of>
                </all>
              </dependencies>
            </component>
            <component id="foo">
            </component>
            <component id="bar">
            </component>
            <component id="baz">
            </component>
            <component id="frodo">
            </component>
            <component id="bilbo">
            </component>
            <component id="gandalf">
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project(
            [manifest], pathlib.Path.cwd(), ["test", "frodo", "bilbo"], None
        )
        dependencies = project.components["test"].dependencies

        self.assertCountEqual(
            dependencies, {'foo', 'bar', 'baz', 'frodo', 'bilbo'}
        )

    def test_no_dependencies(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
            </component>
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project([manifest], pathlib.Path.cwd(), ["*"], ["gandalf"])
        dependencies = project.components["test"].dependencies

        self.assertCountEqual(dependencies, [])


class ProjectTest(unittest.TestCase):
    """Project tests."""

    def test_create_project(self):
        """test creating a project."""
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <dependencies>
                <component_dependency value="foo"/>
                <component_dependency value="bar"/>
                <any_of>
                  <component_dependency value="baz"/>
                </any_of>
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
              <!-- core0 should be included in the project -->
              <source relative_path="include" type="c_include"
                      device_cores="CORE0">
                <files mask="core0.h"/>
              </source>
              <source relative_path="src" type="src" device_cores="CORE0">
                <files mask="core0.cc"/>
              </source>
              <!-- core1 should not be included in the project -->
              <source relative_path="include" type="c_include"
                      device_cores="CORE1">
                <files mask="core1.h"/>
              </source>
              <source relative_path="src" type="src" device_cores="CORE1">
                <files mask="core0.cc"/>
              </source>
              <!-- common should be -->
              <source relative_path="include" type="c_include"
                      device_cores="CORE0 CORE1">
                <files mask="common.h"/>
              </source>
              <source relative_path="src" type="src" device_cores="CORE0 CORE1">
                <files mask="common.cc"/>
              </source>
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
            <!-- baz should not be included in the output -->
            <component id="baz" package_base_path="baz">
              <defines>
                <define name="BAZ"/>
              </defines>
              <source relative_path="include" type="c_include">
                <files mask="baz.h"/>
              </source>
              <source relative_path="src" type="src">
                <files mask="baz.cc"/>
              </source>
              <include_paths>
                <include_path relative_path="include" type="c_include"/>
              </include_paths>
            </component>
            <component id="frodo" package_base_path="frodo">
              <dependencies>
                <component_dependency value="bilbo"/>
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
            <!-- bilbo should be excluded from the project -->
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
          </components>
        </manifest>
        '''
        manifest = (
            xml.etree.ElementTree.fromstring(test_manifest_xml),
            pathlib.Path.cwd() / "manifest.xml",
        )
        project = Project(
            [manifest],
            pathlib.Path.cwd(),
            ["test", "frodo"],
            ["baz"],
            "CORE0",
        )
        components = project.components.values()

        component_ids = (component.id for component in components)
        self.assertCountEqual(
            component_ids, ['test', 'frodo', 'bilbo', 'foo', 'bar']
        )

        defines = chain.from_iterable(
            component.defines for component in components
        )
        self.assertCountEqual(defines, ['FRODO', 'BILBO', 'FOO', 'BAR'])

        include_dirs = chain.from_iterable(
            component.include_dirs for component in components
        )
        self.assertCountEqual(
            include_dirs,
            [
                pathlib.Path('frodo/include'),
                pathlib.Path('bilbo/include'),
                pathlib.Path('foo/include'),
                pathlib.Path('bar/include'),
            ],
        )

        headers = chain.from_iterable(
            component.headers for component in components
        )
        self.assertCountEqual(
            headers,
            [
                pathlib.Path('frodo/include/frodo.h'),
                pathlib.Path('bilbo/include/bilbo.h'),
                pathlib.Path('foo/include/foo.h'),
                pathlib.Path('foo/include/core0.h'),
                pathlib.Path('foo/include/common.h'),
                pathlib.Path('bar/include/bar.h'),
            ],
        )

        sources = chain.from_iterable(
            component.sources for component in components
        )
        self.assertCountEqual(
            sources,
            [
                pathlib.Path('frodo/src/frodo.cc'),
                pathlib.Path('bilbo/src/bilbo.cc'),
                pathlib.Path('foo/src/foo.cc'),
                pathlib.Path('foo/src/core0.cc'),
                pathlib.Path('foo/src/common.cc'),
                pathlib.Path('bar/src/bar.cc'),
            ],
        )

        libs = chain.from_iterable(component.libs for component in components)
        self.assertCountEqual(libs, [pathlib.Path('frodo/libonering.a')])


if __name__ == '__main__':
    unittest.main()
