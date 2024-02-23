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

import { assert } from '@open-wc/testing';
import { MockLogSource } from '../src/custom/mock-log-source';
import { createLogViewer } from '../src/createLogViewer';
import { LocalStorageState } from '../src/shared/state';
import { LogStore } from '../src/log-store';

function setUpLogViewer(logSources) {
  const logStore = new LogStore();
  const destroyLogViewer = createLogViewer(
    document.body,
    undefined,
    logStore,
    ...logSources,
  );
  const logViewer = document.querySelector('log-viewer');

  handleResizeObserverError();
  return { logSources, destroyLogViewer, logViewer, logStore };
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

describe('log-view', () => {
  let logSources;
  let logStore;
  let destroyLogViewer;
  let logViewer;
  let stateStore;

  async function getLogViews() {
    const logViewerEl = document.querySelector('log-viewer');

    await logViewerEl.updateComplete;
    await new Promise((resolve) => setTimeout(resolve, 100));

    const logViews = logViewerEl.shadowRoot.querySelectorAll('log-view');
    return logViews;
  }

  describe('state', () => {
    beforeEach(() => {
      window.localStorage.clear();
      ({ logSources, destroyLogViewer, logViewer, logStore } = setUpLogViewer([
        new MockLogSource(),
      ]));
      stateStore = new LocalStorageState();
    });

    afterEach(() => {
      destroyLogViewer();
      window.localStorage.clear();
    });

    it('should default to single view when state is cleared', async () => {
      const logViews = await getLogViews();
      assert.lengthOf(logViews, 1);
    });

    it('should populate correct number of views from state', async () => {
      // Destroy log viewer created by beforeEach()
      destroyLogViewer();

      const viewState1 = {
        columnData: [
          {
            fieldName: 'test',
            characterLength: 0,
            manualWidth: null,
            isVisible: false,
          },
        ],
        search: '',
        viewID: 'abc',
        viewTitle: 'Log View',
      };
      const viewState2 = structuredClone(viewState1);
      viewState2.viewID = 'def';

      const state = stateStore.getState();
      state.logViewConfig = [viewState1, viewState2];
      stateStore.setState(state);

      // Create a new log viewer with an existing state
      ({ logSources, destroyLogViewer, logViewer, logStore } = setUpLogViewer([
        new MockLogSource(),
      ]));
      const logViews = await getLogViews();
      assert.lengthOf(logViews, 2);
      window.localStorage.clear();
    });
  });

  describe('sources', () => {
    before(() => {
      window.localStorage.clear();
      ({ logSources, destroyLogViewer, logViewer, logStore } = setUpLogViewer([
        new MockLogSource('Source 1'),
        new MockLogSource('Source 2'),
      ]));
      stateStore = new LocalStorageState();
    });

    after(() => {
      destroyLogViewer();
      window.localStorage.clear();
    });

    it('registers a new source upon receiving its first log entry', async () => {
      const logSource1 = logSources[0];

      logSource1.publishLogEntry({
        timestamp: new Date(),
        fields: [{ key: 'message', value: 'Message from Source 1' }],
      });

      await logViewer.updateComplete;
      await new Promise((resolve) => setTimeout(resolve, 100));

      const logViews = await getLogViews();
      const sources = logViews[0]?.sources;
      const sourceNames = Array.from(sources.values()).map(
        (source) => source.name,
      );

      assert.include(
        sourceNames,
        'Source 1',
        'New source should be registered after emitting its first log entry',
      );
    });

    it('keeps a record of multiple log sources', async () => {
      const logSource2 = logSources[1];

      logSource2.publishLogEntry({
        timestamp: new Date(),
        fields: [{ key: 'message', value: 'Message from Source 2' }],
      });

      await logViewer.updateComplete;
      await new Promise((resolve) => setTimeout(resolve, 100));

      const logViews = await getLogViews();
      const sources = logViews[0]?.sources;
      const sourceNames = Array.from(sources.values()).map(
        (source) => source.name,
      );

      assert.includeMembers(
        sourceNames,
        ['Source 1', 'Source 2'],
        'Both sources should be present',
      );
    });
  });
});
