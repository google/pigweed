// Copyright 2024 The Pigweed Authors
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
import { writeFileSync, mkdirSync } from 'fs';
import { join } from 'path';

import {
  findPigweedJsonAbove,
  findPigweedJsonBelow,
  inferPigweedProjectRoot,
} from './project';

import { makeTempDir } from './testUtils';

suite('find pigweed.json', () => {
  test('find pigweed.json 1 level above', async () => {
    const dir = await makeTempDir();
    writeFileSync(join(dir, 'pigweed.json'), '');
    const nestedDir = join(dir, 'nested');
    mkdirSync(nestedDir);
    const projectRoot = findPigweedJsonAbove(nestedDir);

    assert.strictEqual(dir, projectRoot);
  });

  test('find pigweed.json 2 levels above', async () => {
    const dir = await makeTempDir();
    writeFileSync(join(dir, 'pigweed.json'), '');
    const nestedDir = join(dir, 'nested');
    mkdirSync(nestedDir);
    const nestedDir2 = join(dir, 'nested', 'again');
    mkdirSync(nestedDir2);
    const projectRoot = findPigweedJsonAbove(nestedDir2);

    assert.strictEqual(dir, projectRoot);
  });

  test('find pigweed.json 1 level below', async () => {
    const dir = await makeTempDir();
    const nestedDir = join(dir, 'nested');
    mkdirSync(nestedDir);
    writeFileSync(join(nestedDir, 'pigweed.json'), '');
    const projectRoot = await findPigweedJsonBelow(dir);

    assert.strictEqual(nestedDir, projectRoot[0]);
  });

  test('find pigweed.json 2 levels below', async () => {
    const dir = await makeTempDir();
    const nestedDir = join(dir, 'nested');
    mkdirSync(nestedDir);
    const nestedDir2 = join(dir, 'nested', 'again');
    mkdirSync(nestedDir2);
    writeFileSync(join(nestedDir2, 'pigweed.json'), '');
    const projectRoot = await findPigweedJsonBelow(dir);

    assert.strictEqual(nestedDir2, projectRoot[0]);
  });

  test('find pigweed.json below returns all found', async () => {
    const dir = await makeTempDir();
    const nestedDir = join(dir, 'nested');
    mkdirSync(nestedDir);
    writeFileSync(join(nestedDir, 'pigweed.json'), '');
    const nestedDir2 = join(dir, 'nested', 'again');
    mkdirSync(nestedDir2);
    writeFileSync(join(nestedDir2, 'pigweed.json'), '');
    const projectRoot = await findPigweedJsonBelow(dir);

    assert.equal(2, projectRoot.length);
    assert.ok(projectRoot.includes(nestedDir));
    assert.ok(projectRoot.includes(nestedDir2));
  });

  test('pigweed.json from above is preferred over below', async () => {
    const dir = await makeTempDir();
    writeFileSync(join(dir, 'pigweed.json'), '');

    const workspaceDir = join(dir, 'workspace');
    mkdirSync(workspaceDir);

    const pigweedDir = join(workspaceDir, 'pigweed');
    mkdirSync(pigweedDir);
    writeFileSync(join(pigweedDir, 'pigweed.json'), '');

    const projectRoot = await inferPigweedProjectRoot(dir);

    assert.strictEqual(dir, projectRoot);
  });
});
