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

function setUpLogViewer() {
  const mockLogSource = new MockLogSource();
  const destroyLogViewer = createLogViewer(mockLogSource, document.body);
  const logViewer = document.querySelector('log-viewer');
  return { mockLogSource, destroyLogViewer, logViewer };
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
  let mockLogSource;
  let destroyLogViewer;
  let logViewer;
  let stateStore;

  beforeEach(() => {
    stateStore = new LocalStorageState();
    handleResizeObserverError();
  });

  afterEach(() => {
    mockLogSource.stop();
    destroyLogViewer();
  });

  describe('state', () => {
    it('should default to single view when state is cleared', async () => {
      window.localStorage.clear();
      ({ mockLogSource, destroyLogViewer, logViewer } = setUpLogViewer());
      const logViews = await getLogViews();
      assert.lengthOf(logViews, 1);
    });

    it('should populate correct number of views from state', async () => {
      const viewState = {
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
      const state = stateStore.getState();
      state.logViewConfig.push(viewState);
      stateStore.setState(state);

      ({ mockLogSource, destroyLogViewer, logViewer } = setUpLogViewer());
      const logViews = await getLogViews();
      assert.lengthOf(logViews, 2);
      window.localStorage.clear();
    });

    async function getLogViews() {
      const logViewerEl = document.querySelector('log-viewer');

      await logViewerEl.updateComplete;
      await new Promise((resolve) => setTimeout(resolve, 100));

      const logViews = logViewerEl.shadowRoot.querySelectorAll('log-view');
      return logViews;
    }
  });
});
