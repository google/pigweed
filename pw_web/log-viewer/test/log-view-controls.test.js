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

import { MockLogSource } from '../src/custom/mock-log-source';
import { createLogViewer } from '../src/createLogViewer';
import { expect } from '@open-wc/testing';
import { LocalStateStorage, StateService } from '../src/shared/state';
import { NodeType, Orientation, ViewNode } from '../src/shared/view-node';

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

describe('log-view-controls', () => {
  let mockLogSource;
  let destroyLogViewer;
  let logViewer;
  let mockColumnData;
  let mockState;
  let stateService;

  before(() => {
    mockColumnData = [
      {
        fieldName: 'test',
        characterLength: 0,
        manualWidth: null,
        isVisible: false,
      },
      {
        fieldName: 'foo',
        characterLength: 0,
        manualWidth: null,
        isVisible: true,
      },
      {
        fieldName: 'bar',
        characterLength: 0,
        manualWidth: null,
        isVisible: false,
      },
    ];
    mockState = {
      rootNode: new ViewNode({
        type: NodeType.Split,
        orientation: Orientation.Horizontal,
        children: [
          new ViewNode({
            searchText: 'hello',
            logViewId: 'child-node-1',
            type: NodeType.View,
            columnData: mockColumnData,
          }),
          new ViewNode({
            searchText: 'world',
            logViewId: 'child-node-2',
            type: NodeType.View,
            columnData: mockColumnData,
          }),
        ],
      }),
    };

    stateService = new StateService(new LocalStateStorage());
    stateService.saveState(mockState);
    handleResizeObserverError();
  });

  after(() => {
    window.localStorage.clear();
    mockLogSource.stop();
    destroyLogViewer();
  });

  describe('state', () => {
    it('should populate search field value on component load', async () => {
      ({ mockLogSource, destroyLogViewer, logViewer } = setUpLogViewer());
      const logViews = await getLogViews();
      const stateSearchString =
        mockState.rootNode.children[0].logViewState.searchText;
      const logControls =
        logViews[0].shadowRoot.querySelector('log-view-controls');
      logControls.requestUpdate();
      const inputEl = logControls.shadowRoot.querySelector('.search-field');

      await logViewer.updateComplete;
      expect(inputEl.value).to.equal(stateSearchString);
    });

    it('should populate search field values for multiple log views on component load', async () => {
      ({ mockLogSource, destroyLogViewer, logViewer } = setUpLogViewer());

      const state = stateService.loadState();
      const logViews = await getLogViews();
      const searchInputs = [];

      logViews.forEach((logView) => {
        const logControls =
          logView.shadowRoot.querySelector('log-view-controls');
        const inputEl = logControls.shadowRoot.querySelector('.search-field');
        searchInputs.push(inputEl.value);
      });

      state.rootNode.children.forEach((childNode, index) => {
        expect(childNode.logViewState.searchText).to.equal(searchInputs[index]);
      });
    });

    it('should recall table column visibility on component load', async () => {
      ({ mockLogSource, destroyLogViewer, logViewer } = setUpLogViewer());

      const state = stateService.loadState();
      const logViews = await getLogViews();
      const logControls =
        logViews[0].shadowRoot.querySelector('log-view-controls');
      const fieldMenu =
        logControls.shadowRoot.querySelectorAll('.item-checkboxes');

      fieldMenu.forEach((field, index) => {
        const columnData = state.rootNode.children[0].logViewState.columnData;
        expect(field.checked).to.equal(columnData[index].isVisible);
      });
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
