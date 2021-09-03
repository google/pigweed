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

import {Message} from 'google-protobuf';
import {MethodDescriptorProto, ServiceDescriptorProto} from 'google-protobuf/google/protobuf/descriptor_pb';
import {Library} from 'pigweed/pw_protobuf_compiler/ts/proto_lib';

import {hash} from './hash';

/** Describes an RPC service. */
export class Service {
  readonly name: string;
  readonly id: number;
  readonly methods = new Map<string, Method>();

  constructor(descriptor: ServiceDescriptorProto, protoLibrary: Library) {
    this.name = descriptor.getName()!;
    this.id = hash(this.name);
    descriptor.getMethodList().forEach(
        (methodDescriptor: MethodDescriptorProto) => {
          const method = new Method(methodDescriptor, protoLibrary, this);
          this.methods.set(method.name, method);
        });
  }
}

/** Describes an RPC method. */
export class Method {
  readonly service: Service;
  readonly name: string;
  readonly id: number;
  readonly clientStreaming: boolean;
  readonly serverStreaming: boolean;
  readonly inputType: any;
  readonly outputType: any;

  constructor(
      descriptor: MethodDescriptorProto,
      protoLibrary: Library,
      service: Service) {
    this.name = descriptor.getName()!;
    this.id = hash(this.name);
    this.service = service;
    this.serverStreaming = descriptor.getServerStreaming()!;
    this.clientStreaming = descriptor.getClientStreaming()!;

    const inputTypePath = descriptor.getInputType()!;
    const outputTypePath = descriptor.getOutputType()!;

    // Remove leading period if it exists.
    this.inputType =
        protoLibrary.getMessageCreator(inputTypePath.replace(/^\./, ''))!;
    this.outputType =
        protoLibrary.getMessageCreator(outputTypePath.replace(/^\./, ''))!;
  }
}
