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

import { css } from "lit";

export const styles = css`
    :host {
        display: block;
        font-family: "Roboto Mono", monospace;
        overflow: scroll;
        height: 100%;
        position: relative;
    }

    * {
        box-sizing: border-box;
    }

    .table-container {
        overflow: auto;
        width: 100%;
        height: 100%;
    }

    table {
        min-width: 100%;
        width: auto;
        table-layout: fixed;
        border-collapse: collapse;
    }

    tr:hover {
        background: rgb(47 47 47);
    }

    th,
    td {
        width: auto;
        padding: 0.5rem 1rem;
        text-align: left;
        border-bottom: 1px solid #4b5054;
        text-align: left;
        position: relative;
    }

    th {
        position: sticky;
        top: 0;
        z-index: 1;
        background: #2d3134;
        white-space: nowrap;
    }

    td {
        vertical-align: top;
        max-width: 96ch;
    }

    .resize-handle {
        content: "";
        position: absolute;
        top: 0;
        right: 0;
        bottom: 0;
        left: 0;
        width: 1px;
        height: 100%;
        cursor: col-resize;
        z-index: 1;
        opacity: 1;
        background-color: #4b5054;
        transition: opacity 0.3s ease;
        pointer-events: auto;
    }

    .resize-handle:hover {
        background-color: #4cdada;
        outline: 1px solid #4cdada;
    }

    .resize-handle::before {
        content: "";
        display: block;
        position: absolute;
        top: 0px;
        bottom: 0px;
        right: -8px;
        width: 1rem;
        /* background: pink; */
    }

    .cell-content {
        display: block;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .overflow-indicator {
        position: absolute;
        width: 10px;
        height: 10px;
        pointer-events: none;
    }

    .right-indicator {
        height: 100%;
        width: 4rem;
        top: 0;
        right: 0;
        background: linear-gradient(to right, transparent, rgb(47 47 47));
    }

    .left-indicator {
        height: 100%;
        width: 4rem;
        top: 0;
        left: 0;
        background: linear-gradient(to left, transparent, rgb(47 47 47));
    }
`;
