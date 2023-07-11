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
import { styles } from './log-viewer.styles';
import { LogView } from './log-view/log-view';
import { LogEntry } from '../shared/interfaces';
import CloseViewEvent from '../events/close-view';
import { repeat } from 'lit/directives/repeat.js';

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
    _logViews: LogView[] = [new LogView()];

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
        this._logViews = [...this._logViews, new LogView()];
    }

    /**
     * Removes a log view when its Close button is clicked.
     *
     * @param event The event object dispatched by the log view controls.
     */
    private handleCloseView(event: CloseViewEvent) {
        const viewId = event.detail.viewId;
        this._logViews = this._logViews.filter((view) => view.id !== viewId);
    }

    render() {
        return html`
            <md-outlined-button
                class="add-button"
                @click="${this.addLogView}"
                title="Add a view"
            >
                Add View
            </md-outlined-button>

            <div class="grid-container">
                ${repeat(
                    this._logViews,
                    (view) => view.id,
                    (view) => html`
                        <log-view
                            id=${view.id}
                            .logs=${[...this.logs]}
                            .isOneOfMany=${this._logViews.length > 1}
                        ></log-view>
                    `
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
