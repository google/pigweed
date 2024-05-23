// Copyright 2024 The Pigweed Authors
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

import { TableColumn } from './interfaces';
import { LogViewState } from './state';

export enum NodeType {
  View = 'view',
  Split = 'split',
}

export enum Orientation {
  Horizontal = 'horizontal',
  Vertical = 'vertical',
}

export interface ViewNodeOptions {
  logViewId?: string;
  type?: NodeType;
  orientation?: Orientation;
  children?: ViewNode[];
  columnData?: TableColumn[];
  searchText?: string;
}

/**
 * Represents a log view node in a dynamic layout. This class can function as either
 * a single log view or a container that manages a split between two child views.
 *
 * @class ViewContainer
 * @property {string} logViewId - A unique identifier for the log view.
 * @property {NodeType} type - Specifies whether the node is a single view (NodeType.View) or a container for split views (NodeType.Split). Defaults to NodeType.View.
 * @property {Orientation} orientation - Defines the orientation of the split; defaults to undefined.
 * @property {ViewNode[]} children - An array of child nodes if this is a split type; empty for single view types.
 * @property {LogViewState} logViewState - State properties for the corresponding log view.
 */
export class ViewNode {
  logViewId?: string;
  type: NodeType;
  orientation: Orientation | undefined;
  children: ViewNode[];
  logViewState?: LogViewState;

  constructor(options?: ViewNodeOptions) {
    this.type = options?.type || NodeType.View;
    this.orientation = options?.orientation || undefined;
    this.children = options?.children || [];

    if (this.type === NodeType.View) {
      this.logViewId = options?.logViewId || crypto.randomUUID();
      this.logViewState = {
        columnData: options?.columnData || [],
        searchText: options?.searchText || '',
      };
    }
  }
}
