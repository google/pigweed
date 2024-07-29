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

import { EventEmitter } from 'vscode';

import { Disposer } from './disposables';

import {
  OK,
  RefreshCallback,
  RefreshManager,
  RefreshStatus,
} from './refreshManager';

/** Event emitted on extension load. */
export const didInit = new EventEmitter<void>();

/** Event emitted when the code analysis target is changed. */
export const didChangeTarget = new EventEmitter<string>();

/** Event emitted when the active files cache has been updated. */
export const didUpdateActiveFilesCache = new EventEmitter<void>();

/** Event emitted whenever the `clangd` configuration is changed. */
export const didChangeClangdConfig = new EventEmitter<void>();

/** Event emitted when the refresh manager state changes. */
export const didChangeRefreshStatus = new EventEmitter<RefreshStatus>();

/**
 * Helper for subscribing to events.
 *
 * This lets you register the event callback and add the resulting handler to
 * a disposer in one call.
 */
export function subscribe<T>(
  event: EventEmitter<T>,
  cb: (data: T) => void,
  disposer: Disposer,
): void {
  disposer.add(event.event(cb));
}

/**
 * Helper for linking refresh manager state changes to VSC events.
 *
 * Given a refresh manager state, it returns a tuple containing a refresh
 * callback that fires the event, and the same status you provided. This just
 * makes it easier to spread those arguments to the refresh manager callback
 * register function.
 */
const fireDidChangeRefreshStatus = (
  status: RefreshStatus,
): [RefreshCallback, RefreshStatus] => {
  const cb: RefreshCallback = () => {
    didChangeRefreshStatus.fire(status);
    return OK;
  };

  return [cb, status];
};

/**
 * Link the refresh manager state machine to the VSC event system.
 *
 * The refresh manager needs to remain editor-agnostic so it can be used outside
 * of VSC. It also has a more constrained event system that isn't completely
 * represented by the VSC event system. This function links the two, such that
 * any refresh manager state changes also trigger a VSC event that can be
 * subscribed to by things that need to be notified or updated when the refresh
 * manager runs, but don't need to be integrated into the refresh manager
 * itself.
 */
export function linkRefreshManagerToEvents(
  refreshManager: RefreshManager<any>,
) {
  refreshManager.on(...fireDidChangeRefreshStatus('idle'));
  refreshManager.on(...fireDidChangeRefreshStatus('willRefresh'));
  refreshManager.on(...fireDidChangeRefreshStatus('refreshing'));
  refreshManager.on(...fireDidChangeRefreshStatus('didRefresh'));
  refreshManager.on(...fireDidChangeRefreshStatus('abort'));
  refreshManager.on(...fireDidChangeRefreshStatus('fault'));
}
