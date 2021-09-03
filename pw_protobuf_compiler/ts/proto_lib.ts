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

/** Tools for compiling and importing Javascript protos on the fly. */

import * as fs from 'fs';
import {Message} from 'google-protobuf';
import {FileDescriptorSet} from 'google-protobuf/google/protobuf/descriptor_pb';

export type MessageCreator = new () => Message;
class MessageMap extends Map<string, MessageCreator> {}
class ModuleMap extends Map<string, any> {}

/**
 * A collection of protocol buffer modules
 */
export class Library {
  readonly fileDescriptorSet: FileDescriptorSet;
  private messages: MessageMap;

  /**
   * Primary way to construct a Library.
   *
   * For example,
   * ```
   * const path = 'pw_rpc/ts/test_protos-descriptor-set.proto.bin'
   * const lib = Library.fromFileDescriptorSet(path, 'test_protos_tspb');
   * ```
   * The example path is for a proto_library bazel rule named 'test_protos'.
   *
   * @param path String location of the FileDescriptorSet binary.
   * @param protoLibName: String name for the js_proto_library dependency. This
   * is used to map from fileDescriptor name to compiled proto path.
   */
  static async fromFileDescriptorSet(path: string, protoLibName: string):
      Promise<Library> {
    const binary = fs.readFileSync(path);
    const fileDescriptorSet = FileDescriptorSet.deserializeBinary(binary);

    let mods = new ModuleMap();
    for (const file of fileDescriptorSet.getFileList()) {
      const moduleName = this.buildModuleName(file.getName()!, protoLibName);
      let mod = await import(moduleName);
      const key = file.getName()!;
      mods.set(key, mod);
    }

    return new Library(fileDescriptorSet, mods)
  }

  /**
   * Maps from a file and js_proto_library name to a module path.
   *
   * For example the following rules would correspond with the call
   * `buildModuleName('dir/test.proto', 'test_proto_tspb')`
   *
   *  proto_library(
   *      name = "test_proto",
   *      srcs = [
   *          "dir/test.proto",
   *      ],
   *  )

   *  js_proto_library(
   *      name = "test_proto_tspb",
   *      protos = [":test_protos"],
   *  )
   *
   * @param {string} fileName Proto file in the form 'root/dir/test.proto'
   * @param {string} protoLibName Name attribute of the relevant
   *   js_proto_library rule.
   */
  private static buildModuleName(fileName: string, protoLibName: string):
      string {
    const name = `${protoLibName}/${protoLibName}_pb/${fileName}`;
    return name.replace(/\.proto$/, '_pb');
  }

  constructor(set: FileDescriptorSet, mods: ModuleMap) {
    this.fileDescriptorSet = set;
    this.messages = this.mapMessages(set, mods);
  }

  /**
   * Creates a map between message identifier "{packageName}.{messageName}"
   * and the Message class.
   */
  private mapMessages(set: FileDescriptorSet, mods: ModuleMap): MessageMap {
    const messages = new MessageMap();
    for (const fileDescriptor of set.getFileList()) {
      const mod = mods.get(fileDescriptor.getName()!)!;
      for (const messageType of fileDescriptor.getMessageTypeList()) {
        const fullName =
            fileDescriptor.getPackage()! + '.' + messageType.getName();
        const message = mod[messageType.getName()!];
        messages.set(fullName, message);
      }
    }
    return messages;
  }

  /**
   * Finds the Message class referenced by the identifier.
   *
   *  @param identifier String identifier of the form
   *  "{packageName}.{messageName}" i.e: "pw.rpc.test.NewMessage".
   */
  getMessageCreator(identifier: string): MessageCreator|undefined {
    return this.messages.get(identifier);
  }
}
