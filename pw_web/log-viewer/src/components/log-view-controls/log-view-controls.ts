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
import { MdMenu } from '@material/web/menu/menu';

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

  @property({ type: Boolean, reflect: true })
  searchExpanded = false;

  /**
   * Flag to determine whether Shoelace components should be used by
   * `LogViewControls`.
   */
  @property({ type: Boolean })
  useShoelaceFeatures = true;

  @state()
  _colToggleMenuOpen = false;

  @state()
  _addlActionsMenuOpen = false;

  @state()
  _toolbarCollapsed = false;

  @query('.col-toggle-menu') _colToggleMenu!: HTMLElement;

  @query('.search-field') _searchField!: HTMLInputElement;

  @query('.addl-actions-button') _addlActionsButton!: HTMLElement;

  @query('.addl-actions-menu') _addlActionsMenu!: HTMLElement;

  @queryAll('.item-checkboxes') _itemCheckboxes!: HTMLCollection[];

  /** The timer identifier for debouncing search input. */
  private _inputDebounceTimer: number | null = null;

  /** The delay (in ms) used for debouncing search input. */
  private readonly INPUT_DEBOUNCE_DELAY = 50;

  private resizeObserver = new ResizeObserver((entries) =>
    this.handleResize(entries),
  );

  connectedCallback() {
    super.connectedCallback();
    this.resizeObserver.observe(this);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.resizeObserver.unobserve(this);
  }

  protected firstUpdated(): void {
    this._searchField.dispatchEvent(new CustomEvent('input'));

    // Supply anchor element to Material Web menus
    if (this._addlActionsMenu) {
      (this._addlActionsMenu as MdMenu).anchorElement = this._addlActionsButton;
    }
  }

  /**
   * Called whenever the search field value is changed. Debounces the input
   * event and dispatches an event with the input value after a specified
   * delay.
   *
   * @param {Event} event - The input event object.
   */
  private handleInput(event: Event) {
    const inputElement =
      (event.target as HTMLInputElement) ?? this._searchField;
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

  private handleResize(entries: ResizeObserverEntry[]) {
    for (const entry of entries) {
      if (entry.contentRect.width < 800) {
        this._toolbarCollapsed = true;
      } else {
        this.searchExpanded = false;
        this._toolbarCollapsed = false;
      }
    }

    this._colToggleMenuOpen = false;
    this._addlActionsMenuOpen = false;
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
    this._colToggleMenuOpen = !this._colToggleMenuOpen;
  }

  /** Opens and closes the Additional Actions menu. */
  private toggleAddlActionsMenu() {
    this._addlActionsMenuOpen = !this._addlActionsMenuOpen;
  }

  /** Opens and closes the search field while it is in a collapsible state. */
  private toggleSearchField() {
    this.searchExpanded = !this.searchExpanded;
  }

  private handleClearSearchClick() {
    this._searchField.value = '';

    const event = new Event('input', {
      bubbles: true,
      cancelable: true,
    });
    this.handleInput(event);

    this.searchExpanded = false;
  }

  render() {
    return html`
      <p class="host-name">${this.viewTitle}</p>

      <div class="toolbar" role="toolbar">
        <md-filled-text-field
          class="search-field"
          placeholder="Filter logs"
          ?hidden=${this._toolbarCollapsed && !this.searchExpanded}
          .value="${this.searchText}"
          @input="${this.handleInput}"
          @keydown="${this.handleKeydown}"
        >
          <md-icon-button
            @click=${this.handleClearSearchClick}
            ?hidden=${this._searchField?.value === '' && !this.searchExpanded}
            title="Clear filter query"
            slot="trailing-icon"
          >
            <md-icon>&#xe888;</md-icon>
          </md-icon-button>
          <md-icon-button
            href="https://pigweed.dev/pw_web/log_viewer.html#filter-logs"
            target="_blank"
            slot="trailing-icon"
            title="Go to the log filter documentation page"
          >
            <md-icon>&#xe8fd;</md-icon>
          </md-icon-button>
        </md-filled-text-field>

        <div class="actions-container">
          <span
            class="action-button"
            title="Expand search field"
            ?hidden=${!this._toolbarCollapsed}
          >
            <md-icon-button
              @click=${this.toggleSearchField}
              ?toggle=${this.searchExpanded}
              ?selected=${this.searchExpanded}
              ?hidden=${this.searchExpanded}
            >
              <md-icon>&#xe8b6;</md-icon>
              <md-icon slot="selected">&#xea76;</md-icon>
            </md-icon-button>
          </span>

          <span
            class="action-button"
            title="Clear logs"
            ?hidden=${this._toolbarCollapsed}
          >
            <md-icon-button @click=${this.handleClearLogsClick}>
              <md-icon>&#xe16c;</md-icon>
            </md-icon-button>
          </span>

          <span
            class="action-button"
            title="Toggle line wrapping"
            ?hidden=${this._toolbarCollapsed}
          >
            <md-icon-button @click=${this.handleWrapToggle} toggle>
              <md-icon>&#xe25b;</md-icon>
            </md-icon-button>
          </span>

          <span
            class="action-button field-toggle"
            title="Toggle columns"
            ?hidden=${this._toolbarCollapsed}
          >
            <md-icon-button @click=${this.toggleColumnVisibilityMenu}>
              <md-icon>&#xe8ec;</md-icon>
            </md-icon-button>
          </span>

          <menu class="col-toggle-menu" ?hidden=${!this._colToggleMenuOpen}>
            ${this.columnData.map(
              (column) => html`
                <li class="col-toggle-menu-item">
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

          <span class="" title="Additional actions">
            <md-icon-button
              @click=${this.toggleAddlActionsMenu}
              class="addl-actions-button"
            >
              <md-icon>&#xe5d4;</md-icon>
            </md-icon-button>

            <md-menu
              quick
              class="addl-actions-menu"
              positioning="popover"
              ?open=${this._addlActionsMenuOpen}
              @closed=${() => {
                this._addlActionsMenuOpen = false;
              }}
            >
              <md-menu-item
                @click=${this.handleWrapToggle}
                role="button"
                title="Toggle line wrapping"
                ?hidden=${!this._toolbarCollapsed}
              >
                <md-icon slot="start" data-variant="icon">&#xe25b;</md-icon>
                <div slot="headline">Toggle line wrapping</div>
              </md-menu-item>

              <md-menu-item
                @click=${this.toggleColumnVisibilityMenu}
                role="button"
                title="Toggle columns"
                ?hidden=${!this._toolbarCollapsed}
              >
                <md-icon slot="start" data-variant="icon">&#xe8ec;</md-icon>
                <div slot="headline">Toggle columns</div>
              </md-menu-item>

              <md-menu-item
                @click=${this.handleSplitRight}
                role="button"
                title="Open a new view to the right of the current view"
                ?hidden=${!this.useShoelaceFeatures}
              >
                <md-icon slot="start" data-variant="icon">&#xf674;</md-icon>
                <div slot="headline">Split right</div>
              </md-menu-item>

              <md-menu-item
                @click=${this.handleSplitDown}
                role="button"
                title="Open a new view below the current view"
                ?hidden=${!this.useShoelaceFeatures}
              >
                <md-icon slot="start" data-variant="icon">&#xf676;</md-icon>
                <div slot="headline">Split down</div>
              </md-menu-item>

              <md-menu-item
                @click=${this.handleDownloadLogs}
                role="button"
                title="Download current logs as a plaintext file"
              >
                <md-icon slot="start" data-variant="icon">&#xf090;</md-icon>
                <div slot="headline">Download logs (.txt)</div>
              </md-menu-item>

              <md-menu-item
                @click=${this.handleClearLogsClick}
                role="button"
                title="Clear logs"
                ?hidden=${!this._toolbarCollapsed}
              >
                <md-icon slot="start" data-variant="icon">&#xe16c;</md-icon>
                <div slot="headline">Clear logs</div>
              </md-menu-item>
            </md-menu>
          </span>

          <span class="action-button" title="Close view">
            <md-icon-button
              ?hidden=${this.hideCloseButton}
              @click=${this.handleCloseViewClick}
            >
              <md-icon>&#xe5cd;</md-icon>
            </md-icon-button>
          </span>

          <span class="action-button" hidden>
            <md-icon-button>
              <md-icon>&#xe5d3;</md-icon>
            </md-icon-button>
          </span>
        </div>
      </div>
    `;
  }
}
