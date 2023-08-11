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

import { LogViewer as RootComponent } from './components/log-viewer';
import { StateStore, LocalStorageState } from './shared/state';
import { LogEntry } from '../src/shared/interfaces';
import { LogSource } from '../src/log-source';

import '@material/web/button/filled-button.js';
import '@material/web/button/outlined-button.js';
import '@material/web/checkbox/checkbox.js';
import '@material/web/field/outlined-field.js';
import '@material/web/textfield/outlined-text-field.js';
import '@material/web/textfield/filled-text-field.js';
import '@material/web/iconbutton/standard-icon-button.js';
import '@material/web/icon/icon.js';

export function createLogViewer(
  logSource: LogSource,
  root: HTMLElement,
  state: StateStore = new LocalStorageState(),
) {
  const logViewer = new RootComponent(state);
  const logs: LogEntry[] = [];
  root.appendChild(logViewer);

  // Define an event listener for the 'logEntry' event
  const logEntryListener = (logEntry: LogEntry) => {
    logs.push(logEntry);
    logViewer.logs = logs;
    logViewer.requestUpdate('logs', []);
  };

  // Add the event listener to the LogSource instance
  logSource.addEventListener('logEntry', logEntryListener);

  // Method to destroy and unsubscribe
  return () => {
    logSource.removeEventListener('logEntry', logEntryListener);
  };
}
