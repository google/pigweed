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

import { expect, assert } from '@open-wc/testing';
import { LocalStateStorage, StateService } from '../src/shared/state';
import { NodeType, Orientation, ViewNode } from '../src/shared/view-node';

describe('state', () => {
  const mockColumnData = [
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

  const mockState = {
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

  const stateService = new StateService(new LocalStateStorage());
  stateService.saveState(mockState);

  describe('state service', () => {
    it('loads a stored state object', () => {
      expect(stateService.loadState()).to.have.property('rootNode');
      assert.lengthOf(stateService.loadState().rootNode.children, 2);
    });
  });
});
