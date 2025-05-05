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

import commonjs from '@rollup/plugin-commonjs';
import resolve from '@rollup/plugin-node-resolve';
import pluginTypescript from '@rollup/plugin-typescript';
import path from 'path';
import sourceMaps from 'rollup-plugin-sourcemaps';
import fs from 'fs';

function copy(fromFile, toFile) {
  return {
    name: 'copy_files',
    generateBundle() {
      if (!fs.existsSync(path.dirname(toFile))) {
        fs.mkdirSync(path.dirname(toFile), { recursive: true });
      }
      fs.copyFileSync(path.resolve(fromFile), path.resolve(toFile));
    },
  };
}

export default [
  {
    input: path.join('src', 'clangd', 'compileCommandsGenerator.ts'),
    output: [
      {
        file: path.join('out', 'bin', 'compileCommandsGenerator.js'),
        format: 'cjs',
        banner: '#!/usr/bin/env node\n\nconst window = null;',
      },
    ],
    plugins: [
      pluginTypescript({
        module: 'es6',
      }),
      resolve(),
      commonjs(),
      // Resolve source maps to the original source
      sourceMaps(),
      copy(
        path.join('scripts', 'cquery.bzl'),
        path.join('out', 'scripts', 'cquery.bzl'),
      ),
    ],
  },
];
