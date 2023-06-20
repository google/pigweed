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
import { customElement, property } from 'lit/decorators.js';
import { styles } from './log-viewer.styles';
import { LogView } from './log-view/log-view';
import { LogEntry } from '../shared/interfaces';

/**
 * Description of LogViewer.
 */
@customElement('log-viewer')
export class LogViewer extends LitElement {
    static styles = styles;

    /**
     * Description of logs.
     */
    @property({ type: Array })
    logs: LogEntry[];

    /**
     * Description of logViews.
     */
    @property({ type: Array })
    logViews: LogView[];

    constructor() {
        super();
        this.logs = [];
        this.logViews = [new LogView()];
    }

    connectedCallback(): void {
        super.connectedCallback();
        this.addEventListener('close-view', this.handleCloseView);
    }

    disconnectedCallback() {
        super.disconnectedCallback();
        this.removeEventListener('close-view', this.handleCloseView);
    }

    render() {
        const logsCopy = [...this.logs]; // Trigger an update in <log-view>
        const hideCloseButton = this.logViews.length <= 1;

        return html`
            <md-outlined-button
                class="add-button"
                @click="${this.addLogView}"
                title="Add a view"
            >
                Add View
            </md-outlined-button>

            <div class="grid-container">
                ${this.logViews.map(
                    (view) =>
                        html`
                            <log-view
                                id=${view.id}
                                .logs=${logsCopy}
                                .hideCloseButton=${hideCloseButton}
                            ></log-view>
                        `
                )}
            </div>
        `;
    }

    addLogView() {
        this.logViews = [...this.logViews, new LogView()];
    }

    handleCloseView(event: Event) {
        const viewId = (event as CustomEvent).detail.viewId;
        this.logViews = this.logViews.filter((view) => view.id !== viewId);
    }
}

declare global {
    interface HTMLElementTagNameMap {
        'log-viewer': LogViewer;
    }
}
