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

import { State } from './interfaces';

/**
 * Abstract Class for StateStore.
 */
export abstract class StateStore {
  abstract getState(): State;
  abstract setState(state: State): void;
}

/**
 * LocalStorage version of StateStore
 */
export class LocalStorageState extends StateStore {
  getState(): State {
    try {
      const state = localStorage.getItem('logState') as string;
      return state == null ? { logViewConfig: [] } : JSON.parse(state);
    } catch (e) {
      console.error(e);
      return { logViewConfig: [] };
    }
  }

  setState(state: State): void {
    localStorage.setItem('logState', JSON.stringify(state));
  }
}
