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
import {
  createBazelInterceptorFile,
  deleteBazelInterceptorFile,
  getBazelInterceptorPath,
} from './clangd/compileCommandsUtils';
import { existsSync, writeFileSync, readFileSync, unlinkSync } from 'fs';
import * as path from 'path';
import { deactivate as callExtensionDeactivate } from './extension';
import { workingDir } from './settings/vscode';

suite('Extension Test Suite', () => {
  vscode.window.showInformationMessage('Starting tests.');
  // TODO: b/449001410 - Re-enable these when we support Windows.
  if (process.platform !== 'win32') {
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
        'bazelisk build //pw_status',
      );
      assert.strictEqual(
        output.indexOf('Build completed successfully') > 0,
        true,
      );
    });
  }

  test('Bazel Interceptor cleanup on deactivate', async () => {
    const interceptorPath = getBazelInterceptorPath();
    if (!interceptorPath) {
      assert.fail('Could not get Bazel interceptor path. Test setup issue.');
    }

    // Ensure a clean state: delete if it exists from a previous failed run
    if (existsSync(interceptorPath)) {
      await deleteBazelInterceptorFile();
    }
    assert.strictEqual(
      existsSync(interceptorPath),
      false,
      'Interceptor file should not exist initially',
    );

    // 1. Create the interceptor file
    await createBazelInterceptorFile();
    assert.strictEqual(
      existsSync(interceptorPath),
      true,
      'Interceptor file should be created',
    );

    // 2. Call the extension's deactivate function
    await callExtensionDeactivate();

    // 3. Assert the file is deleted
    assert.strictEqual(
      existsSync(interceptorPath),
      false,
      'Interceptor file should be deleted after deactivate',
    );
  });

  suite('Clangd file cleanup on deactivate', () => {
    const workspaceRoot = workingDir.get();
    const clangdFilePath = path.join(workspaceRoot, '.clangd');
    const clangdSharedFilePath = path.join(workspaceRoot, '.clangd.shared');

    const managedContent = '# This is a .clangd file managed by Pigweed ext';
    const sharedContent = '# This is .clangd.shared content';

    setup(async () => {
      // Clean up before each test
      if (existsSync(clangdFilePath)) {
        unlinkSync(clangdFilePath);
      }
      if (existsSync(clangdSharedFilePath)) {
        unlinkSync(clangdSharedFilePath);
      }
    });

    teardown(async () => {
      // Ensure cleanup after each test
      if (existsSync(clangdFilePath)) {
        unlinkSync(clangdFilePath);
      }
      if (existsSync(clangdSharedFilePath)) {
        unlinkSync(clangdSharedFilePath);
      }
    });

    test('.clangd should be restored from .clangd.shared if it exists', async () => {
      // 1. Setup: Create .clangd (managed) and .clangd.shared
      writeFileSync(clangdFilePath, managedContent);
      writeFileSync(clangdSharedFilePath, sharedContent);

      assert.ok(
        existsSync(clangdFilePath),
        '.clangd should exist before deactivate',
      );
      assert.strictEqual(
        readFileSync(clangdFilePath, 'utf-8'),
        managedContent,
        '.clangd initial content mismatch',
      );
      assert.ok(
        existsSync(clangdSharedFilePath),
        '.clangd.shared should exist before deactivate',
      );

      // 2. Action: Call deactivate
      await callExtensionDeactivate();

      // 3. Assert: .clangd should exist and match .clangd.shared
      assert.ok(
        existsSync(clangdFilePath),
        '.clangd should still exist after deactivate',
      );
      assert.strictEqual(
        readFileSync(clangdFilePath, 'utf-8'),
        sharedContent,
        '.clangd content should match .clangd.shared after deactivate',
      );
    });

    test('.clangd should be deleted if .clangd.shared does not exist', async () => {
      // 1. Setup: Create only .clangd (managed)
      writeFileSync(clangdFilePath, managedContent);
      assert.ok(
        existsSync(clangdFilePath),
        '.clangd should exist before deactivate',
      );
      assert.strictEqual(
        readFileSync(clangdFilePath, 'utf-8'),
        managedContent,
        '.clangd initial content mismatch',
      );
      assert.ok(
        !existsSync(clangdSharedFilePath),
        '.clangd.shared should not exist',
      );

      // 2. Action: Call deactivate
      await callExtensionDeactivate();

      // 3. Assert: .clangd should be deleted
      assert.ok(
        !existsSync(clangdFilePath),
        '.clangd should be deleted after deactivate',
      );
    });

    test('.clangd should be created from .clangd.shared if .clangd did not exist', async () => {
      // 1. Setup: Create only .clangd.shared
      writeFileSync(clangdSharedFilePath, sharedContent);
      assert.ok(
        !existsSync(clangdFilePath),
        '.clangd should not exist before deactivate',
      );
      assert.ok(
        existsSync(clangdSharedFilePath),
        '.clangd.shared should exist',
      );

      // 2. Action: Call deactivate
      await callExtensionDeactivate();

      // 3. Assert: .clangd should be created and match .clangd.shared
      assert.ok(
        existsSync(clangdFilePath),
        '.clangd should be created after deactivate',
      );
      assert.strictEqual(
        readFileSync(clangdFilePath, 'utf-8'),
        sharedContent,
        '.clangd content should match .clangd.shared after deactivate',
      );
    });

    test('deactivate should do nothing if neither .clangd nor .clangd.shared exist', async () => {
      // 1. Setup: No .clangd files
      assert.ok(!existsSync(clangdFilePath), '.clangd should not exist');
      assert.ok(
        !existsSync(clangdSharedFilePath),
        '.clangd.shared should not exist',
      );

      // 2. Action: Call deactivate
      await callExtensionDeactivate();

      // 3. Assert: Still no .clangd files
      assert.ok(!existsSync(clangdFilePath), '.clangd should still not exist');
      assert.ok(
        !existsSync(clangdSharedFilePath),
        '.clangd.shared should still not exist',
      );
    });
  });
});
