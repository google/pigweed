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
        position: relative;
        display: block;
        height: 100%;
        font-family: 'Roboto Mono', monospace;
    }

    .table-container {
        width: 100%;
        height: 100%;
        overflow: auto;
        padding-bottom: 3rem;
        scroll-behavior: auto;
    }

    table {
        width: auto;
        height: 100%;
        min-width: 100vw;
        table-layout: fixed;
        border-collapse: collapse;
    }

    thead,
    th {
        position: sticky;
        top: 0;
        z-index: 1;
    }

    thead {
        background: #2d3134;
    }

    tr {
        display: grid;
        width: 100%;
        justify-content: flex-start;
        border-bottom: 1px solid #4b5054;
    }

    tr:hover > td {
        background: rgba(58, 58, 58, .5);
    }

    th,
    td {
        padding: 0.5rem 1rem;
        text-align: left;
        display: block;
        overflow: hidden;
        text-overflow: ellipsis;
        grid-row: 1;
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
        content: '';
        position: absolute;
        top: 0;
        right: 0;
        bottom: 0;
        left: 0;
        z-index: 1;
        width: 1px;
        height: 100%;
        opacity: 1;
        background-color: #4b5054;
        cursor: col-resize;
        pointer-events: auto;
        transition: opacity 300ms ease;
    }

    .resize-handle:hover {
        background-color: var(--md-sys-color-primary);
        outline: 1px solid var(--md-sys-color-primary);
    }

    .resize-handle::before {
        content: '';
        position: absolute;
        top: 0;
        right: -0.5rem;
        bottom: 0;
        width: 1rem;
        display: block;
    }

    .cell-icon {
        font-size: 1rem;
        font-variation-settings: 'FILL' 1, 'wght' 400, 'GRAD' 200, 'opsz' 58;
        padding: 0.25rem 0.25rem 0.25rem 0;
    }

    .overflow-indicator {
        position: absolute;
        top: 0;
        width: 4rem;
        height: 100%;
        pointer-events: none;
    }

    .right-indicator {
        right: 0;
        background: linear-gradient(to right, transparent, rgb(47 47 47));
    }

    .left-indicator {
        left: 0;
        background: linear-gradient(to left, transparent, rgb(47 47 47));
    }

    mark {
        background-color: var(--md-sys-color-primary);
        outline: 1px solid var(--md-sys-color-primary);
        border-radius: 2px;
    }
`;
