// Copyright 2023 The Pigweed Authors
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

import * as hjson from 'hjson';
import * as vscode from 'vscode';

/**
 * Schema for extensions.json
 */
export interface ExtensionsJson {
  recommendations?: string[];
  unwantedRecommendations?: string[];
}

/**
 * Partial schema for the workspace config file
 */
interface WorkspaceConfig {
  extensions?: ExtensionsJson;
}

// When the project is opened directly (i.e., by opening the repo directory),
// we have direct access to extensions.json. But if the project is part of a
// workspace (https://code.visualstudio.com/docs/editor/workspaces), we'll get
// a combined config that includes the equivalent of extensions.json associated
// with the "extensions" key. This is taken into consideration only for the sake
// of completeness; Pigweed doesn't currently support the use of workspaces.
type LoadableConfig = ExtensionsJson & WorkspaceConfig;

/**
 * Load a config file that contains extensions.json data. This could be
 * extensions.json itself, or a workspace file that contains the equivalent.
 * @param uri - A file path to load
 * @returns - The extensions.json file data
 */
export async function loadExtensionsJson(
  uri: vscode.Uri,
): Promise<ExtensionsJson> {
  const buffer = await vscode.workspace.fs.readFile(uri);
  const config: LoadableConfig = hjson.parse(buffer.toString());

  if (config.extensions) {
    return config.extensions;
  }

  return config as ExtensionsJson;
}

/**
 * Find and return the extensions.json data for the project.
 * @param includeWorkspace - Also search workspace files
 * @returns The extensions.json file data
 */
export async function getExtensionsJson(
  includeWorkspace = false,
): Promise<ExtensionsJson> {
  const files = await vscode.workspace.findFiles(
    '.vscode/extensions.json',
    '**/node_modules/**',
  );

  if (includeWorkspace) {
    const workspaceFile = vscode.workspace.workspaceFile;

    if (workspaceFile) {
      files.push(workspaceFile);
    }
  }

  if (files.length == 0) {
    // TODO(chadnorvell): Improve this
    vscode.window.showErrorMessage('extensions.json is missing!');
    throw new Error('extensions.json is missing!');
  } else {
    if (files.length > 1) {
      vscode.window.showWarningMessage(
        'Found multiple extensions.json! Will only use the first.',
      );
    }

    return await loadExtensionsJson(files[0]);
  }
}
