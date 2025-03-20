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
import * as vscode from 'vscode';
import { clangdPath, initBazelClangdPath } from './clangd/bazel';
import { executeInTerminalAndGetStdout } from './terminal';
import { patchBazeliskIntoTerminalPath } from './terminal';

suite('Extension Test Suite', () => {
  vscode.window.showInformationMessage('Starting tests.');
  test('Clang init', async () => {
    const success = await initBazelClangdPath();
    assert.strictEqual(success, true);
  });

  test('Clang is from Pigweed', async () => {
    const path = clangdPath();
    assert.strictEqual(path!.includes('pw_toolchain/host_clang'), true);
  });

  test('Bazel build test', async () => {
    await patchBazeliskIntoTerminalPath();
    const output = await executeInTerminalAndGetStdout(
      'bazel build //pw_status',
    );
    assert.strictEqual(
      output.indexOf('Build completed successfully') > 0,
      true,
    );
  });
});
