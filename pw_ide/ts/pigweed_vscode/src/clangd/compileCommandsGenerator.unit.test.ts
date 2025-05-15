// Copyright 2025 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

import * as assert from 'assert';

import {
  applyVirtualIncludeFix,
  generateCompileCommands,
  generateCompileCommandsFromAqueryCquery,
  inferPlatformOfAction,
  parseBazelBuildCommand,
  resolveVirtualIncludesToRealPaths,
} from './compileCommandsGenerator';
import { CompileCommand } from './parser';
import path from 'path';
import { getReliableBazelExecutable } from '../bazel';
import { workingDir } from '../settings/vscode';
import { CDB_FILE_DIR, CDB_FILE_NAME } from './paths';
import { MockLoggerUI } from './compileCommandsGeneratorUI';

function fixPathSeparator(p: string) {
  return p.replace(/\//g, path.sep);
}

function fixPathSeparators(paths: string[]) {
  return paths.map(fixPathSeparator);
}

const SAMPLE_AQUERY_ACTION = {
  targetId: 423,
  targetLabel: fixPathSeparator(
    '//pw_containers/intrusive_map:intrusive_map_test',
  ),
  actionKey: '990ae6a0baba62cba42e7576fa759c8622188f59632495aaefec66cc77bab3d6',
  mnemonic: 'CppCompile',
  configurationId: 2,
  arguments: fixPathSeparators([
    'external/+_repo_rules5+llvm_toolchain_macos/bin/clang++',
    '--sysroot\u003dexternal/+_repo_rules3+macos_sysroot',
    '-Wthread-safety',
    '-D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS\u003d1',
    '-g',
    '-fno-common',
    '-fno-exceptions',
    '-ffunction-sections',
    '-fdata-sections',
    '-no-canonical-prefixes',
    '-fno-rtti',
    '-Wno-register',
    '-Wnon-virtual-dtor',
    '-Wall',
    '-Wextra',
    '-Wimplicit-fallthrough',
    '-Wcast-qual',
    '-Wpointer-arith',
    '-Wshadow',
    '-Wredundant-decls',
    '-Werror',
    '-Wno-error\u003dcpp',
    '-Wno-error\u003ddeprecated-declarations',
    '-fdiagnostics-color',
    '-MD',
    '-MF',
    'bazel-out/darwin_arm64-fastbuild/bin/pw_containers/_objs/intrusive_map_test/intrusive_map_test.pic.d',
    '-frandom-seed\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_containers/_objs/intrusive_map_test/intrusive_map_test.pic.o',
    '-fPIC',
    '-DPW_FUNCTION_ENABLE_DYNAMIC_ALLOCATION\u003d1',
    '-DPW_STATUS_CFG_CHECK_IF_USED\u003d1',
    '-iquote',
    '.',
    '-iquote',
    'bazel-out/darwin_arm64-fastbuild/bin',
    '-iquote',
    'external/rules_cc+',
    '-iquote',
    'bazel-out/darwin_arm64-fastbuild/bin/external/rules_cc+',
    '-iquote',
    'external/bazel_tools',
    '-iquote',
    'bazel-out/darwin_arm64-fastbuild/bin/external/bazel_tools',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_containers/_virtual_includes/intrusive_map',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_containers/_virtual_includes/aa_tree',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_containers/_virtual_includes/intrusive_item',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_assert/_virtual_includes/assert',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_assert/_virtual_includes/config',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_assert/_virtual_includes/print_and_abort_assert_backend',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_assert/_virtual_includes/print_and_abort',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_bytes/_virtual_includes/packed_ptr',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_function/_virtual_includes/pw_function',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_function/_virtual_includes/config',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_preprocessor/_virtual_includes/pw_preprocessor',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_polyfill/_virtual_includes/pw_polyfill',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_containers/_virtual_includes/intrusive_multimap',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_compilation_testing/_virtual_includes/negative_compilation_testing',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_span/_virtual_includes/pw_span',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_span/_virtual_includes/config',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/_virtual_includes/pw_unit_test',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/_virtual_includes/status_macros',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_status/_virtual_includes/pw_status',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/_virtual_includes/config',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/_virtual_includes/event_handler',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/_virtual_includes/pw_unit_test.facade',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_bytes/_virtual_includes/alignment',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_bytes/_virtual_includes/pw_bytes',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_bytes/_virtual_includes/bit',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_containers/_virtual_includes/iterator',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_string/_virtual_includes/builder',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_string/_virtual_includes/format',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_string/_virtual_includes/string',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_string/_virtual_includes/to_string',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_string/_virtual_includes/config',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_string/_virtual_includes/util',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_result/_virtual_includes/pw_result',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/_virtual_includes/simple_printing_event_handler',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/_virtual_includes/googletest_style_event_handler',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_sys_io/_virtual_includes/pw_sys_io',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_sys_io/_virtual_includes/pw_sys_io.facade',
    '-isystem',
    'third_party/fuchsia/repo/sdk/lib/stdcompat/include',
    '-isystem',
    'bazel-out/darwin_arm64-fastbuild/bin/third_party/fuchsia/repo/sdk/lib/stdcompat/include',
    '-isystem',
    'third_party/fuchsia/repo/sdk/lib/fit/include',
    '-isystem',
    'bazel-out/darwin_arm64-fastbuild/bin/third_party/fuchsia/repo/sdk/lib/fit/include',
    '-isystem',
    'pw_unit_test/light_public_overrides',
    '-isystem',
    'bazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/light_public_overrides',
    '-isystem',
    'pw_unit_test/public_overrides',
    '-isystem',
    'bazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/public_overrides',
    '-std\u003dc++17',
    '-fmodules-strict-decluse',
    '-Wprivate-header',
    '-Xclang',
    '-fmodule-name\u003d//pw_containers:intrusive_map_test',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_containers/intrusive_map_test.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_toolchain/host_clang/module.modulemap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_containers/intrusive_map.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_containers/intrusive_multimap.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_compilation_testing/negative_compilation_testing.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_span/pw_span.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_build/default_link_extra_lib.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/pw_unit_test.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/simple_printing_main.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/external/rules_cc+/link_extra_lib.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/external/bazel_tools/tools/cpp/link_extra_lib.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/external/bazel_tools/tools/cpp/malloc.cppmap',
    '-c',
    'pw_containers/intrusive_map_test.cc',
    '-o',
    'bazel-out/darwin_arm64-fastbuild/bin/pw_containers/_objs/intrusive_map_test/intrusive_map_test.pic.o',
  ]),
  environmentVariables: [
    {
      key: 'PATH',
      value: '/bin:/usr/bin:/usr/local/bin',
    },
    {
      key: 'PWD',
      value: '/proc/self/cwd',
    },
  ],
  discoversInputs: true,
  executionPlatform: '@@platforms//host:host',
  platform: 'darwin_arm64-fastbuild',
};

const SAMPLE_CQUERY_HEADERS = [
  [
    fixPathSeparator('//pw_containers:intrusive_map'),
    fixPathSeparators([
      'pw_containers/public/pw_containers/intrusive_map.h',
      'bazel-out/darwin_arm64-fastbuild/bin/pw_containers/_virtual_includes/intrusive_map/pw_containers/intrusive_map.h',
    ]),
  ],
];

const SAMPLE_COMPILE_COMMAND = {
  file: fixPathSeparator('pw_containers/intrusive_map_test.cc'),
  directory: fixPathSeparator('bazel-out/darwin_arm64-fastbuild/bin'),
  arguments: fixPathSeparators([
    'external/+_repo_rules5+llvm_toolchain_macos/bin/clang++',
    '--sysroot\u003dexternal/+_repo_rules3+macos_sysroot',
    '-Wthread-safety',
    '-D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS\u003d1',
    '-g',
    '-fno-common',
    '-fno-exceptions',
    '-ffunction-sections',
    '-fdata-sections',
    '-no-canonical-prefixes',
    '-fno-rtti',
    '-Wno-register',
    '-Wnon-virtual-dtor',
    '-Wall',
    '-Wextra',
    '-Wimplicit-fallthrough',
    '-Wcast-qual',
    '-Wpointer-arith',
    '-Wshadow',
    '-Wredundant-decls',
    '-Werror',
    '-Wno-error\u003dcpp',
    '-Wno-error\u003ddeprecated-declarations',
    '-fdiagnostics-color',
    '-MD',
    '-MF',
    'bazel-out/darwin_arm64-fastbuild/bin/pw_containers/_objs/intrusive_map_test/intrusive_map_test.pic.d',
    '-frandom-seed\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_containers/_objs/intrusive_map_test/intrusive_map_test.pic.o',
    '-fPIC',
    '-DPW_FUNCTION_ENABLE_DYNAMIC_ALLOCATION\u003d1',
    '-DPW_STATUS_CFG_CHECK_IF_USED\u003d1',
    '-iquote',
    '.',
    '-iquote',
    'bazel-out/darwin_arm64-fastbuild/bin',
    '-iquote',
    'external/rules_cc+',
    '-iquote',
    'bazel-out/darwin_arm64-fastbuild/bin/external/rules_cc+',
    '-iquote',
    'external/bazel_tools',
    '-iquote',
    'bazel-out/darwin_arm64-fastbuild/bin/external/bazel_tools',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_containers/_virtual_includes/intrusive_map',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_containers/_virtual_includes/aa_tree',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_containers/_virtual_includes/intrusive_item',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_assert/_virtual_includes/assert',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_assert/_virtual_includes/config',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_assert/_virtual_includes/print_and_abort_assert_backend',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_assert/_virtual_includes/print_and_abort',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_bytes/_virtual_includes/packed_ptr',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_function/_virtual_includes/pw_function',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_function/_virtual_includes/config',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_preprocessor/_virtual_includes/pw_preprocessor',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_polyfill/_virtual_includes/pw_polyfill',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_containers/_virtual_includes/intrusive_multimap',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_compilation_testing/_virtual_includes/negative_compilation_testing',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_span/_virtual_includes/pw_span',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_span/_virtual_includes/config',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/_virtual_includes/pw_unit_test',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/_virtual_includes/status_macros',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_status/_virtual_includes/pw_status',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/_virtual_includes/config',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/_virtual_includes/event_handler',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/_virtual_includes/pw_unit_test.facade',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_bytes/_virtual_includes/alignment',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_bytes/_virtual_includes/pw_bytes',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_bytes/_virtual_includes/bit',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_containers/_virtual_includes/iterator',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_string/_virtual_includes/builder',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_string/_virtual_includes/format',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_string/_virtual_includes/string',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_string/_virtual_includes/to_string',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_string/_virtual_includes/config',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_string/_virtual_includes/util',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_result/_virtual_includes/pw_result',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/_virtual_includes/simple_printing_event_handler',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/_virtual_includes/googletest_style_event_handler',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_sys_io/_virtual_includes/pw_sys_io',
    '-Ibazel-out/darwin_arm64-fastbuild/bin/pw_sys_io/_virtual_includes/pw_sys_io.facade',
    '-isystem',
    'third_party/fuchsia/repo/sdk/lib/stdcompat/include',
    '-isystem',
    'bazel-out/darwin_arm64-fastbuild/bin/third_party/fuchsia/repo/sdk/lib/stdcompat/include',
    '-isystem',
    'third_party/fuchsia/repo/sdk/lib/fit/include',
    '-isystem',
    'bazel-out/darwin_arm64-fastbuild/bin/third_party/fuchsia/repo/sdk/lib/fit/include',
    '-isystem',
    'pw_unit_test/light_public_overrides',
    '-isystem',
    'bazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/light_public_overrides',
    '-isystem',
    'pw_unit_test/public_overrides',
    '-isystem',
    'bazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/public_overrides',
    '-std\u003dc++17',
    '-fmodules-strict-decluse',
    '-Wprivate-header',
    '-Xclang',
    '-fmodule-name\u003d//pw_containers:intrusive_map_test',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_containers/intrusive_map_test.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_toolchain/host_clang/module.modulemap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_containers/intrusive_map.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_containers/intrusive_multimap.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_compilation_testing/negative_compilation_testing.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_span/pw_span.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_build/default_link_extra_lib.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/pw_unit_test.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/pw_unit_test/simple_printing_main.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/external/rules_cc+/link_extra_lib.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/external/bazel_tools/tools/cpp/link_extra_lib.cppmap',
    '-Xclang',
    '-fmodule-map-file\u003dbazel-out/darwin_arm64-fastbuild/bin/external/bazel_tools/tools/cpp/malloc.cppmap',
    '-c',
    'pw_containers/intrusive_map_test.cc',
    '-o',
    'bazel-out/darwin_arm64-fastbuild/bin/pw_containers/_objs/intrusive_map_test/intrusive_map_test.pic.o',
  ]),
};

test('inferPlatformOfAction', () => {
  assert.deepEqual(
    inferPlatformOfAction(SAMPLE_AQUERY_ACTION),
    'darwin_arm64-fastbuild',
  );
});

test('resolveVirtualIncludesToRealPaths', () => {
  const cqueryHeaderWithNoVirtualInclude = [
    [
      '//pw_containers:intrusive_map',
      fixPathSeparators([
        'pw_containers/public/pw_containers/intrusive_map.h',
        'pw_containers/public/pw_containers/intrusive_map.h',
      ]),
    ],
  ];

  const map = resolveVirtualIncludesToRealPaths(SAMPLE_CQUERY_HEADERS as any);
  const mapEmpty = resolveVirtualIncludesToRealPaths(
    cqueryHeaderWithNoVirtualInclude as any,
  );

  assert.equal(Object.values(map)[0], fixPathSeparator('pw_containers/public'));
  assert.equal(Object.values(mapEmpty).length, 0);
});

test('applyVirtualIncludeFix', () => {
  const headersMap = resolveVirtualIncludesToRealPaths(
    SAMPLE_CQUERY_HEADERS as any,
  );
  const compileCommandFixed = applyVirtualIncludeFix(
    new CompileCommand(SAMPLE_COMPILE_COMMAND),
    headersMap,
  );

  assert.deepEqual(
    compileCommandFixed.data.arguments!.filter(
      (arg: string) =>
        arg.indexOf(fixPathSeparator('_virtual_includes/intrusive_map')) >= 0,
    ),
    [],
  );
});

test('generateCompileCommandsFromAqueryCquery', async () => {
  const compileCommandPerPlatform =
    await generateCompileCommandsFromAqueryCquery(
      __dirname,
      [SAMPLE_AQUERY_ACTION],
      SAMPLE_CQUERY_HEADERS as any,
    );

  assert.notEqual(
    compileCommandPerPlatform.get('darwin_arm64-fastbuild'),
    undefined,
  );
  const compileCommands = compileCommandPerPlatform.get(
    'darwin_arm64-fastbuild',
  )!;
  assert.equal(compileCommands.db.length, 1);
  const compileCommand = compileCommands.db[0];
  assert.equal(
    compileCommand.data.file,
    fixPathSeparator('pw_containers/intrusive_map_test.cc'),
  );
});

test('parseBazelBuildCommand_singleTarget_noArgs', async () => {
  const bazel = getReliableBazelExecutable();
  const mockLogger = new MockLoggerUI();
  await generateCompileCommands(
    bazel!,
    workingDir.get(),
    CDB_FILE_DIR,
    CDB_FILE_NAME,
    ['//pw_i2c_rp2040:pw_i2c_rp2040', '//pw_status'],
    [],
    mockLogger as any,
  );
  assert.equal(
    mockLogger
      .getStdout()
      .indexOf('Finished generating compile_commands.json') > 0,
    true,
  );
  assert.equal(
    mockLogger.getStderr().indexOf('aquery failed with exit code') > 0,
    true,
  );
});

test('parseBazelBuildCommand_singleTarget_noArgs', async () => {
  const bazel = getReliableBazelExecutable();
  const { targets, args } = await parseBazelBuildCommand(
    'build //pw_status/...',
    bazel!,
    workingDir.get(),
  );
  assert.deepEqual(targets, ['//pw_status/...']);
  assert.deepEqual(args, []);
});

test.only('parseBazelBuildCommand_singleTargetWith@_noArgs', async () => {
  const bazel = getReliableBazelExecutable();
  const { targets, args } = await parseBazelBuildCommand(
    'build @pigweed//pw_status/...',
    bazel!,
    workingDir.get(),
  );
  assert.deepEqual(targets, ['@pigweed//pw_status/...']);
  assert.deepEqual(args, []);
});

test('parseBazelBuildCommand_multipleTargets_noArgs', async () => {
  const bazel = getReliableBazelExecutable();
  const command = 'build //pw_string:pw_string //pw_status/ts:js_files';
  const { targets, args } = await parseBazelBuildCommand(
    command,
    bazel!,
    workingDir.get(),
  );
  assert.deepEqual(targets, [
    '//pw_string:pw_string',
    '//pw_status/ts:js_files',
  ]);
  assert.deepEqual(args, []);
});

test('parseBazelBuildCommand_oneConfigArg_oneTarget', async () => {
  const bazel = getReliableBazelExecutable();
  const command = 'build --config rp2040 //pw_status/...';
  const { targets, args } = await parseBazelBuildCommand(
    command,
    bazel!,
    workingDir.get(),
  );
  assert.deepEqual(targets, ['//pw_status/...']);
  assert.deepEqual(args, ['--config=rp2040']);
});

test('parseBazelBuildCommand_onePlatformArg_oneTarget_testSubcommand', async () => {
  const bazel = getReliableBazelExecutable();
  const command =
    'test --platforms=@pigweed//targets/rp2040 //pw_string:pw_string';
  const { targets, args } = await parseBazelBuildCommand(
    command,
    bazel!,
    workingDir.get(),
  );
  assert.deepEqual(targets, ['//pw_string:pw_string']);
  assert.deepEqual(args, ['--platforms=@pigweed//targets/rp2040']);
});

test('parseBazelBuildCommand_bothArgs_multipleTargets', async () => {
  const bazel = getReliableBazelExecutable();
  const command =
    'build --config rp2040 --platforms=@pigweed//targets/rp2040 //pw_status/... //pw_string/...';
  const { targets, args } = await parseBazelBuildCommand(
    command,
    bazel!,
    workingDir.get(),
  );
  assert.deepEqual(targets, ['//pw_status/...', '//pw_string/...']);
  // Order of args from canonicalize-flags can vary. Sorting makes the test more robust.
  const expectedArgs = [
    '--config=rp2040',
    '--platforms=@pigweed//targets/rp2040',
  ].sort();
  assert.deepEqual(
    [...args].sort(),
    expectedArgs,
    `Expected sorted args ${expectedArgs} but got ${args}`,
  );
});

test('parseBazelBuildCommand_target_thenConfigArg', async () => {
  const bazel = getReliableBazelExecutable();
  const command = 'build //pw_status/ts:js_files --config rp2040';
  const { targets, args } = await parseBazelBuildCommand(
    command,
    bazel!,
    workingDir.get(),
  );
  assert.deepEqual(targets, ['//pw_status/ts:js_files']);
  assert.deepEqual(args, ['--config=rp2040']);
});

test('parseBazelBuildCommand_arg_target_arg_target', async () => {
  const bazel = getReliableBazelExecutable();
  const command =
    'build --config rp2040 //pw_status/... --platforms=@pigweed//targets/rp2040 //pw_string:pw_string';
  const { targets, args } = await parseBazelBuildCommand(
    command,
    bazel!,
    workingDir.get(),
  );
  assert.deepEqual(targets, ['//pw_status/...', '//pw_string:pw_string']);
  const expectedArgs = [
    '--config=rp2040',
    '--platforms=@pigweed//targets/rp2040',
  ].sort();
  assert.deepEqual(
    [...args].sort(),
    expectedArgs,
    `Expected sorted args ${expectedArgs} but got ${args}`,
  );
});

test('parseBazelBuildCommand_platformArg_in_quotes', async () => {
  const bazel = getReliableBazelExecutable();
  const command =
    'test --platforms=@pigweed//targets/rp2040 --define=VAR="some value with spaces" //pw_status/ts:js_files';
  const { targets, args } = await parseBazelBuildCommand(
    command,
    bazel!,
    workingDir.get(),
  );
  assert.deepEqual(targets, ['//pw_status/ts:js_files']);
  const expectedArgs = [
    '--define=VAR="some value with spaces"',
    '--platforms=@pigweed//targets/rp2040',
  ].sort();
  assert.deepEqual(
    [...args].sort(),
    expectedArgs,
    `Expected sorted args ${expectedArgs} but got ${args}`,
  );
});

test('parseBazelBuildCommand_platformArg_before_configArg', async () => {
  const bazel = getReliableBazelExecutable();
  const command =
    'test --platforms=@pigweed//targets/rp2040 --config rp2040 //pw_status/ts:js_files';
  const { targets, args } = await parseBazelBuildCommand(
    command,
    bazel!,
    workingDir.get(),
  );
  assert.deepEqual(targets, ['//pw_status/ts:js_files']);
  const expectedArgs = [
    '--config=rp2040',
    '--platforms=@pigweed//targets/rp2040',
  ].sort();
  assert.deepEqual(
    [...args].sort(),
    expectedArgs,
    `Expected sorted args ${expectedArgs} but got ${args}`,
  );
});

test('parseBazelBuildCommand_error_noTargets_withSpecifiedArgs', async () => {
  const bazel = getReliableBazelExecutable();
  const command = 'build --config rp2040 --platforms=@pigweed//targets/rp2040';
  await assert.rejects(
    parseBazelBuildCommand(command, bazel!, workingDir.get()),
    (err: Error) => {
      assert.ok(err instanceof Error);
      assert.match(
        err.message,
        /Could not find any bazel targets \(starting with \/\/ or :\) in command: build --config rp2040 --platforms=@pigweed\/\/targets\/rp2040/,
      );
      return true;
    },
  );
});

test('parseBazelBuildCommand_onlySubcommand_build', async () => {
  const bazel = getReliableBazelExecutable();
  await assert.rejects(
    parseBazelBuildCommand('build', bazel!, workingDir.get()),
    /Could not find any bazel targets \(starting with \/\/ or :\) in command: build/,
  );
});

test('parseBazelBuildCommand_error_emptyCommand_string', async () => {
  const bazel = getReliableBazelExecutable();
  await assert.rejects(
    parseBazelBuildCommand('', bazel!, workingDir.get()),
    /Invalid bazel command \(empty\): /,
  );
});

test('parseBazelBuildCommand_error_invalidSubcommand_withSpecifiedArgsAndTarget', async () => {
  const bazel = getReliableBazelExecutable();
  const command = 'shipit --config rp2040 //pw_status/...'; // "shipit" is not a standard bazel command
  await assert.rejects(
    parseBazelBuildCommand(command, bazel!, workingDir.get()),
    (err: Error) => {
      assert.ok(err instanceof Error, 'Error should be an instance of Error');
      // This error comes from bazel canonicalize-flags failing due to invalid --for_command
      assert.ok(
        /Error during bazel canonicalize-flags: bazel canonicalize-flags failed with exit code/.test(
          err.message,
        ) || /No such command 'shipit'/.test(err.message), // Actual Bazel error might vary
        `Unexpected error message: ${err.message}`,
      );
      return true;
    },
  );
});
