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
import {makeStyles, Box} from '@material-ui/core';
import * as React from 'react';
import {Status} from '@pigweed/pw_status';
import {Message} from 'google-protobuf';
import {Call} from '@pigweed/pw_rpc';

type Props = {
  calls: Call[];
};

const useStyles = makeStyles(() => ({
  root: {
    padding: '8px',
    'background-color': '131416',
    height: '300px',
    'overflow-y': 'auto',
    width: '100%',
    color: 'white',
  },
}));

export function RpcPane(props: Props) {
  const classes = useStyles();

  function row(call: Call, index: number) {
    return (
      <Box key={index} sx={{fontFamily: 'Monospace'}}>
        {call.rpc.service.name}.{call.rpc.method.name}
        \n---
        {call.completed ? Status[call.status!] : 'In progress...'}
      </Box>
    );
  }

  return <div className={classes.root}>{props.calls.map(row)}</div>;
}
