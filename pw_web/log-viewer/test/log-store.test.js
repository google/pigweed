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

import { expect } from '@open-wc/testing';
import { createLogViewer } from '../src/createLogViewer';
import { MockLogSource } from '../src/custom/mock-log-source';
import { LogStore } from '../src/log-store';

// Initialize the log viewer component with a mock log source
function setUpLogViewer() {
  const mockLogSource = new MockLogSource();
  const logStore = new LogStore();
  const destroyLogViewer = createLogViewer(
    mockLogSource,
    document.body,
    undefined,
    logStore,
  );
  const logViewer = document.querySelector('log-viewer');
  return { mockLogSource, destroyLogViewer, logViewer, logStore };
}

// Handle benign ResizeObserver error caused by custom log viewer initialization
// See: https://developer.mozilla.org/en-US/docs/Web/API/ResizeObserver#observation_errors
function handleResizeObserverError() {
  const e = window.onerror;
  window.onerror = function (err) {
    if (
      err === 'ResizeObserver loop completed with undelivered notifications.'
    ) {
      console.warn(
        'Ignored: ResizeObserver loop completed with undelivered notifications.',
      );
      return false;
    } else {
      return e(...arguments);
    }
  };
}

describe('log-store', () => {
  let mockLogSource;
  let destroyLogViewer;
  let logViewer;
  let logStore;

  beforeEach(() => {
    window.localStorage.clear();
    ({ mockLogSource, destroyLogViewer, logViewer, logStore } =
      setUpLogViewer());
    handleResizeObserverError();
  });

  afterEach(() => {
    mockLogSource.stop();
    destroyLogViewer();
  });

  it('should maintain log entries emitted', async () => {
    const logEntry = mockLogSource.readLogEntryFromHost();
    mockLogSource.publishLogEntry(logEntry);
    const logs = logStore.getLogs();

    expect(logs.length).equal(1);
    expect(logs[0]).equal(logEntry);
  });
});
