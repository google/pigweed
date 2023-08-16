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

export enum ConditionType {
  StringSearch,
  ColumnSearch,
  ExactPhraseSearch,
  AndExpression,
  OrExpression,
  NotExpression,
}

export type StringSearchCondition = {
  type: ConditionType.StringSearch;
  searchString: string;
};

export type ColumnSearchCondition = {
  type: ConditionType.ColumnSearch;
  column: string;
  value?: string;
};

export type ExactPhraseSearchCondition = {
  type: ConditionType.ExactPhraseSearch;
  exactPhrase: string;
};

export type AndExpressionCondition = {
  type: ConditionType.AndExpression;
  expressions: FilterCondition[];
};

export type OrExpressionCondition = {
  type: ConditionType.OrExpression;
  expressions: FilterCondition[];
};

export type NotExpressionCondition = {
  type: ConditionType.NotExpression;
  expression: FilterCondition;
};

export type FilterCondition =
  | ColumnSearchCondition
  | StringSearchCondition
  | ExactPhraseSearchCondition
  | AndExpressionCondition
  | OrExpressionCondition
  | NotExpressionCondition;
