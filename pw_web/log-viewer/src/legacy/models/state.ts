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

import { LogField, LogHeadersData, copyHeaderConst } from "./fields";
import { FilterExpression, OrExpression, parseFilter } from "./filter";

// Any non-backwards compatible change to PersistedState must increment this value.
export const CURRENT_VERSION = 1;

export type PersistedState = {
    version: number;
    filter: string;
    fields: LogHeadersData;
    wrappingLogs: boolean;
};

/**
 * Returns an empty `PersistedState`.
 */
function emptyState(): PersistedState {
    return {
        version: CURRENT_VERSION,
        filter: "",
        fields: copyHeaderConst(),
        wrappingLogs: true,
    };
}

interface DataStore {
    setState(state: PersistedState): void;
    getState(): PersistedState;
}

export class MemoryStore implements DataStore {
    constructor(private state: PersistedState = emptyState()) {}
    setState(newState: PersistedState) {
        this.state = newState;
    }

    getState(): PersistedState {
        return this.state;
    }
}

/**
 * Wrapper around the state encapsulating common operations.
 *
 * Anything stored through `vscode.setState` will be persisted across editor restarts.
 */
export class State extends EventTarget {
    private current: PersistedState = emptyState();
    private currentFilterExpression: FilterExpression = new OrExpression([]);

    /**
     * Create a new state object. This will load the previously set filters from storage.
     */
    constructor(public store: DataStore = new MemoryStore()) {
        super();
        this.reset();
    }

    /**
     * Whether or not the wrap logs option is set.
     */
    public get shouldWrapLogs(): boolean {
        return this.current.wrappingLogs;
    }

    /**
     * Updates the set wrap logs option.
     */
    public set shouldWrapLogs(shouldWrap: boolean) {
        this.current.wrappingLogs = shouldWrap;
        this.store.setState(this.current);
    }

    /**
     * Keep track of width of the resized columns.
     * @param field field of targeted column
     * @param width attribute to be stored in state
     */
    public setHeaderWidth(header: LogField, width: string) {
        this.current.fields[header].width = width;
        this.store.setState(this.current);
    }

    /**
     * Updates the list of active filters with the given new set of filters.
     * @param filters new filters to add to the current set of filters.
     */
    public registerFilter(expression: FilterExpression, filterText: string) {
        this.currentFilterExpression = expression;
        this.current.filter = filterText;
        this.store.setState(this.current);
        this.dispatchEvent(
            new CustomEvent("filterChange", {
                detail: {
                    filter: this.currentFilterExpression,
                },
            })
        );
    }

    get currentFields(): LogHeadersData {
        return this.current.fields;
    }

    get currentFilter(): FilterExpression {
        return this.currentFilterExpression;
    }

    get currentFilterText(): string {
        return this.current.filter;
    }

    /**
     * Loads the persisted state into the current state.
     */
    public reset() {
        this.current = this.currentPersistentState();
        if (
            this.current.filter === undefined ||
            this.current.filter === null ||
            this.current.filter === ""
        ) {
            this.currentFilterExpression = new OrExpression([]);
            this.current.filter = "";
        } else {
            const parseResult = parseFilter(this.current.filter);
            if (parseResult.ok) {
                this.currentFilterExpression = parseResult.value;
            } else {
                this.currentFilterExpression = new OrExpression([]);
            }
        }
    }

    private currentPersistentState(): PersistedState {
        const state = this.store.getState();
        if (state === undefined || state?.version !== CURRENT_VERSION) {
            return emptyState();
        }
        return {
            version: state.version ?? CURRENT_VERSION,
            filter: state.filter ?? "",
            fields: state.fields ?? copyHeaderConst(),
            wrappingLogs: state.wrappingLogs ?? true,
        };
    }
}
