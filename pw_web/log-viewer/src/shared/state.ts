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

import { TableColumn } from './interfaces';
import { ViewNode } from './view-node';

export interface LogViewerState {
  rootNode: ViewNode;
}

export interface LogViewState {
  searchText?: string;
  columnData?: TableColumn[];
  viewTitle?: string;
  wordWrap?: boolean;
}

interface StateStorage {
  loadState(): LogViewerState | undefined;
  saveState(state: LogViewerState): void;
}

export class LocalStateStorage implements StateStorage {
  private readonly STATE_STORAGE_KEY = 'logViewerState';

  loadState(): LogViewerState | undefined {
    const storedStateString = localStorage.getItem(this.STATE_STORAGE_KEY);
    if (storedStateString) {
      return JSON.parse(storedStateString);
    }
    return undefined;
  }

  saveState(state: LogViewerState) {
    localStorage.setItem(this.STATE_STORAGE_KEY, JSON.stringify(state));
  }
}

export class StateService {
  private store: StateStorage;

  constructor(store: StateStorage) {
    this.store = store;
  }

  loadState(): LogViewerState | undefined {
    return this.store.loadState();
  }

  saveState(state: LogViewerState) {
    this.store.saveState(state);
  }
}
