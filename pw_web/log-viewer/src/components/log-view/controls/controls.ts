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

import { LitElement, html } from "lit";
import { customElement, property } from "lit/decorators.js";
import { styles } from "./controls.styles";

/**
 * Description of LogViewControls.
 */
@customElement("log-view-controls")
export class LogViewControls extends LitElement {
    static styles = styles;

    @property({ type: Array })
    fieldKeys: string[];

    constructor() {
        super();
        this.fieldKeys = [];
    }

    render() {
        return html`
            <p class="host-name">Host name</p>
            
            <div class="input-container">
                <input placeholder="Search" type="text"></input>
            </div>

            <div class="actions-container">
                <md-standard-icon-button>
                    <md-icon>pause_circle</md-icon>
                </md-standard-icon-button>

                <md-standard-icon-button>
                    <md-icon>wrap_text</md-icon>
                </md-standard-icon-button>

                <md-standard-icon-button>
                    <md-icon>delete_sweep</md-icon>
                </md-standard-icon-button>

                <div class='field-toggle'>
                    <md-standard-icon-button @click=${this.toggleFieldsDropdown}>
                        <md-icon>view_column</md-icon>
                    </md-standard-icon-button>
                    <menu class='field-menu' hidden>
                        ${Array.from(this.fieldKeys).map((field) => html`
                        <li class='field-menu-item'>
                        <input class='fields' @click=${this.handleFieldToggle} checked type='checkbox' value=${field}>
                            <label for=${field}>${field}</label>
                        </li>
                        `)}
                    </menu>
                </div>

                <md-standard-icon-button>
                    <md-icon>more_horiz</md-icon>
                </md-standard-icon-button>
            </div>
        `;
    }

    handleFieldToggle(e: Event) {
        // TODO(b/283505711): Handle select all/none condition
        const inputEl = e.target as HTMLInputElement;
        let fieldToggle = new CustomEvent('field-toggle', {
            detail: {
                bubbles: true,
                composed: true,
                message: 'visible fields have changed',
                field: inputEl.value,
                isChecked: inputEl.checked,
            }
        });
        this.dispatchEvent(fieldToggle);
    }

    toggleFieldsDropdown() {
        const dropdownElement = this.renderRoot.querySelector('.field-menu') as HTMLElement;
        const dropdownButton = this.renderRoot.querySelector('.field-toggle') as HTMLElement;
        dropdownElement.hidden = (dropdownElement!.hidden == true) ? false : true;
        dropdownButton.classList.toggle('button-toggle');
    }
}
