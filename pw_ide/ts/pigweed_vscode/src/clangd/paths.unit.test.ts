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
import * as path from 'path';
import { Target, CDB_FILE_DIRS, CDB_FILE_NAME } from './paths';

test('should use base name for targets in canonical directory', () => {
  const target = new Target('host_clang');

  assert.equal(target.name, 'host_clang');
  assert.equal(target.displayName, 'host_clang');
  assert.equal(path.dirname(target.dir), CDB_FILE_DIRS[0]);
  assert.equal(
    target.path,
    path.join(CDB_FILE_DIRS[0], 'host_clang', CDB_FILE_NAME),
  );
});

test('should append directory name for targets in non-canonical directory', () => {
  const target = new Target(
    'host_clang',
    path.join(CDB_FILE_DIRS[1], 'host_clang'),
  );

  assert.equal(target.name, 'host_clang');
  assert.equal(target.displayName, 'host_clang (.pw_ide)');
  assert.equal(path.dirname(target.dir), CDB_FILE_DIRS[1]);
  assert.equal(
    target.path,
    path.join(CDB_FILE_DIRS[1], 'host_clang', CDB_FILE_NAME),
  );
});
