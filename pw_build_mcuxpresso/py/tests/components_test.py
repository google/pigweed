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

from pw_build_mcuxpresso import components


class GetComponentTest(unittest.TestCase):
    """get_component tests."""

    def test_without_basepath(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)

        (component, base_path) = components.get_component(root, 'test')

        self.assertIsInstance(component, xml.etree.ElementTree.Element)
        self.assertEqual(component.tag, 'component')
        self.assertEqual(base_path, None)

    def test_with_basepath(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test" package_base_path="test">
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)

        (component, base_path) = components.get_component(root, 'test')

        self.assertIsInstance(component, xml.etree.ElementTree.Element)
        self.assertEqual(component.tag, 'component')
        self.assertEqual(base_path, pathlib.Path('test'))

    def test_component_not_found(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="other">
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)

        (component, base_path) = components.get_component(root, 'test')

        self.assertEqual(component, None)
        self.assertEqual(base_path, None)

    def test_with_device_cores(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test" package_base_path="test" device_cores="CORE0">
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)

        (component, base_path) = components.get_component(
            root, 'test', device_core='CORE0'
        )

        self.assertIsInstance(component, xml.etree.ElementTree.Element)
        self.assertEqual(component.tag, 'component')
        self.assertEqual(base_path, pathlib.Path('test'))

    def test_device_core_not_found(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test" package_base_path="test" device_cores="CORE0">
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)

        (component, base_path) = components.get_component(
            root, 'test', device_core='CORE1'
        )

        self.assertEqual(component, None)
        self.assertEqual(base_path, None)


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
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        defines = components.parse_defines(root, 'test')

        self.assertEqual(defines, ['TEST_WITH_VALUE=1', 'TEST_WITHOUT_VALUE'])

    def test_no_defines(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        defines = components.parse_defines(root, 'test')

        self.assertEqual(defines, [])

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
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        defines = components.parse_defines(root, 'test', device_core='CORE0')

        self.assertEqual(defines, ['TEST_WITH_CORE', 'TEST_WITHOUT_CORES'])


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
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        include_paths = components.parse_include_paths(root, 'test')

        self.assertEqual(
            include_paths, [pathlib.Path('example'), pathlib.Path('asm')]
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
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        include_paths = components.parse_include_paths(root, 'test')

        self.assertEqual(
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
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        include_paths = components.parse_include_paths(root, 'test')

        self.assertEqual(include_paths, [])

    def test_no_include_paths(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        include_paths = components.parse_include_paths(root, 'test')

        self.assertEqual(include_paths, [])

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
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        include_paths = components.parse_include_paths(
            root, 'test', device_core='CORE0'
        )

        self.assertEqual(
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
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        headers = components.parse_headers(root, 'test')

        self.assertEqual(
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
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        headers = components.parse_headers(root, 'test')

        self.assertEqual(
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
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        headers = components.parse_headers(root, 'test')

        self.assertEqual(
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
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        headers = components.parse_headers(root, 'test')

        self.assertEqual(headers, [])

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
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        headers = components.parse_headers(root, 'test', device_core='CORE0')

        self.assertEqual(
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
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        sources = components.parse_sources(root, 'test')

        self.assertEqual(
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
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        sources = components.parse_sources(root, 'test')

        self.assertEqual(
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
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        sources = components.parse_sources(root, 'test')

        self.assertEqual(
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
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        sources = components.parse_sources(root, 'test')

        self.assertEqual(sources, [])

    def test_no_sources(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        sources = components.parse_sources(root, 'test')

        self.assertEqual(sources, [])

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
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        sources = components.parse_sources(root, 'test', device_core='CORE0')

        self.assertEqual(
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
              <source relative_path="gcc" type="lib">
                <files mask="libtest.a"/>
                <files mask="libtest_arm.a"/>
              </source>
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        libs = components.parse_libs(root, 'test')

        self.assertEqual(
            libs,
            [pathlib.Path('gcc/libtest.a'), pathlib.Path('gcc/libtest_arm.a')],
        )

    def test_with_base_path(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test" package_base_path="src">
              <source relative_path="gcc" type="lib">
                <files mask="libtest.a"/>
                <files mask="libtest_arm.a"/>
              </source>
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        libs = components.parse_libs(root, 'test')

        self.assertEqual(
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
              <source relative_path="gcc" type="lib">
                <files mask="libtest.a"/>
              </source>
              <source relative_path="arm" type="lib">
                <files mask="libtest_arm.a"/>
              </source>
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        libs = components.parse_libs(root, 'test')

        self.assertEqual(
            libs,
            [pathlib.Path('gcc/libtest.a'), pathlib.Path('arm/libtest_arm.a')],
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
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        libs = components.parse_libs(root, 'test')

        self.assertEqual(libs, [])

    def test_device_cores(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <source relative_path="with_core" type="lib" device_cores="CORE0">
                <files mask="libtest.a"/>
              </source>
              <source relative_path="without_core" type="lib"
                      device_cores="CORE1">
                <files mask="libtest.a"/>
              </source>
              <source relative_path="without_cores" type="lib">
                <files mask="libtest.a"/>
              </source>
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        libs = components.parse_libs(root, 'test', device_core='CORE0')

        self.assertEqual(
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
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        dependencies = components.parse_dependencies(root, 'test')

        self.assertEqual(dependencies, ['foo'])

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
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        dependencies = components.parse_dependencies(root, 'test')

        self.assertEqual(dependencies, ['foo', 'bar', 'baz'])

    def test_any_of_ignored(self):
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
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        dependencies = components.parse_dependencies(root, 'test')

        self.assertEqual(dependencies, [])

    def test_any_of_inside_all_ignored(self):
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
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        dependencies = components.parse_dependencies(root, 'test')

        self.assertEqual(dependencies, ['foo', 'bar', 'baz'])

    def test_no_dependencies(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        dependencies = components.parse_dependencies(root, 'test')

        self.assertEqual(dependencies, [])


class CheckDependenciesTest(unittest.TestCase):
    """check_dependencies tests."""

    def test_any_of_satisfied(self):
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
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        satisfied = components.check_dependencies(
            root, 'test', ['test', 'foo'], exclude=None
        )

        self.assertEqual(satisfied, True)

    def test_any_of_not_satisfied(self):
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
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        satisfied = components.check_dependencies(
            root, 'test', ['test'], exclude=None
        )

        self.assertEqual(satisfied, False)

    def test_any_of_satisfied_by_exclude(self):
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
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        satisfied = components.check_dependencies(
            root, 'test', ['test'], exclude=['foo']
        )

        self.assertEqual(satisfied, True)

    def test_any_of_all_satisfied(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <dependencies>
                <any_of>
                  <all>
                    <component_dependency value="foo"/>
                    <component_dependency value="bar"/>
                    <component_dependency value="baz"/>
                  </all>
                </any_of>
              </dependencies>
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        satisfied = components.check_dependencies(
            root, 'test', ['test', 'foo', 'bar', 'baz'], exclude=None
        )

        self.assertEqual(satisfied, True)

    def test_any_of_all_not_satisfied(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <dependencies>
                <any_of>
                  <all>
                    <component_dependency value="foo"/>
                    <component_dependency value="bar"/>
                    <component_dependency value="baz"/>
                  </all>
                </any_of>
              </dependencies>
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        satisfied = components.check_dependencies(
            root, 'test', ['test', 'foo', 'bar'], exclude=None
        )

        self.assertEqual(satisfied, False)

    def test_any_of_all_satisfied_by_exclude(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <dependencies>
                <any_of>
                  <all>
                    <component_dependency value="foo"/>
                    <component_dependency value="bar"/>
                    <component_dependency value="baz"/>
                  </all>
                </any_of>
              </dependencies>
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        satisfied = components.check_dependencies(
            root, 'test', ['test', 'foo', 'bar'], exclude=['baz']
        )

        self.assertEqual(satisfied, True)

    def test_any_of_all_or_one_satisfied(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <dependencies>
                <any_of>
                  <all>
                    <component_dependency value="foo"/>
                    <component_dependency value="bar"/>
                    <component_dependency value="baz"/>
                  </all>
                  <component_dependency value="frodo"/>
                </any_of>
              </dependencies>
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        satisfied = components.check_dependencies(
            root, 'test', ['test', 'frodo'], exclude=None
        )

        self.assertEqual(satisfied, True)

    def test_any_of_all_or_one_not_satisfied(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <dependencies>
                <any_of>
                  <all>
                    <component_dependency value="foo"/>
                    <component_dependency value="bar"/>
                    <component_dependency value="baz"/>
                  </all>
                  <component_dependency value="frodo"/>
                </any_of>
              </dependencies>
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        satisfied = components.check_dependencies(
            root, 'test', ['test'], exclude=None
        )

        self.assertEqual(satisfied, False)

    def test_any_of_all_or_one_satisfied_by_exclude(self):
        test_manifest_xml = '''
        <manifest>
          <components>
            <component id="test">
              <dependencies>
                <any_of>
                  <all>
                    <component_dependency value="foo"/>
                    <component_dependency value="bar"/>
                    <component_dependency value="baz"/>
                  </all>
                  <component_dependency value="frodo"/>
                </any_of>
              </dependencies>
            </component>
          </components>
        </manifest>
        '''
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        satisfied = components.check_dependencies(
            root, 'test', ['test'], exclude=['frodo']
        )

        self.assertEqual(satisfied, True)


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
              <source relative_path="./" type="lib">
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
        root = xml.etree.ElementTree.fromstring(test_manifest_xml)
        project = components.Project(
            root,
            ['test', 'frodo'],
            exclude=['baz', 'bilbo'],
            device_core='CORE0',
        )

        self.assertEqual(project.component_ids, ['test', 'frodo', 'foo', 'bar'])
        self.assertEqual(project.defines, ['FRODO', 'FOO', 'BAR'])
        self.assertEqual(
            project.include_dirs,
            [
                pathlib.Path('frodo/include'),
                pathlib.Path('foo/include'),
                pathlib.Path('bar/include'),
            ],
        )
        self.assertEqual(
            project.headers,
            [
                pathlib.Path('frodo/include/frodo.h'),
                pathlib.Path('foo/include/foo.h'),
                pathlib.Path('foo/include/core0.h'),
                pathlib.Path('foo/include/common.h'),
                pathlib.Path('bar/include/bar.h'),
            ],
        )
        self.assertEqual(
            project.sources,
            [
                pathlib.Path('frodo/src/frodo.cc'),
                pathlib.Path('foo/src/foo.cc'),
                pathlib.Path('foo/src/core0.cc'),
                pathlib.Path('foo/src/common.cc'),
                pathlib.Path('bar/src/bar.cc'),
            ],
        )
        self.assertEqual(project.libs, [pathlib.Path('frodo/libonering.a')])
        self.assertTrue(project.dependencies_satisfied)


if __name__ == '__main__':
    unittest.main()
