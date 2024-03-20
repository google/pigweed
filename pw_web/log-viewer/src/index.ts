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

import { JsonLogSource } from './custom/json-log-source';
import { BrowserLogSource } from './custom/browser-log-source';
import { createLogViewer } from './createLogViewer';
import { LocalStorageState } from './shared/state';
import { LogSource } from './log-source';
import { LogStore } from './log-store';

const logStore = new LogStore();
const logSources = [new BrowserLogSource(), new JsonLogSource()] as LogSource[];
const state = new LocalStorageState();

const containerEl = document.querySelector(
  '#log-viewer-container',
) as HTMLElement;

if (containerEl) {
  createLogViewer(logSources, containerEl, state, logStore);
}

// Start reading log data
logSources.forEach((logSource: LogSource) => {
  logSource.start();
});
