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
import { TableColumn } from '../../shared/interfaces';

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

  @property({ type: Array })
  columnData: TableColumn[] = [];

  /** Indicates whether to enable the button for closing the current log view. */
  @property({ type: Boolean })
  hideCloseButton = false;

  /** The title of the parent log view, to be displayed on the log view toolbar */
  @property()
  viewTitle = '';

  @property()
  searchText = '';

  @state()
  _moreActionsMenuOpen = false;

  @query('.field-menu') _fieldMenu!: HTMLMenuElement;

  @query('#search-field') _searchField!: HTMLInputElement;

  @queryAll('.item-checkboxes') _itemCheckboxes!: HTMLCollection[];

  /** The timer identifier for debouncing search input. */
  private _inputDebounceTimer: number | null = null;

  /** The delay (in ms) used for debouncing search input. */
  private readonly INPUT_DEBOUNCE_DELAY = 50;

  @query('.more-actions-button') moreActionsButtonEl!: HTMLElement;

  constructor() {
    super();
  }

  protected firstUpdated(): void {
    this._searchField.dispatchEvent(new CustomEvent('input'));
  }

  /**
   * Called whenever the search field value is changed. Debounces the input
   * event and dispatches an event with the input value after a specified
   * delay.
   *
   * @param {Event} event - The input event object.
   */
  private handleInput(event: Event) {
    const inputElement = event.target as HTMLInputElement;
    const inputValue = inputElement.value;

    // Update searchText immediately for responsiveness
    this.searchText = inputValue;

    // Debounce to avoid excessive updates and event dispatching
    if (this._inputDebounceTimer) {
      clearTimeout(this._inputDebounceTimer);
    }

    this._inputDebounceTimer = window.setTimeout(() => {
      this.dispatchEvent(
        new CustomEvent('input-change', {
          detail: { viewId: this.viewId, inputValue: inputValue },
          bubbles: true,
          composed: true,
        }),
      );
    }, this.INPUT_DEBOUNCE_DELAY);

    this.markKeysInText(this._searchField);
  }

  private markKeysInText(target: HTMLElement) {
    const pattern = /\b(\w+):(?=\w)/;
    const textContent = target.textContent || '';
    const conditions = textContent.split(/\s+/);
    const wordsBeforeColons: string[] = [];

    for (const condition of conditions) {
      const match = condition.match(pattern);
      if (match) {
        wordsBeforeColons.push(match[0]);
      }
    }
  }

  private handleKeydown = (event: KeyboardEvent) => {
    if (event.key === 'Enter' || event.key === 'Cmd') {
      event.preventDefault();
    }
  };

  /**
   * Dispatches a custom event for clearing logs. This event includes a
   * `timestamp` object indicating the date/time in which the 'clear-logs' event
   * was dispatched.
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

  /** Dispatches a custom event for toggling wrapping. */
  private handleWrapToggle() {
    const wrapToggle = new CustomEvent('wrap-toggle', {
      bubbles: true,
      composed: true,
    });

    this.dispatchEvent(wrapToggle);
  }

  /**
   * Dispatches a custom event for closing the parent view. This event includes
   * a `viewId` object indicating the `id` of the parent log view.
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
   * Dispatches a custom event for showing or hiding a column in the table. This
   * event includes a `field` string indicating the affected column's field name
   * and an `isChecked` boolean indicating whether to show or hide the column.
   *
   * @param {Event} event - The click event object.
   */
  private handleColumnToggle(event: Event) {
    const inputEl = event.target as HTMLInputElement;
    const columnToggle = new CustomEvent('column-toggle', {
      bubbles: true,
      composed: true,
      detail: {
        viewId: this.viewId,
        field: inputEl.value,
        isChecked: inputEl.checked,
      },
    });

    this.dispatchEvent(columnToggle);
  }

  private handleSplitRight() {
    const splitView = new CustomEvent('split-view', {
      detail: {
        columnData: this.columnData,
        viewTitle: this.viewTitle,
        searchText: this.searchText,
        orientation: 'horizontal',
        parentId: this.viewId,
      },
      bubbles: true,
      composed: true,
    });

    this.dispatchEvent(splitView);
  }

  private handleSplitDown() {
    const splitView = new CustomEvent('split-view', {
      detail: {
        columnData: this.columnData,
        viewTitle: this.viewTitle,
        searchText: this.searchText,
        orientation: 'vertical',
        parentId: this.viewId,
      },
      bubbles: true,
      composed: true,
    });

    this.dispatchEvent(splitView);
  }

  /**
   * Dispatches a custom event for downloading a logs file. This event includes
   * a `format` string indicating the format of the file to be downloaded and a
   * `viewTitle` string which passes the title of the current view for naming
   * the file.
   *
   * @param {Event} event - The click event object.
   */
  private handleDownloadLogs() {
    const downloadLogs = new CustomEvent('download-logs', {
      bubbles: true,
      composed: true,
      detail: {
        format: 'plaintext',
        viewTitle: this.viewTitle,
      },
    });

    this.dispatchEvent(downloadLogs);
  }

  /** Opens and closes the column visibility dropdown menu. */
  private toggleColumnVisibilityMenu() {
    this._fieldMenu.hidden = !this._fieldMenu.hidden;
  }

  /** Opens and closes the More Actions menu. */
  private toggleMoreActionsMenu() {
    this._moreActionsMenuOpen = !this._moreActionsMenuOpen;
  }

  render() {
    return html`
      <p class="host-name">${this.viewTitle}</p>

      <div class="input-container">
        <input
          id="search-field"
          type="text"
          .value="${this.searchText}"
          @input="${this.handleInput}"
          @keydown="${this.handleKeydown}"
        />
      </div>

      <div class="actions-container">
        <span class="action-button" title="Clear logs">
          <md-icon-button @click=${this.handleClearLogsClick}>
            <md-icon>&#xe16c;</md-icon>
          </md-icon-button>
        </span>

        <span class="action-button" title="Toggle line wrapping">
          <md-icon-button @click=${this.handleWrapToggle} toggle>
            <md-icon>&#xe25b;</md-icon>
          </md-icon-button>
        </span>

        <span class="action-button field-toggle" title="Toggle columns">
          <md-icon-button @click=${this.toggleColumnVisibilityMenu} toggle>
            <md-icon>&#xe8ec;</md-icon>
          </md-icon-button>
          <menu class="field-menu" hidden>
            ${this.columnData.map(
              (column) => html`
                <li class="field-menu-item">
                  <input
                    class="item-checkboxes"
                    @click=${this.handleColumnToggle}
                    ?checked=${column.isVisible}
                    type="checkbox"
                    value=${column.fieldName}
                    id=${column.fieldName}
                  />
                  <label for=${column.fieldName}>${column.fieldName}</label>
                </li>
              `,
            )}
          </menu>
        </span>

        <span class="action-button" title="Additional actions">
          <md-icon-button
            @click=${this.toggleMoreActionsMenu}
            class="more-actions-button"
          >
            <md-icon>&#xe5d4;</md-icon>
          </md-icon-button>

          <md-menu
            quick
            fixed
            ?open=${this._moreActionsMenuOpen}
            .anchor=${this.moreActionsButtonEl}
            @closed=${() => {
              this._moreActionsMenuOpen = false;
            }}
          >
            <md-menu-item
              headline="Split Right"
              @click=${this.handleSplitRight}
              role="button"
              title="Open a new view to the right of the current view"
            >
              <md-icon slot="start" data-variant="icon">&#xf674;</md-icon>
            </md-menu-item>

            <md-menu-item
              headline="Split Down"
              @click=${this.handleSplitDown}
              role="button"
              title="Open a new view below the current view"
            >
              <md-icon slot="start" data-variant="icon">&#xf676;</md-icon>
            </md-menu-item>

            <md-menu-item
              headline="Download logs (.txt)"
              @click=${this.handleDownloadLogs}
              role="button"
              title="Download current logs as a plaintext file"
            >
              <md-icon slot="start" data-variant="icon">&#xf090;</md-icon>
            </md-menu-item>
          </md-menu>
        </span>

        <span
          class="action-button"
          title="Close view"
          ?hidden=${this.hideCloseButton}
        >
          <md-icon-button @click=${this.handleCloseViewClick}>
            <md-icon>close</md-icon>
          </md-icon-button>
        </span>

        <span class="action-button" hidden>
          <md-icon-button>
            <md-icon>&#xe5d3;</md-icon>
          </md-icon-button>
        </span>
      </div>
    `;
  }
}
