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

import { globIterate } from 'glob';

import * as vscode from 'vscode';
import { ExtensionContext } from 'vscode';

export type VscCommandCallback = (...args: any[]) => any;

/**
 * A much less verbose way to register VSC commands.
 */
export const commandRegisterer =
  (context: ExtensionContext) =>
  (command: string, callback: VscCommandCallback, thisArg?: any): void => {
    context.subscriptions.push(
      vscode.commands.registerCommand(command, callback, thisArg),
    );
  };

/** Return true if there exists at least one file matching the glob. */
export async function fileTypeExists(globString: string): Promise<boolean> {
  for await (const _ of globIterate(globString)) {
    return true;
  }

  return false;
}
