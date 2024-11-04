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

import commonjs from '@rollup/plugin-commonjs';
import resolve from '@rollup/plugin-node-resolve';
import pluginTypescript from '@rollup/plugin-typescript';
import path from 'path';
import sourceMaps from 'rollup-plugin-sourcemaps';

export default [
  // // Bundle proto collection script
  {
    input: path.join('pw_protobuf_compiler', 'ts', 'build_default_protos.ts'),
    output: [
      {
        file: path.join('dist', 'bin', 'build_default_protos.js'),
        format: 'cjs',
        banner: '#!/usr/bin/env node\n\nconst window = null;',
      },
    ],
    plugins: [
      pluginTypescript({
        tsconfig: './tsconfig.json',
        exclude: ['**/*_test.ts'],
      }),
      resolve(),
      commonjs(),

      // Resolve source maps to the original source
      sourceMaps(),
    ],
  },
  {
    input: path.join('pw_protobuf_compiler', 'ts', 'build_cli.ts'),
    output: [
      {
        file: path.join('dist', 'bin', 'pw_protobuf_compiler.js'),
        format: 'cjs',
        banner: '#!/usr/bin/env node\n\nconst window = null;',
      },
    ],
    plugins: [
      pluginTypescript({
        tsconfig: './tsconfig.json',
        exclude: ['**/*_test.ts'],
      }),
      resolve(),
      commonjs(),

      // Resolve source maps to the original source
      sourceMaps(),
    ],
  },
];
