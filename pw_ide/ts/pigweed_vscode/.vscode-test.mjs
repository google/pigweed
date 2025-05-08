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

import { defineConfig } from '@vscode/test-cli';
import path from 'path';

export default defineConfig([
  {
    label: 'unitTests',
    files: 'out/**/*.unit.test.js',
    workspaceFolder: path.join(process.cwd(), '..', '..', '..'),
    mocha: {
      ui: 'tdd',
      timeout: 120000,
    },
  },
  {
    label: 'e2eTests',
    files: 'out/**/*.e2e.test.js',
    workspaceFolder: path.join(process.cwd(), '..', '..', '..'),
    mocha: {
      timeout: 60000,
    },
    launchArgs: ['--install-extension', 'BazelBuild.vscode-bazel'],
  },
]);
