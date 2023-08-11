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
import {
  customElement,
  property,
  query,
  queryAll,
  state,
} from 'lit/decorators.js';
import { styles } from './log-view-controls.styles';
import { State } from '../../shared/interfaces';
import { StateStore, LocalStorageState } from '../../shared/state';

/**
 * A sub-component of the log view with user inputs for managing and customizing
 * log entry display and interaction.
 *
 * @element log-view-controls
 */
@customElement('log-view-controls')
export class LogViewControls extends LitElement {
  static styles = styles;

  /** The `id` of the parent view containing this log list. */
  @property({ type: String })
  viewId = '';

  /** The field keys (column values) for the incoming log entries. */
  @property({ type: Array })
  fieldKeys: string[] = [];

  /** Indicates whether to enable the button for closing the current log view. */
  @property({ type: Boolean })
  hideCloseButton = false;

  @property({ type: String })
  _searchText = '';

  @property({ type: Array })
  colsHidden: (boolean | undefined)[] = [];

  /** A StateStore object that stores state of views */
  @state()
  _stateStore: StateStore = new LocalStorageState();

  @state()
  _state: State;

  @state()
  _viewTitle = '';

  @query('.field-menu') _fieldMenu!: HTMLMenuElement;

  @query('.search-input') _searchInput!: HTMLInputElement;

  @queryAll('.item-checkboxeses') _itemCheckboxes!: HTMLCollection[];

  private firstCheckboxLoad = false;

  /** The timer identifier for debouncing search input. */
  private _inputDebounceTimer: number | null = null;

  /** The delay (in ms) used for debouncing search input. */
  private readonly INPUT_DEBOUNCE_DELAY = 50;

  constructor() {
    super();
    this._state = this._stateStore.getState();
  }

  protected firstUpdated(): void {
    if (this._state !== null) {
      const viewConfigArr = this._state.logViewConfig;
      for (const i in viewConfigArr) {
        if (viewConfigArr[i].viewID === this.viewId) {
          this._searchText = viewConfigArr[i].search as string;
          this._viewTitle = viewConfigArr[i].viewTitle as string;
        }
      }
    }
    this._searchInput.value = this._searchText;
    this._searchInput.dispatchEvent(new CustomEvent('input'));
  }

  protected updated(): void {
    const checkboxItems = Array.from(this._itemCheckboxes);
    if (checkboxItems.length > 0 && !this.firstCheckboxLoad) {
      for (const i in checkboxItems) {
        const checkboxEl = checkboxItems[i] as unknown as HTMLInputElement;
        checkboxEl.checked = !this.colsHidden[Number(i) + 1];
      }
      this.firstCheckboxLoad = !this.firstCheckboxLoad;
    }
  }

  /**
   * Called whenever the search field value is changed. Debounces the input
   * event and dispatches an event with the input value after a specified
   * delay.
   *
   * @param {Event} event - The input event object.
   */
  private handleInput = (event: Event) => {
    if (this._inputDebounceTimer) {
      clearTimeout(this._inputDebounceTimer);
    }

    const inputElement = event.target as HTMLInputElement;
    const inputValue = inputElement.value;

    this._inputDebounceTimer = window.setTimeout(() => {
      const customEvent = new CustomEvent('input-change', {
        detail: { inputValue },
        bubbles: true,
        composed: true,
      });

      this.dispatchEvent(customEvent);
    }, this.INPUT_DEBOUNCE_DELAY);
  };

  /**
   * Dispatches a custom event for clearing logs. This event includes a
   * `timestamp` object indicating the date/time in which the 'clear-logs'
   * event was dispatched.
   */
  private handleClearLogsClick() {
    const timestamp = new Date();

    const clearLogs = new CustomEvent('clear-logs', {
      detail: { timestamp },
      bubbles: true,
      composed: true,
    });

    this.dispatchEvent(clearLogs);
  }

  /**
   * Dispatches a custom event for toggling wrapping.
   */
  private handleWrapToggle(event: Event) {
    const dropdownButton = event.target as HTMLElement;
    dropdownButton.classList.toggle('button-toggle-enabled');

    const wrapToggle = new CustomEvent('wrap-toggle', {
      bubbles: true,
      composed: true,
    });

    this.dispatchEvent(wrapToggle);
  }

  /**
   * Dispatches a custom event for closing the parent view. This event
   * includes a `viewId` object indicating the `id` of the parent log view.
   */
  private handleCloseViewClick() {
    const closeView = new CustomEvent('close-view', {
      bubbles: true,
      composed: true,
      detail: {
        viewId: this.viewId,
      },
    });

    this.dispatchEvent(closeView);
  }

  /**
   * Dispatches a custom event for showing or hiding a column in the table.
   * This event includes a `field` string indicating the affected column's
   * field name and an `isChecked` boolean indicating whether to show or hide
   * the column.
   *
   * @param {Event} event - The click event object.
   */
  private handleColumnToggle(event: Event) {
    const inputEl = event.target as HTMLInputElement;
    const columnToggle = new CustomEvent('column-toggle', {
      bubbles: true,
      composed: true,
      detail: {
        field: inputEl.value,
        isChecked: inputEl.checked,
      },
    });

    this.dispatchEvent(columnToggle);
  }

  /**
   * Opens and closes the column visibility dropdown menu.
   *
   * @param {Event} event - The click event object.
   */
  private toggleColumnVisibilityMenu(event: Event) {
    const dropdownButton = event.target as HTMLElement;
    this._fieldMenu.hidden = !this._fieldMenu.hidden;
    dropdownButton.classList.toggle('button-toggle-enabled');
  }

  render() {
    return html`
            <p class="host-name"> ${this._viewTitle}</p>

            <div class="input-container">
                <input class="search-input" placeholder="Search" type="text" @input=${
                  this.handleInput
                }></input>
            </div>

            <div class="actions-container">
                <span class="action-button" hidden>
                    <md-standard-icon-button>
                        <md-icon>pause_circle</md-icon>
                    </md-standard-icon-button>
                </span>

                <span class="action-button" hidden>
                    <md-standard-icon-button>
                        <md-icon>wrap_text</md-icon>
                    </md-standard-icon-button>
                </span>

                <span class="action-button" title="Clear logs">
                    <md-standard-icon-button @click=${
                      this.handleClearLogsClick
                    }>
                        <md-icon>delete_sweep</md-icon>
                    </md-standard-icon-button>
                </span>

                <span class="action-button" title="Toggle Line Wrapping">
                    <md-standard-icon-button @click=${this.handleWrapToggle}>
                        <md-icon>wrap_text</md-icon>
                    </md-standard-icon-button>
                </span>

                <span class='field-toggle' title="Toggle fields">
                    <md-standard-icon-button @click=${
                      this.toggleColumnVisibilityMenu
                    }>
                        <md-icon>view_column</md-icon>
                    </md-standard-icon-button>
                    <menu class='field-menu' hidden>
                        ${Array.from(this.fieldKeys).map(
                          (field) => html`
                            <li class="field-menu-item">
                              <input
                                class="item-checkboxeses"
                                @click=${this.handleColumnToggle}
                                checked
                                type="checkbox"
                                value=${field}
                                id=${field}
                              />
                              <label for=${field}>${field}</label>
                            </li>
                          `,
                        )}
                    </menu>
                </span>

                <span class="action-button" title="Close view" ?hidden=${
                  this.hideCloseButton
                }>
                    <md-standard-icon-button  @click=${
                      this.handleCloseViewClick
                    }>
                        <md-icon>close</md-icon>
                    </md-standard-icon-button>
                </span>

                <span class="action-button" hidden>
                    <md-standard-icon-button>
                        <md-icon>more_horiz</md-icon>
                    </md-standard-icon-button>
                </span>
            </div>
        `;
  }
}
