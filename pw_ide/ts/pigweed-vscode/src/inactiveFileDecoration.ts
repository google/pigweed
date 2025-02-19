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

import * as vscode from 'vscode';

import {
  EventEmitter,
  FileDecoration,
  FileDecorationProvider,
  ThemeColor,
  Uri,
} from 'vscode';

import { ClangdActiveFilesCache, FileStatus } from './clangd';
import { Disposable } from './disposables';

import {
  didChangeClangdConfig,
  didChangeTarget,
  didUpdateActiveFilesCache,
} from './events';

import logger from './logging';
import { getPigweedProjectRoot } from './project';
import { settings, workingDir } from './settings/vscode';

type DecorationsMap = Map<string, FileDecoration | undefined>;

/** Event emitted when the file decorations have changed. */
const didChangeFileDecorations = new EventEmitter<Uri[]>();

const makeDecoration = (
  target?: string,
): Record<FileStatus, FileDecoration | undefined> => ({
  ACTIVE: undefined,
  INACTIVE: {
    badge: 'X',
    color: new ThemeColor('disabledForeground'),
    tooltip: `This file is not built in the ${
      target ? `"${target}"` : 'current'
    } target group.`,
  },
  ORPHANED: {
    badge: '!!',
    color: new ThemeColor('errorForeground'),
    tooltip:
      'This file is not built by any defined target groups, ' +
      'so no code intelligence can be provided for it. ' +
      'You can fix this by adding a target that includes this file to a ' +
      'target group in the "refresh_compile_commands" invocation in the ' +
      'top-level BUILD.bazel file.',
  },
});

export class InactiveFileDecorationProvider
  extends Disposable
  implements FileDecorationProvider
{
  private decorations: DecorationsMap;
  private activeFilesCache: ClangdActiveFilesCache;

  constructor(activeFilesCache: ClangdActiveFilesCache) {
    super();
    this.activeFilesCache = activeFilesCache;
    this.decorations = new Map();

    this.disposables.push(
      ...[
        vscode.window.registerFileDecorationProvider(this),
        didChangeTarget.event(this.update),
        didUpdateActiveFilesCache.event(() => this.update()),
      ],
    );

    if (!settings.hideInactiveFileIndicators()) {
      this.disposables.push(didChangeClangdConfig.event(() => this.update()));
    }
  }

  // When this event is triggered, VSC is notified that the files included in
  // the event have had their decorations changed.
  readonly onDidChangeFileDecorations = didChangeFileDecorations.event;

  // This is called internally by VSC to provide the decoration for a file
  // whenever it's asked for.
  provideFileDecoration(uri: Uri) {
    return this.decorations.get(uri.toString());
  }

  /** Provide updated decorations for the current target's active files. */
  async refreshDecorations(providedTarget?: string): Promise<DecorationsMap> {
    const newDecorations: DecorationsMap = new Map();
    const target = providedTarget ?? settings.codeAnalysisTarget();

    if (!target) {
      return newDecorations;
    }

    const projectRoot = await getPigweedProjectRoot(settings, workingDir);

    if (!projectRoot) {
      return newDecorations;
    }

    const workspaceFiles = await vscode.workspace.findFiles(
      '**/*.{c,cc,cpp,h,hpp}', // include
      '**/{.*,bazel*,external}/**', // exclude
    );

    for (const uri of workspaceFiles) {
      const { status } = await this.activeFilesCache.fileStatus(
        projectRoot,
        target,
        uri,
      );

      const decoration = makeDecoration(providedTarget)[status];
      newDecorations.set(uri.toString(), decoration);
    }

    return newDecorations;
  }

  /** Update file decorations per the current target's active files. */
  update = async (target?: string) => {
    logger.info('Updating inactive file indicators');
    const newDecorations = await this.refreshDecorations(target);

    const updatedUris = new Set([
      ...this.decorations.keys(),
      ...newDecorations.keys(),
    ]);

    // This alone may not have any effect until VSC decides it needs to request
    // new decorations for a file.
    this.decorations = newDecorations;

    // Firing this event notifies VSC that all of these files' decorations have
    // changed.
    didChangeFileDecorations.fire(
      [...updatedUris.values()].map((it) => Uri.parse(it, true)),
    );
  };
}
