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

import { css } from 'lit';

export const styles = css`
    * {
        box-sizing: border-box;
    }

    :host {
        background-color: var(--md-sys-color-surface);
        display: block;
        font-family: 'Roboto Mono', monospace;
        height: 100%;
        position: relative;
    }

    .table-container {
        height: 100%;
        overflow: auto;
        padding-bottom: 3rem;
        scroll-behavior: auto;
        width: 100%;
    }

    table {
        border-collapse: collapse;
        height: 100%;
        min-width: 100vw;
        table-layout: fixed;
        width: auto;
    }

    thead,
    th {
        position: sticky;
        top: 0;
        z-index: 1;
    }

    thead {
        background: var(--md-sys-surface-container);
    }

    tr {
        border-bottom: 1px solid var(--md-sys-color-outline-variant);
        display: grid;
        justify-content: flex-start;
        width: 100%;
    }

    tr:hover > td {
        background-color: rgba(var(--md-sys-inverse-surface-rgb), 0.1);
        color: var(--sys-on-inverse-surface);
    }

    th,
    td {
        display: block;
        grid-row: 1;
        overflow: hidden;
        padding: 0.5rem 1rem;
        text-align: left;
        text-overflow: ellipsis;
    }

    th[hidden],
    td[hidden] {
        display: none;
    }

    th {
        grid-row: 1;
        white-space: nowrap;
    }

    td {
        display: inline-flex;
        position: relative;
        vertical-align: top;
    }

    .resize-handle {
        background-color: var(--md-sys-color-outline-variant);
        bottom: 0;
        content: '';
        cursor: col-resize;
        height: 100%;
        left: 0;
        opacity: 1;
        pointer-events: auto;
        position: absolute;
        right: 0;
        top: 0;
        transition: opacity 300ms ease;
        width: 1px;
        z-index: 1;
    }

    .resize-handle:hover {
        background-color: var(--md-sys-color-primary);
        outline: 1px solid var(--md-sys-color-primary);
    }

    .resize-handle::before {
        bottom: 0;
        content: '';
        display: block;
        position: absolute;
        right: -0.5rem;
        top: 0;
        width: 1rem;
    }

    .cell-icon {
        font-size: 1rem;
        font-variation-settings: 'FILL' 1, 'wght' 400, 'GRAD' 200, 'opsz' 58;
        padding: 0.25rem 0.25rem 0.25rem 0;
    }

    .overflow-indicator {
        height: 100%;
        pointer-events: none;
        position: absolute;
        top: 0;
        width: 4rem;
    }

    .right-indicator {
        background: linear-gradient(to right, transparent, var(--md-sys-color-surface));
        right: 0;
    }

    .left-indicator {
        background: linear-gradient(to left, transparent, var(--md-sys-color-surface));
        left: 0;
    }

    mark {
        background-color: var(--md-sys-color-primary-container);
        border-radius: 2px;
        color: var(--md-sys-color-on-primary-container);
        outline: 1px solid var(--md-sys-color-outline-variant);
    }
`;
