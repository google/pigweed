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

import {makeStyles, Paper, Box} from '@material-ui/core';
import * as React from 'react';
import {default as AnsiUp} from 'ansi_up';
import * as Parser from 'html-react-parser';

type Props = {
  lines: string[];
};

const useStyles = makeStyles(() => ({
  root: {
    padding: '8px',
    'background-color': 'black',
    height: '500px',
    'overflow-y': 'auto',
    width: '100%',
    color: 'white',
  },
}));

export function Log(props: Props) {
  const classes = useStyles();
  const xtermRef = React.useRef(null);
  const ansiUp = new AnsiUp();

  function row(text: string, index: number) {
    const textHtml = ansiUp.ansi_to_html(text);
    return (
      <Box key={index} sx={{fontFamily: 'Monospace'}}>
        {Parser.default(textHtml)}
      </Box>
    );
  }

  return <div className={classes.root}>{props.lines.map(row)}</div>;
}
