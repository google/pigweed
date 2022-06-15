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

import resolve from '@rollup/plugin-node-resolve';
import pluginTypescript from '@rollup/plugin-typescript';
import builtins from 'builtin-modules';
import path from 'path';
import dts from 'rollup-plugin-dts';
import sourceMaps from 'rollup-plugin-sourcemaps';

const modules = ['pw_status'];

const rollupConfig = modules.map((module) => {
  const modInputPath = path.join(module, 'ts');
  const modOutputPath = path.join('dist', module);
  return {
    input: path.join(modInputPath, 'index.ts'), external: builtins,
        output:
            [
              {
                file: path.join(modOutputPath, 'index.umd.js'),
                format: 'umd',
                sourcemap: true,
                name: module || 'Pigweed'
              },
              {
                file: path.join(modOutputPath, 'index.mjs'),
                format: 'esm',
                sourcemap: true,
              }
            ],
        plugins: [
          pluginTypescript(
              {tsconfig: './tsconfig.rollup.json', esModuleInterop: true}),
          // Allow node_modules resolution, so you can use 'external' to control
          // which external modules to include in the bundle
          // https://github.com/rollup/rollup-plugin-node-resolve#usage
          resolve({browser: true, preferBuiltins: false}),

          // Resolve source maps to the original source
          sourceMaps()
        ]
  }
})

const rollupTypesConfig = modules.map((module) => {
  const modInputPath = path.join('dist', module, 'types', module, 'ts');
  const modOutputPath = path.join('dist', module);
  return {
    input: path.join(modInputPath, 'index.d.ts'),
        output: [{file: path.join(modOutputPath, 'index.d.ts'), format: 'es'}],
        plugins: [dts()]
  }
})

export default rollupConfig.concat(rollupTypesConfig);
