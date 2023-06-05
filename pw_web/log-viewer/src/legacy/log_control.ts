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

import { css, html, LitElement } from "lit";
import { customElement, property, state } from "lit/decorators.js";
import { SEARCH_PLACEHOLDER } from "./models/constants";
import { FilterExpression, OrExpression, parseFilter } from "./models/filter";

export const PARSE_ERROR_MSG_ID = "parse-error-msg";

@customElement("log-control")
export class LogControl extends LitElement {
    static styles = [
        css`
            :host {
                --md-outlined-field-content-type: "Google Sans", Arial,
                    sans-serif;
                --md-outlined-field-container-padding-vertical: 0.5rem;
            }

            input {
                font-size: 1rem;
                font-family: "Google Sans";
                background: unset;
                border: unset;
                outline: none;
            }

            p#parse-error-msg {
                position: absolute;
            }

            p#filter-results {
                color: var(--vscode-foreground);
            }

            .visually-hidden {
                border: 0;
                clip: rect(0, 0, 0, 0);
                clip-path: inset(50%);
                -webkit-clip-path: inset(50%);
                height: 1px;
                overflow: hidden;
                padding: 0;
                position: absolute;
                white-space: nowrap;
                width: 1px;
            }
        `,
    ];

    public static readonly filterChangeEvent: string = "filter-change";

    @state()
    public resultsMessage = "";

    @state()
    private errorMessage = "";

    @property({ attribute: true, type: String })
    public value = "";

    constructor(filterText: string | undefined) {
        super();
        this.value = filterText ?? "";
        if (this.value.length > 0) {
            const result = parseFilter(this.value);
            if (!result.ok) {
                this.errorMessage = result.error;
            }
        }
    }

    render() {
        return html`
            <md-outlined-text-field
                id="search"
                type="text"
                placeholder="${SEARCH_PLACEHOLDER}"
                value="${this.value}"
                @keypress="${this.onInputKeyPress}"
                @keydown="${this.onInputKeyDown}"
                autocomplete="off"
                label="Search logs"
            >
                <md-icon slot="leadingicon">search</md-icon>
            </md-outlined-text-field>

            <p id="${PARSE_ERROR_MSG_ID}">${this.errorMessage}</p>
            <p role="alert" id="filter-results" class="visually-hidden">
                ${this.resultsMessage}
            </p>
        `;
    }

    /**
     * Resets the state of the log control.
     */
    public reset() {
        this.value = "";
    }

    /**
     * Handles a keypress on the search input. On submission, it parses the filter and publishes it.
     * @param event a keyboard event
     */
    private onInputKeyPress(event: KeyboardEvent) {
        if (event.key === "Enter") {
            const search = this.shadowRoot!.getElementById(
                "search"
            )! as HTMLInputElement;
            const searchText = search.value.trim();

            this.value = searchText;
            this.errorMessage = "";

            if (searchText.length === 0) {
                this.dispatchFilterChangeEvent(new OrExpression([]), "");
            } else {
                const searchText = this.value.trim();
                const result = parseFilter(searchText);
                if (result.ok) {
                    this.dispatchFilterChangeEvent(result.value, searchText);
                } else {
                    this.errorMessage = result.error;
                }
            }
        }
    }

    private onInputKeyDown(event: KeyboardEvent) {
        if (
            event.key === "Backspace" ||
            event.key === "Delete" ||
            event.key === "Clear"
        ) {
            this.errorMessage = "";
        }
    }

    /**
     * Dispatches an event carrying a filter that was submitted in the search input.
     * @param filter the filter that will be dispatched.
     * @param text the filter in text form.
     */
    private dispatchFilterChangeEvent(filter: FilterExpression, text: string) {
        this.dispatchEvent(
            new CustomEvent(LogControl.filterChangeEvent, {
                detail: { filter, text },
            })
        );
    }
}
