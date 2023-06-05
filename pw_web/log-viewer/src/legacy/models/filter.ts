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

import * as parser from "../../assets/filters.js";

// NOTE: this type must match the definitions in the PEG.js parser.
export type Category =
    | "any"
    | "moniker"
    | "tag"
    | "package-name"
    | "manifest"
    | "message"
    | "time"
    | "severity"
    | "pid"
    | "tid"
    | "custom";

// NOTE: this type must match the definitions in the PEG.js parser.
export type Operator =
    | "contains"
    | "equal"
    | "greater"
    | "greaterEq"
    | "less"
    | "lessEq";

const severityMap: Record<string, number> = {
    trace: 0,
    debug: 1,
    info: 2,
    warn: 3,
    error: 4,
    fatal: 5,
};

// Categories which the `any` filter will apply to.
const anyCategories: Category[] = [
    "moniker",
    "tag",
    "package-name",
    "manifest",
    "message",
];

export type Result<T, E = Error> =
    | { ok: true; value: T }
    | { ok: false; error: E };

/**
 * Interface impleemnted by all expression types that can evaluate a log.
 */
export interface FilterExpression {
    /**
     * Whether or not the log should be filtered.
     * @param el the element containing the log data to evalute
     */
    accepts(el: Element): boolean;

    /**
     * Whether or not the filter evalutes nothing.
     */
    isEmpty(): boolean;
}

/**
 * A parsed "and" expression.
 */
export class AndExpression implements FilterExpression {
    constructor(private items: FilterExpression[]) {}

    accepts(el: Element): boolean {
        return this.isEmpty() || this.items.every((item) => item.accepts(el));
    }

    isEmpty(): boolean {
        return this.items.length === 0;
    }
}

/**
 * A parsed "or" expression.
 */
export class OrExpression implements FilterExpression {
    constructor(private items: FilterExpression[]) {}

    accepts(el: Element): boolean {
        return this.isEmpty() || this.items.some((item) => item.accepts(el));
    }

    isEmpty(): boolean {
        return this.items.length === 0;
    }
}

/**
 * A parsed "not" expression.
 */
export class NotExpression implements FilterExpression {
    constructor(private item: FilterExpression) {}

    accepts(el: Element): boolean {
        return !this.item.accepts(el);
    }

    isEmpty(): boolean {
        return this.item.isEmpty();
    }
}

export class NoTimestampError extends Error {
    public filter: Filter;
    constructor(filter: Filter) {
        super("No reference timestamp available from device");
        this.filter = filter;
    }
}

/**
 * Represents a single filter and provides the necessary functionality for evaluating logs.
 */
export class Filter implements FilterExpression {
    // The category that this filter applies to.
    readonly category: Category;

    // The sub category that this filter applies to. This is only used for custom keys.
    readonly subCategory: string | undefined;

    // The operator invoked by this filter.
    readonly operator: Operator;

    // The value that the operator will be applied to.
    readonly value: boolean | number | string | RegExp;

    // The most recent monotonic timestamp from the device.
    public nowTimestampFromDevice?: number;

    constructor(args: {
        category: Category;
        subCategory: string | undefined;
        operator: Operator;
        value: boolean | number | string | RegExp;
    }) {
        this.category = args.category;
        this.subCategory = args.subCategory;
        this.operator = args.operator;
        this.value = args.value;

        // Contains operator works without case sensitivity.
        if (this.operator === "contains" && typeof this.value === "string") {
            this.value = this.value.toLowerCase();
        }
    }

    public accepts(el: Element): boolean {
        switch (this.category) {
            case "any":
                return this.evalAny(el);
            case "custom":
                return this.evalCustom(el);
            case "severity":
                return this.evalSeverity(el);
            case "tag":
                return this.evalTag(el);
            case "manifest":
            case "message":
            case "moniker":
            case "package-name":
                return this.evalGeneral(el);
            case "pid":
            case "tid":
                return this.evalPidTid(this.category, el);
            case "time":
                if (!this.nowTimestampFromDevice) {
                    throw new NoTimestampError(this);
                }
                return this.evalTimestamp(el, this.nowTimestampFromDevice);
        }
    }

    isEmpty(): boolean {
        return false;
    }

    private evalSeverity(el: Element): boolean {
        const query = el.getAttribute("data-severity");
        if (!query) {
            return false;
        }
        const severity = this.value as string;
        switch (this.operator) {
            // Contains on severity works as "minimum severity" so the same as ">=".
            case "contains":
            case "greaterEq":
                return severityMap[query] >= severityMap[severity];
            case "lessEq":
                return severityMap[query] <= severityMap[severity];
            case "less":
                return severityMap[query] < severityMap[severity];
            case "greater":
                return severityMap[query] > severityMap[severity];
            case "equal":
                return severityMap[query] === severityMap[severity];
        }
    }

    private evalAny(el: Element): boolean {
        return (
            anyCategories.some((category) => {
                if (category === "tag") {
                    return this.evalTag(el);
                } else {
                    return this.evalGeneralHelper(category, el);
                }
            }) ||
            el.getAttributeNames().some((attributeName) => {
                return (
                    attributeName.startsWith("data-custom-") &&
                    this.evalCustomHelper(attributeName, el)
                );
            })
        );
    }

    private evalTag(el: Element): boolean {
        const totalTagsStr = el.getAttribute("data-tags");
        if (!totalTagsStr) {
            return false;
        }
        const totalTags = parseInt(totalTagsStr);
        for (let i = 0; i < totalTags; i++) {
            const tag = el.getAttribute(`data-tag-${i}`);
            if (
                evalStringOp(tag!, this.operator, this.value as string | RegExp)
            ) {
                return true;
            }
        }
        return false;
    }

    evalPidTid(category: "pid" | "tid", el: Element): boolean {
        const query = el.getAttribute(`data-${category}`);
        if (!query) {
            return false;
        }
        return evalNumberOp(
            parseInt(query),
            this.operator,
            this.value as number
        );
    }

    evalTimestamp(el: Element, nowDeviceMonotonic: number): boolean {
        const query = el.getAttribute("data-timestamp");
        if (!query) {
            return false;
        }
        const timeDiff = nowDeviceMonotonic - parseFloat(query);
        return evalNumberOp(timeDiff, this.operator, this.value as number);
    }

    private evalCustom(el: Element): boolean {
        if (this.subCategory === undefined) {
            return false;
        }
        return this.evalCustomHelper(`data-custom-${this.subCategory}`, el);
    }

    private evalCustomHelper(attributeName: string, el: Element) {
        const query = el.getAttribute(attributeName);
        if (!query) {
            return false;
        }
        if (
            typeof this.value === "object" &&
            this.value.constructor.name === "RegExp"
        ) {
            return evalRegex(query, this.operator, this.value);
        } else if (typeof this.value === "string") {
            return evalStringOp(query, this.operator, this.value as string);
        } else if (typeof this.value === "boolean") {
            return evalBoolOp(
                query === "true",
                this.operator,
                this.value as boolean
            );
        } else if (typeof this.value === "number") {
            return evalNumberOp(
                parseInt(query),
                this.operator,
                this.value as number
            );
        } else {
            console.log(`Unrecognized type for a value: ${typeof this.value}`);
            return false;
        }
    }

    private evalGeneral(el: Element): boolean {
        return this.evalGeneralHelper(this.category, el);
    }

    private evalGeneralHelper(category: Category, el: Element): boolean {
        const query = el.getAttribute(`data-${category}`);
        if (!query) {
            return false;
        }
        return evalStringOp(
            query!,
            this.operator,
            this.value as string | RegExp
        );
    }
}

/**
 * Executes a string logical operation on two values.
 *
 * @param lhs left hand side value of the operation.
 * @param op  the operation to apply for both strings.
 * @param rhs  right hand side value of the operation.
 * @returns the result of the operation.
 */
function evalStringOp(
    lhs: string,
    op: Operator,
    rhs: string | RegExp
): boolean {
    if (typeof rhs !== "string") {
        return evalRegex(lhs, op, rhs);
    }
    switch (op) {
        case "contains":
            return lhs.toLowerCase().includes(rhs);
        case "equal":
            return lhs === rhs;
        default:
            return false;
    }
}

/**
 * Executes a comparison operation on two booleans.
 *
 * @param lhs left hand side value of the operation.
 * @param op the operation to apply for both booleans.
 * @param rhs right hand side value of the operation.
 * @returns the result of the operation.
 */
function evalBoolOp(lhs: boolean, op: Operator, rhs: boolean): boolean {
    switch (op) {
        case "contains":
        case "equal":
            return lhs === rhs;
        default:
            return false;
    }
}

/**
 * Executes a comparison operation on two numbers.
 *
 * @param lhs left hand side value of the operation.
 * @param op the operation to apply to both booleans.
 * @param rhs right hand side of the operation.
 * @returns the result of the operation.
 */
function evalNumberOp(lhs: number, op: Operator, rhs: number): boolean {
    switch (op) {
        case "contains":
        case "equal":
            // This allows to perform comparisons such as "3" == 3 allowing to remove the need of doing
            // parseInt.
            // eslint-disable-next-line eqeqeq
            return lhs == rhs;
        case "greater":
            return lhs > rhs;
        case "greaterEq":
            return lhs >= rhs;
        case "less":
            return lhs < rhs;
        case "lessEq":
            return lhs <= rhs;
    }
}

/**
 * Matches the given `lhs` value with a regular expression with the given operator.
 *
 * @param lhs the string to match
 * @param op operator (equals or contains)
 * @param rhs the regular expression to match against
 * @return true if the operator is contains and the string matches. If the operator is equals, then
 * the matched string also must be equal to `lhs` for the return to be true.
 */
function evalRegex(lhs: string, op: Operator, rhs: RegExp): boolean {
    const m = lhs.match(rhs);
    switch (op) {
        case "contains":
            return m !== null;
        case "equal":
            return m !== null && m[0] === lhs;
        default:
            return false;
    }
}

/**
 * Parses a string specifying a filter expression into a structured expression that
 * can be evaluated.
 *
 * @param text input text to parse
 * @returns a result containing the parsed filter or an error message if the
 *     filter couldn't be parsed
 */
export function parseFilter(text: string): Result<FilterExpression, string> {
    try {
        const expression = parser.parse(
            // eslint-disable-next-line @typescript-eslint/naming-convention
            text,
            { OrExpression, AndExpression, NotExpression, Filter: Filter }
        ) as FilterExpression;
        return {
            ok: true,
            value: expression,
        };
    } catch (error) {
        return { ok: false, error: `${error}` };
    }
}
