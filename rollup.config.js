// Copyright 2022 The Pigweed Authors
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
import nodePolyfills from 'rollup-plugin-node-polyfills';
import postcss from 'rollup-plugin-postcss';
import sourceMaps from 'rollup-plugin-sourcemaps';
import terser from '@rollup/plugin-terser';

export default [
  // Bundle proto collection into one UMD file for consumption from browser
  {
    input: path.join('dist', 'protos', 'collection.ts'),
    output: [
      {
        file: path.join('dist', 'proto_collection.umd.js'),
        format: 'umd',
        sourcemap: true,
        name: 'PigweedProtoCollection',
      },
    ],
    plugins: [
      pluginTypescript({ tsconfig: './tsconfig.json' }),
      commonjs(),
      resolve(),

      // Resolve source maps to the original source
      sourceMaps(),
    ],
  },
  // Bundle Pigweed log component and modules
  {
    input: path.join('ts', 'logging.ts'),
    output: [
      {
        file: path.join('dist', 'logging.umd.js'),
        format: 'umd',
        sourcemap: true,
        name: 'PigweedLogging',
        inlineDynamicImports: true,
      },
      {
        file: path.join('dist', 'logging.mjs'),
        format: 'esm',
        sourcemap: true,
        inlineDynamicImports: true,
      },
    ],
    plugins: [
      postcss({ plugins: [] }),
      pluginTypescript({
        tsconfig: './tsconfig.json',
        exclude: ['**/*_test.ts'],
      }),
      nodePolyfills(),
      resolve(),
      commonjs(),

      // Resolve source maps to the original source
      sourceMaps(),
    ],
  },
  // Bundle pw_console's web counterparts
  {
    input: path.join('ts', 'console.ts'),
    output: [
      {
        file: path.join('dist', 'pw_console.umd.js'),
        format: 'umd',
        sourcemap: true,
        name: 'PWConsole',
        inlineDynamicImports: true,
      },
      {
        file: path.join('dist', 'pw_console.mjs'),
        format: 'esm',
        sourcemap: true,
        inlineDynamicImports: true,
      },
    ],
    plugins: [
      postcss({ plugins: [] }),
      pluginTypescript({
        tsconfig: './tsconfig.json',
        exclude: ['**/*_test.ts'],
      }),
      nodePolyfills(),
      resolve(),
      commonjs(),

      // Resolve source maps to the original source
      sourceMaps(),

      // Minify builds
      terser(),
    ],
  },
  // Bundle Pigweed modules
  {
    input: path.join('ts', 'index.ts'),
    output: [
      {
        file: path.join('dist', 'index.umd.js'),
        format: 'umd',
        sourcemap: true,
        name: 'Pigweed',
      },
      {
        file: path.join('dist', 'index.mjs'),
        format: 'esm',
        sourcemap: true,
      },
    ],
    plugins: [
      pluginTypescript({
        tsconfig: './tsconfig.json',
        exclude: ['**/*_test.ts'],
      }),
      nodePolyfills(),
      resolve(),
      commonjs(),

      // Resolve source maps to the original source
      sourceMaps(),
    ],
  },
];
