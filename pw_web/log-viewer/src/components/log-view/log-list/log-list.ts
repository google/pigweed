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

import { LitElement, html, PropertyValues, TemplateResult } from 'lit';
import { customElement, property, state } from 'lit/decorators.js';
import { FieldData, LogEntry } from '../../../shared/interfaces';
import { styles } from './log-list.styles';
import '@lit-labs/virtualizer';
import { virtualize } from '@lit-labs/virtualizer/virtualize.js';

/**
 * Description of LogList.
 */
@customElement('log-list')
export class LogList extends LitElement {
    static styles = styles;

    /**
     * Description of viewId.
     */
    @property()
    viewId = '';

    /**
     * Description of logs.
     */
    @property({ type: Array })
    logs: LogEntry[] = [];

    /**
     * Description of filterValue.
     */
    @property({ type: String })
    filterValue = '';

    /**
     * Description of _maxEntries.
     */
    @state()
    private _maxEntries = 100_000;

    /**
     * Description of _fieldNames.
     */
    @state()
    private _fieldNames = new Set<string>();

    /**
     * Description of _isOverflowingToRight.
     */
    @state()
    private _isOverflowingToRight = false;

    /**
     * Description of _scrollPercentageHorizontal.
     */
    @state()
    private _scrollPercentageHorizontal = 0;

    /**
     * Description of _autoscrollIsEnabled.
     */
    @state()
    private _autoscrollIsEnabled = true;

    /**
     * Description of columnResizeData.
     */
    private columnResizeData: {
        columnIndex: number;
        startX: number;
        startWidth: number;
    } | null = null;

    @property({ type: Array })
    colsHidden: boolean[] = [];

    constructor() {
        super();
    }

    firstUpdated() {
        window.addEventListener('scroll', this.handleTableScroll);
        this.renderRoot
            .querySelector('tbody')
            ?.addEventListener('rangeChanged', this.updateGridTemplateColumns);

        if (this.logs.length > 0) {
            this.performUpdate();
        }
    }

    updated(changedProperties: PropertyValues) {
        super.updated(changedProperties);
        setInterval(() => this.updateOverflowIndicators(), 1000);

        if (
            changedProperties.has('offsetWidth') ||
            changedProperties.has('scrollWidth')
        ) {
            this.updateOverflowIndicators();
        }

        if (changedProperties.has('logs')) {
            this.updateTableView();
        }
    }

    private updateTableView() {
        const container = this.renderRoot.querySelector(
            '.table-container'
        ) as HTMLElement;

        if (container && this._autoscrollIsEnabled) {
            container.scrollTop = container.scrollHeight;
        }
    }

    clearGridTemplateColumns() {
        const table = this.renderRoot.querySelector(
            'table'
        ) as HTMLTableElement;
        const rows = Array.from(table.querySelectorAll('tr'));

        rows.forEach((row) => {
            row.style.gridTemplateColumns = '';
        });
    }

    updateGridTemplateColumns = () => {
        const table = this.renderRoot.querySelector(
            'table'
        ) as HTMLTableElement;
        const rows = Array.from(table.querySelectorAll('tr'));

        // TODO(b/287277404): move this logic into a separate function
        // Set column visibility based on `colsHidden` array
        rows.forEach((row) => {
            const cells = Array.from(row.querySelectorAll('td, th'));
            cells.forEach((cell, index: number) => {
                const colHidden = this.colsHidden[index];

                const cellEl = cell as HTMLElement;
                cellEl.hidden = colHidden;
            });
        });

        // Get the number of visible columns
        const columnCount =
            Array.from(rows[0]?.children || []).filter(
                (child) => !child.hasAttribute('hidden')
            ).length || 0;

        // Initialize an array to store the maximum width of each column
        const columnWidths: number[] = new Array(columnCount).fill(0);

        // Iterate through each row to find the maximum width in each column
        rows.forEach((row) => {
            const cells = Array.from(row.children).filter(
                (cell) => !cell.hasAttribute('hidden')
            ) as HTMLTableCellElement[];

            cells.forEach((cell, columnIndex) => {
                const cellWidth = cell.getBoundingClientRect().width;
                columnWidths[columnIndex] = Math.max(
                    columnWidths[columnIndex],
                    cellWidth
                );
            });
        });

        // Generate the gridTemplateColumns value for each row
        rows.forEach((row) => {
            const gridTemplateColumns = columnWidths
                .map((width, index) => {
                    if (index === columnWidths.length - 1) {
                        return '1fr';
                    } else {
                        return `${width}px`;
                    }
                })
                .join(' ');
            row.style.gridTemplateColumns = gridTemplateColumns;
        });
    };

    disconnectedCallback() {
        super.disconnectedCallback();

        const container = this.renderRoot.querySelector('.table-container');
        container?.removeEventListener('scroll', this.handleTableScroll);
    }

    highlightMatchedText(text: string): TemplateResult[] {
        if (!this.filterValue) {
            return [html`${text}`];
        }

        const escapedFilterValue = this.filterValue.replace(
            /[.*+?^${}()|[\]\\]/g,
            '\\$&'
        );
        const regex = new RegExp(`(${escapedFilterValue})`, 'gi');
        const parts = text.split(regex);
        return parts.map((part) =>
            regex.test(part) ? html`<mark>${part}</mark>` : html`${part}`
        );
    }

    render() {
        const logsDisplayed: LogEntry[] = this.logs.slice(0, this._maxEntries);
        this.setFieldNames(logsDisplayed);

        return html`
            <div
                class="table-container"
                role="log"
                @wheel="${this.handleTableScroll}"
            >
                <table>
                    <thead>
                        ${this.tableHeaderRow()}
                    </thead>

                    <tbody>
                        ${virtualize({
                            items: logsDisplayed,
                            renderItem: (log) =>
                                html`${this.tableDataRow(log)}`,
                        })}
                    </tbody>
                </table>

                ${this.overflowIndicators()}
            </div>
        `;
    }

    setFieldNames(logs: LogEntry[]) {
        logs.forEach((logEntry) => {
            logEntry.fields.forEach((fieldData) => {
                this._fieldNames.add(fieldData.key);
            });
        });
    }

    tableHeaderRow() {
        return html`
            <tr>
                ${Array.from(this._fieldNames).map((fieldKey, columnIndex) =>
                    this.tableHeaderCell(fieldKey, columnIndex)
                )}
            </tr>
        `;
    }

    tableHeaderCell(fieldKey: string, columnIndex: number) {
        return html`
            <th>
                ${fieldKey}
                ${columnIndex > 0 ? this.resizeHandle(columnIndex - 1) : html``}
            </th>
        `;
    }

    resizeHandle(columnIndex: number) {
        return html`
            <span
                class="resize-handle"
                @mousedown="${(event: MouseEvent) =>
                    this.handleColumnResizeStart(event, columnIndex)}"
            ></span>
        `;
    }

    tableDataRow(log: LogEntry) {
        return html`
            <tr>
                ${log.fields.map((field, columnIndex) =>
                    this.tableDataCell(field, columnIndex)
                )}
            </tr>
        `;
    }

    tableDataCell = (field: FieldData, columnIndex: number) => html`
        <td>
            ${this.highlightMatchedText(field.value.toString())}
            ${columnIndex > 0 ? this.resizeHandle(columnIndex - 1) : html``}
        </td>
    `;

    overflowIndicators = () => html`
        <div
            class="overflow-indicator left-indicator"
            style="opacity: ${this._scrollPercentageHorizontal}"
            ?hidden="${!this._isOverflowingToRight}"
        ></div>

        <div
            class="overflow-indicator right-indicator"
            style="opacity: ${1 - this._scrollPercentageHorizontal}"
            ?hidden="${!this._isOverflowingToRight}"
        ></div>
    `;

    handleTableScroll = () => {
        const container = this.renderRoot.querySelector(
            '.table-container'
        ) as HTMLElement;

        if (!container) {
            return;
        }

        const containerWidth = container.offsetWidth;
        const scrollLeft = container.scrollLeft;
        const maxScrollLeft = container.scrollWidth - containerWidth;
        const threshold = 128;

        this._scrollPercentageHorizontal = scrollLeft / maxScrollLeft || 0;

        if (
            container.scrollHeight - container.scrollTop <=
            container.offsetHeight + threshold
        ) {
            this._autoscrollIsEnabled = true;
        } else {
            this._autoscrollIsEnabled = false;
        }

        this.requestUpdate();
    };

    updateOverflowIndicators() {
        const container = this.renderRoot.querySelector('.table-container');

        if (!container) {
            return;
        }

        const containerWidth = this.offsetWidth;
        const tableWidth = container?.scrollWidth;

        this._isOverflowingToRight = tableWidth > containerWidth;
    }

    handleColumnResizeStart(event: MouseEvent, columnIndex: number) {
        event.preventDefault();
        const startX = event.clientX;
        const table = this.renderRoot.querySelector(
            'table'
        ) as HTMLTableElement;

        const th = table.querySelector(
            `th:nth-child(${columnIndex + 1})`
        ) as HTMLTableCellElement;
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
            document.removeEventListener('mousemove', handleColumnResize);
            document.removeEventListener('mouseup', handleColumnResizeEnd);
        };

        document.addEventListener('mousemove', handleColumnResize);
        document.addEventListener('mouseup', handleColumnResizeEnd);
    }

    handleColumnResize(event: MouseEvent) {
        if (!this.columnResizeData) return;
        const { columnIndex, startX, startWidth } = this.columnResizeData;
        const table = this.renderRoot.querySelector(
            'table'
        ) as HTMLTableElement;

        const th = table.querySelector(
            `th:nth-child(${columnIndex + 1})`
        ) as HTMLTableCellElement;

        const offsetX = event.clientX - startX;
        const newWidth = Math.max(startWidth + offsetX, 48); // Minimum width

        th.style.width = `${newWidth}px`;

        const totalColumns = table.querySelectorAll('th').length;
        let gridTemplateColumns = '';

        for (let i = 0; i < totalColumns; i++) {
            if (i === columnIndex) {
                gridTemplateColumns += `${newWidth}px `;
            } else {
                const tableEl = table.querySelector(
                    `th:nth-child(${i + 1})`
                ) as HTMLElement;
                const otherColumnWidth = tableEl?.offsetWidth;

                gridTemplateColumns += `${otherColumnWidth}px `;
            }
        }

        const rows = table.querySelectorAll('tr');
        rows.forEach((row) => {
            row.style.gridTemplateColumns = gridTemplateColumns;
        });
    }
}
