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

import { assert, expect } from '@open-wc/testing';
import { LocalStorageState, StateStore } from '../src/shared/state';

describe('state', () => {
  const stateStore = new LocalStorageState();
  const state = stateStore.getState();
  const viewState = {
    columnData: [
      {
        fieldName: 'test',
        characterLength: 0,
        manualWidth: null,
        isVisible: true,
      },
    ],
    search: '',
    viewID: 'abc',
    viewTitle: 'Log View',
  };

  it('should be empty when initialized', () => {
    expect(state).to.have.property('logViewConfig');
    assert.lengthOf(state.logViewConfig, 0, 'logViewConfig should be empty');
  });

  it('when the state is set, the state from the getter should match', () => {
    state.logViewConfig.push(viewState);
    stateStore.setState(state);
    const storedState = stateStore.getState();
    expect(storedState.logViewConfig[1]).to.equal(state.logViewConfig[1]);
  });
});
