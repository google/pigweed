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
    background-color: var(--sys-log-viewer-color-table-bg);
    color: var(--sys-log-viewer-color-table-text);
    display: block;
    font-family: 'Roboto Mono', monospace;
    font-size: 1rem;
    height: 100%;
    overflow: hidden;
    position: relative;
  }

  .table-container {
    display: grid;
    height: 100%;
    overflow: scroll;
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
    background-color: var(--sys-log-viewer-color-table-header-bg);
    color: var(--sys-log-viewer-color-table-header-text);
  }

  tr {
    border-bottom: 1px solid var(--sys-log-viewer-color-table-cell-outline);
    display: grid;
    justify-content: flex-start;
    width: 100%;
  }

  .log-row--warning {
    --bg-color: var(--sys-log-viewer-color-surface-yellow);
    --text-color: var(--sys-log-viewer-color-on-surface-yellow);
    --icon-color: var(--sys-log-viewer-color-orange-bright);
  }

  .log-row--error,
  .log-row--critical {
    --bg-color: var(--sys-log-viewer-color-surface-error);
    --text-color: var(--sys-log-viewer-color-on-surface-error);
    --icon-color: var(--sys-log-viewer-color-error-bright);
  }

  .log-row--debug {
    --bg-color: initial;
    --text-color: var(--sys-log-viewer-color-debug);
    --icon-color: var(--sys-log-viewer-color-debug);
  }

  .log-row--warning .cell-content--icon,
  .log-row--error .cell-content--icon,
  .log-row--critical .cell-content--icon {
    color: var(--icon-color);
  }

  .log-row--warning,
  .log-row--error,
  .log-row--critical,
  .log-row--debug {
    background-color: var(--bg-color);
    color: var(--text-color);
  }

  .log-row--nowrap .cell-content {
    overflow: hidden;
    white-space: nowrap;
    text-overflow: ellipsis;
  }

  tr:hover > td {
    background-color: rgba(var(--md-sys-inverse-surface-rgb), 0.05);
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
  td[hidden],
  .jump-to-bottom-btn[hidden] {
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
    align-items: flex-start;
  }

  .jump-to-bottom-btn {
    --md-filled-button-container-elevation: 4;
    --md-filled-button-hover-container-elevation: 4;
    bottom: 2rem;
    left: 50%;
    position: absolute;
    transform: translate(-50%);
  }

  .resize-handle {
    background-color: var(--sys-log-viewer-color-table-cell-outline);
    bottom: 0;
    content: '';
    cursor: col-resize;
    height: 100%;
    left: 0;
    mix-blend-mode: luminosity;
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
    mix-blend-mode: unset;
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

  .cell-content--icon {
    padding-top: 0.125rem;
  }

  .cell-icon {
    display: block;
    font-variation-settings:
      'FILL' 1,
      'wght' 400,
      'GRAD' 200,
      'opsz' 58;
    user-select: none;
  }

  .overflow-indicator {
    height: 100%;
    pointer-events: none;
    position: absolute;
    width: 8rem;
  }

  .bottom-indicator {
    align-self: flex-end;
    background: linear-gradient(
      to bottom,
      transparent,
      var(--sys-log-viewer-color-overflow-indicator)
    );
    height: 8rem;
    pointer-events: none;
    position: absolute;
    width: 100%;
  }

  .left-indicator {
    background: linear-gradient(
      to left,
      transparent,
      var(--sys-log-viewer-color-overflow-indicator)
    );
    justify-self: flex-start;
  }

  .right-indicator {
    background: linear-gradient(
      to right,
      transparent,
      var(--sys-log-viewer-color-overflow-indicator)
    );
    justify-self: flex-end;
  }

  mark {
    background-color: var(--sys-log-viewer-color-table-mark);
    border-radius: 2px;
    color: var(--md-sys-color-on-primary-container);
    outline: 1px solid var(--sys-log-viewer-color-table-mark);
  }

  ::-webkit-scrollbar {
    -webkit-appearance: auto;
  }

  ::-webkit-scrollbar-corner {
    background: var(--md-sys-color-surface-container-low);
  }

  ::-webkit-scrollbar-thumb {
    min-height: 3rem;
  }

  ::-webkit-scrollbar-thumb:horizontal {
    border-radius: 20px;
    box-shadow: inset 0 0 2rem 2rem var(--md-sys-color-outline-variant);
    border: inset 3px transparent;
    border-top: inset 4px transparent;
  }

  ::-webkit-scrollbar-thumb:vertical {
    border-radius: 20px;
    box-shadow: inset 0 0 2rem 2rem var(--md-sys-color-outline-variant);
    border: inset 3px transparent;
    border-left: inset 4px transparent;
  }

  ::-webkit-scrollbar-track:horizontal {
    box-shadow: inset 0 0 2rem 2rem var(--md-sys-color-surface-container-low);
    border-top: solid 1px var(--md-sys-color-outline-variant);
  }

  ::-webkit-scrollbar-track:vertical {
    box-shadow: inset 0 0 2rem 2rem var(--md-sys-color-surface-container-low);
    border-left: solid 1px var(--md-sys-color-outline-variant);
  }
`;
