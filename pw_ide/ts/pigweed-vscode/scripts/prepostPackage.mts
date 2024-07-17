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

import { existsSync as fsExists } from 'fs';
import * as fs from 'fs/promises';
import * as path from 'path';
import * as process from 'process';
import { findPigweedJsonAbove } from '../src/project.js';

/** Join paths and resolve them in one shot. */
const joinResolve = (...paths: string[]) => path.resolve(path.join(...paths));

/** Get the one argument that can be provided to the script. */
const getArg = () => process.argv.at(2);

async function prepostPackage() {
  const pwRoot = findPigweedJsonAbove(process.cwd());
  if (!pwRoot) throw new Error('Could not find Pigweed root!');

  const extRoot = path.join(pwRoot, 'pw_ide', 'ts', 'pigweed-vscode');
  if (!fsExists(extRoot)) throw new Error('Could not find extension root!');

  // A license file must be bundled with the extension.
  // Instead of redundantly storing it in this dir, we just temporarily copy
  // it into this dir when building, then remove it.
  const licensePath = joinResolve(pwRoot, 'LICENSE');
  const tempLicensePath = joinResolve(extRoot, 'LICENSE');

  // We don't store the icon in the source tree.
  // We expect to find it in the parent dir of the Pigweed repo dir, and like
  // the license file, we temporarily copy it for bundling then remove it.
  const iconPath = joinResolve(path.dirname(pwRoot), 'icon.png');
  const tempIconPath = joinResolve(extRoot, 'icon.png');

  const cleanup = async () => {
    try {
      await fs.unlink(tempLicensePath);
      await fs.unlink(tempIconPath);
    } catch (err: unknown) {
      const { code } = err as { code: string };
      if (code !== 'ENOENT') throw err;
    }
  };

  const arg = getArg();

  if (!arg || arg === '--pre') {
    try {
      await fs.copyFile(licensePath, tempLicensePath);
      await fs.copyFile(iconPath, tempIconPath);
    } catch (err: unknown) {
      await cleanup();
      throw err;
    }

    return;
  }

  if (arg === '--post') {
    await cleanup();
    return;
  }

  console.error(`Unrecognized argument: ${arg}`);
}

await prepostPackage();
