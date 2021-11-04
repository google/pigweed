// Copyright 2021 The Pigweed Authors
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

/* eslint-env browser */

import {makeStyles, Paper} from '@material-ui/core';
import * as React from 'react';

type Props = {
  lines: string[];
};

const useStyles = makeStyles(() => ({
  root: {},
}));

export function Log(props: Props) {
  const classes = useStyles();

  function row(text: string, index: number) {
    return <div key={index}>{text}</div>;
  }

  return <div className={classes.root}>{props.lines.map(row)}</div>;
}
