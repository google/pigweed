// Copyright 2020 The Pigweed Authors
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
import {FrameStatus, Frame} from '@pigweed/pw_hdlc';

type Props = {
  frames: Frame[];
};

const useStyles = makeStyles(() => ({
  root: {
    padding: '8px',
    'background-color': '#131416',
    height: '500px',
    'overflow-y': 'auto',
    width: '100%',
    color: 'white',
  },
}));

export function SerialLog(props: Props) {
  const classes = useStyles();
  const decoder = new TextDecoder();

  // TODO(b/199515206): Display HDLC packets in user friendly manner.
  //
  // See the python console serial debug window for reference.
  function row(frame: Frame, index: number) {
    let rowText = '';
    if (frame.status === FrameStatus.OK) {
      rowText = decoder.decode(frame.data);
    } else {
      rowText = `[${frame.rawDecoded}]`;
    }

    return (
      <div key={index}>
        {frame.status}: {rowText}
      </div>
    );
  }

  return (
    <Paper className={classes.root}>
      <Box sx={{fontFamily: 'Monospace'}}>
        {props.frames.map((frame: Frame, index: number) => row(frame, index))}
      </Box>
    </Paper>
  );
}
