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

import { LitElement, html } from 'lit';
import { customElement, property, state } from 'lit/decorators.js';
import { repeat } from 'lit/directives/repeat.js';
import {
  TableColumn,
  LogEntry,
  LogViewConfig,
  State,
} from '../shared/interfaces';
import { StateStore } from '../shared/state';
import { styles } from './log-viewer.styles';
import { LogView } from './log-view/log-view';
import CloseViewEvent from '../events/close-view';

/**
 * The root component which renders one or more log views for displaying
 * structured log entries.
 *
 * @element log-viewer
 */
@customElement('log-viewer')
export class LogViewer extends LitElement {
  static styles = styles;

  /** An array of log entries to be displayed. */
  @property({ type: Array })
  logs: LogEntry[] = [];

  /** An array of rendered log view instances. */
  @state()
  _logViews: LogView[] = [];

  /** A StateStore object that stores state of views */
  @state()
  _stateStore: StateStore;

  @state()
  _state: State;

  constructor(state: StateStore) {
    super();
    this._stateStore = state;
    this._state = this._stateStore.getState();
  }

  protected firstUpdated(): void {
    if (this._state.logViewConfig.length == 0) {
      this.addLogView();
      return;
    }

    const viewState = this._state.logViewConfig;
    const viewEls = [];
    for (const i in viewState) {
      const view = new LogView();
      view.id = viewState[i].viewID;
      view.searchText = viewState[i].search;
      viewEls.push(view);
      this._logViews = viewEls;
    }
  }

  connectedCallback() {
    super.connectedCallback();
    this.addEventListener('close-view', this.handleCloseView);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.removeEventListener('close-view', this.handleCloseView);
  }

  /** Creates a new log view in the `_logViews` arrray. */
  private addLogView() {
    const newView = new LogView();
    const newViewState = this.addLogViewState(newView);
    const viewStates: State = { logViewConfig: this._state.logViewConfig };
    viewStates.logViewConfig.push(newViewState);
    this._logViews = [...this._logViews, newView];
    this._stateStore.setState(viewStates);
    this._state = viewStates;
  }

  /** Creates a new log view state to store in the state object. */
  private addLogViewState(view: LogView): LogViewConfig {
    const fieldColumns = [];
    const fields = view.getFields();

    for (const i in fields) {
      const col: TableColumn = {
        isVisible: true,
        fieldName: fields[i],
        characterLength: 0,
        manualWidth: null,
      };
      fieldColumns.push(col);
    }

    const obj = {
      columnData: fieldColumns,
      search: '',
      viewID: view.id,
      viewTitle: 'Log View',
    };

    return obj as LogViewConfig;
  }

  /**
   * Removes a log view when its Close button is clicked.
   *
   * @param event The event object dispatched by the log view controls.
   */
  private handleCloseView(event: CloseViewEvent) {
    const viewId = event.detail.viewId;
    const index = this._logViews.findIndex((view) => view.id === viewId);
    this._logViews = this._logViews.filter((view) => view.id !== viewId);

    if (index > -1) {
      this._state.logViewConfig.splice(index, 1);
      this._stateStore.setState(this._state);
    }
  }

  render() {
    return html`
      <div class="grid-container">
        ${repeat(
          this._logViews,
          (view) => view.id,
          (view) => html`
            <log-view
              id=${view.id}
              .logs=${[...this.logs]}
              .isOneOfMany=${this._logViews.length > 1}
              .stateStore=${this._stateStore}
              @add-view="${this.addLogView}"
            ></log-view>
          `,
        )}
      </div>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'log-viewer': LogViewer;
  }
}
