// Copyright 2022 The Pigweed Authors
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

import { exec, ExecException } from 'child_process';
import fs from 'fs';
import path from 'path';
import generateTemplate from './codegen/template_replacement';
const googProtobufPath = require.resolve('google-protobuf');
const googProtobufModule = fs.readFileSync(googProtobufPath, 'utf-8');

const run = function (
  executable: string,
  args: string[],
  cwd: string = process.cwd(),
) {
  return new Promise<void>((resolve) => {
    exec(
      `${executable} ${args.join(' ')}`,
      { cwd },
      (error: ExecException | null, stdout: string | Buffer) => {
        if (error) {
          throw error;
        }

        console.log(stdout);
        resolve();
      },
    );
  });
};

function getRealPathOfSymlink(path: string) {
  const stats = fs.statSync(path);
  if (stats.isSymbolicLink()) {
    return fs.realpathSync(path);
  } else {
    return path;
  }
}

const protoc = async function (
  protos: string[],
  outDir: string,
  cwd: string = process.cwd(),
) {
  const PROTOC_GEN_TS_PATH = getRealPathOfSymlink(
    path.resolve(
      path.dirname(require.resolve('ts-protoc-gen/generate.js')),
      'bin',
      'protoc-gen-ts',
    ),
  );

  const protocBinary = require.resolve('@protobuf-ts/protoc/protoc.js');

  await run(
    protocBinary,
    [
      `--plugin="protoc-gen-ts=${PROTOC_GEN_TS_PATH}"`,
      `--descriptor_set_out=${path.join(outDir, 'descriptor.bin')}`,
      `--js_out=import_style=commonjs,binary:${outDir}`,
      `--ts_out=${outDir}`,
      `--proto_path=${cwd}`,
      ...protos,
    ],
    cwd,
  );

  // ES6 workaround: Replace google-protobuf imports with entire library.
  protos.forEach((protoPath) => {
    const outPath = path.join(outDir, protoPath.replace('.proto', '_pb.js'));

    if (fs.existsSync(outPath)) {
      let data = fs.readFileSync(outPath, 'utf8');
      data = data.replace(
        "var jspb = require('google-protobuf');",
        googProtobufModule,
      );
      data = data.replace('var goog = jspb;', '');
      fs.writeFileSync(outPath, data);
    }
  });
};

const makeProtoCollection = function (
  descriptorBinPath: string,
  protoPath: string,
  outputCollectionName: string,
) {
  generateTemplate(`${protoPath}/${outputCollectionName}`, descriptorBinPath);
};

export function buildProtos(
  protos: string[],
  outDir: string,
  outputCollectionName = 'collection.js',
  cwd: string = process.cwd(),
) {
  protoc(protos, outDir, cwd).then(() => {
    makeProtoCollection(
      path.join(outDir, 'descriptor.bin'),
      outDir,
      outputCollectionName,
    );
  });
}
