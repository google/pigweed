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

import { LitElement, html, PropertyValues } from "lit";
import { customElement, property, state } from "lit/decorators.js";
import { LogEntry } from "../../../shared/interfaces";
import { styles } from "./log-list.styles";

/**
 * Description of LogList.
 */
@customElement("log-list")
export class LogList extends LitElement {
    static styles = styles;

    /**
     * Description of logs.
     */
    @property({ type: Array })
    logs: LogEntry[];

    /**
     * Description of filter.
     */
    @state()
    private _maxEntries: number;

    @state()
    private _fieldKeys = new Set<string>();

    @state()
    private _isOverflowingToRight: boolean;

    @state()
    private scrollPercentageHorizontal: number;

    private columnResizeData: {
        columnIndex: number;
        startX: number;
        startWidth: number;
    } | null = null;

    constructor() {
        super();
        this.logs = [];
        this._maxEntries = 32;
        this._isOverflowingToRight = false;
        this.scrollPercentageHorizontal = 0;
    }

    updated(changedProperties: PropertyValues) {
        super.updated(changedProperties);
        setInterval(() => this.updateOverflowIndicators(), 1000);

        window.addEventListener("scroll", this.handleScroll);
        // this.addResizeListeners();

        if (
            changedProperties.has("offsetWidth") ||
            changedProperties.has("scrollWidth")
        ) {
            this.updateOverflowIndicators();
        }
    }

    firstUpdated() {
        this.updateWidth();
    }

    disconnectedCallback() {
        super.disconnectedCallback();

        const table = this.renderRoot.querySelector(".table-container");
        table?.removeEventListener("scroll", this.handleScroll);
    }

    render() {
        const logsDisplayed = this.logs.slice(0, this._maxEntries);

        logsDisplayed.forEach((logEntry) => {
            logEntry.fields.forEach((fieldData) => {
                this._fieldKeys.add(fieldData.key);
            });
        });

        return html`
            <div class="table-container" role="region" tabindex="0">
                <table @wheel="${this.handleScroll}">
                    <tr>
                        ${Array.from(this._fieldKeys).map(
                            (fieldKey, columnIndex) => html`
                                <th>
                                    <span class="cell-content"
                                        >${fieldKey}</span
                                    >
                                    ${columnIndex > 0
                                        ? html`
                                              <div
                                                  class="resize-handle"
                                                  @mousedown="${(
                                                      event: MouseEvent
                                                  ) =>
                                                      this.handleColumnResizeStart(
                                                          event,
                                                          columnIndex - 1
                                                      )}"
                                              ></div>
                                          `
                                        : html``}
                                </th>
                            `
                        )}
                    </tr>

                    ${logsDisplayed.map(
                        (log: LogEntry) => html` <tr>
                            ${log.fields.map(
                                (field, columnIndex) =>
                                    html`
                                        <td>
                                            <span class="cell-content"
                                                >${field.value}</span
                                            >

                                            ${columnIndex > 0
                                                ? html`
                                                      <div
                                                          class="resize-handle"
                                                          @mousedown="${(
                                                              event: MouseEvent
                                                          ) =>
                                                              this.handleColumnResizeStart(
                                                                  event,
                                                                  columnIndex -
                                                                      1
                                                              )}"
                                                      ></div>
                                                  `
                                                : html``}
                                        </td>
                                    `
                            )}
                        </tr>`
                    )}
                </table>

                <div
                    class="overflow-indicator right-indicator"
                    style="opacity: ${1 - this.scrollPercentageHorizontal}"
                    ?hidden="${!this._isOverflowingToRight}"
                ></div>
                <div
                    class="overflow-indicator left-indicator"
                    style="opacity: ${this.scrollPercentageHorizontal}"
                    ?hidden="${!this._isOverflowingToRight}"
                ></div>
            </div>
        `;
    }

    handleScroll = () => {
        const container = this.renderRoot.querySelector(
            ".table-container"
        ) as HTMLElement;

        if (!container) {
            return;
        }

        const containerWidth = container.offsetWidth;
        const containerHeight = container.offsetHeight;
        const scrollLeft = container.scrollLeft;
        const scrollTop = container.scrollTop;
        const maxScrollLeft = container.scrollWidth - containerWidth;
        const maxScrollTop = container.scrollHeight - containerHeight;

        this.scrollPercentageHorizontal = scrollLeft / maxScrollLeft || 0;
        // this.scrollPercentageVertical = scrollTop / maxScrollTop || 0;

        this.requestUpdate();
    };

    updateOverflowIndicators() {
        const table = this.renderRoot.querySelector(".table-container");

        if (!table) {
            return;
        }

        const containerWidth = this.offsetWidth;
        const tableWidth = table!.scrollWidth;
        const containerHeight = this.offsetHeight;
        const tableHeight = table!.scrollHeight;

        this._isOverflowingToRight = tableWidth > containerWidth;
    }

    updateWidth() {
        const table = this.shadowRoot?.querySelector("table");

        if (table) {
            // Calculate the width of the table
            const tableWidth = table.offsetWidth;

            // Apply the calculated width as fixed value
            table.style.width = `${tableWidth}px`;
        }
    }

    handleColumnResizeStart(event: MouseEvent, columnIndex: number) {
        event.preventDefault();
        const startX = event.clientX;
        const table = this.renderRoot.querySelector(
            "table"
        ) as HTMLTableElement;

        const th = table.querySelector(
            `th:nth-child(${columnIndex + 1})`
        ) as HTMLTableHeaderCellElement;
        const startWidth = th.offsetWidth;

        this.columnResizeData = {
            columnIndex,
            startX,
            startWidth,
        };

        const handleColumnResize = (event: MouseEvent) => {
            this.handleColumnResize(event);
        };

        const handleColumnResizeEnd = () => {
            this.columnResizeData = null;
            document.removeEventListener("mousemove", handleColumnResize);
            document.removeEventListener("mouseup", handleColumnResizeEnd);
        };

        document.addEventListener("mousemove", handleColumnResize);
        document.addEventListener("mouseup", handleColumnResizeEnd);
    }

    handleColumnResize(event: MouseEvent) {
        if (!this.columnResizeData) return;
        const { columnIndex, startX, startWidth } = this.columnResizeData;
        const table = this.renderRoot.querySelector(
            "table"
        ) as HTMLTableElement;
        const th = table.querySelector(
            `th:nth-child(${columnIndex + 1})`
        ) as HTMLTableHeaderCellElement;
        const tds = table.querySelectorAll(
            `td:nth-child(${columnIndex + 1})`
        ) as NodeListOf<HTMLTableDataCellElement>;

        const offsetX = event.clientX - startX;
        const newWidth = Math.max(startWidth + offsetX, 20); // Minimum width

        th.style.width = `${newWidth}px`;
        for (let i = 0; i < tds.length; i++) {
            tds[i].style.width = `${newWidth}px`;
        }
    }
}
