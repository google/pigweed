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

import {
  MessageCreator,
  ProtoCollection,
} from 'pigweedjs/pw_protobuf_compiler';
import { Message } from 'google-protobuf';
import {
  MethodDescriptorProto,
  ServiceDescriptorProto,
} from 'google-protobuf/google/protobuf/descriptor_pb';

import { hash } from './hash';

interface ChannelOutput {
  (data: Uint8Array): void;
}

export class Channel {
  readonly id: number;
  private output: ChannelOutput;

  constructor(
    id: number,
    output: ChannelOutput = () => {
      /* do nothing. */
    },
  ) {
    this.id = id;
    this.output = output;
  }

  send(data: Uint8Array) {
    this.output(data);
  }
}

/** Describes an RPC service. */
export class Service {
  readonly name: string;
  readonly id: number;
  readonly methods = new Map<number, Method>();
  readonly methodsByName = new Map<string, Method>();

  constructor(serviceName: string, methodsList: RPCMethodDescriptor[]) {
    this.name = serviceName;
    this.id = hash(this.name);
    methodsList.forEach((methodDescriptor) => {
      const method = new Method(this, methodDescriptor);
      this.methods.set(method.id, method);
      this.methodsByName.set(method.name, method);
    });
  }
  static fromProtoDescriptor(
    descriptor: ServiceDescriptorProto,
    protoCollection: ProtoCollection,
    packageName: string,
  ) {
    const name = packageName + '.' + descriptor.getName()!;
    const methodList = descriptor
      .getMethodList()
      .map((methodDescriptor: MethodDescriptorProto) =>
        Method.protoDescriptorToRPCMethodDescriptor(
          methodDescriptor,
          protoCollection,
        ),
      );
    return new Service(name, methodList);
  }
}

export type MessageSerializer = {
  serialize: (message: Message) => Uint8Array;
  deserialize: (bytes: Uint8Array) => Message;
};

export type RPCMethodDescriptor = {
  name: string;
  requestType: MessageCreator;
  responseType: MessageCreator;
  customRequestSerializer?: MessageSerializer;
  customResponseSerializer?: MessageSerializer;
  clientStreaming?: boolean;
  serverStreaming?: boolean;
  protoDescriptor?: MethodDescriptorProto;
};

export enum MethodType {
  UNARY,
  SERVER_STREAMING,
  CLIENT_STREAMING,
  BIDIRECTIONAL_STREAMING,
}

/** Describes an RPC method. */
export class Method {
  readonly service: Service;
  readonly name: string;
  readonly id: number;
  readonly clientStreaming: boolean;
  readonly serverStreaming: boolean;
  readonly requestType: any;
  readonly responseType: any;
  readonly descriptor?: MethodDescriptorProto;
  readonly customRequestSerializer?: MessageSerializer;
  readonly customResponseSerializer?: MessageSerializer;

  constructor(service: Service, methodDescriptor: RPCMethodDescriptor) {
    this.name = methodDescriptor.name;
    this.id = hash(this.name);
    this.service = service;
    this.serverStreaming = methodDescriptor.serverStreaming || false;
    this.clientStreaming = methodDescriptor.clientStreaming || false;

    this.requestType = methodDescriptor.requestType;
    this.responseType = methodDescriptor.responseType;
    this.descriptor = methodDescriptor.protoDescriptor;
    this.customRequestSerializer = methodDescriptor.customRequestSerializer;
    this.customResponseSerializer = methodDescriptor.customResponseSerializer;
  }

  static protoDescriptorToRPCMethodDescriptor(
    descriptor: MethodDescriptorProto,
    protoCollection: ProtoCollection,
  ): RPCMethodDescriptor {
    const requestTypePath = descriptor.getInputType()!;
    const responseTypePath = descriptor.getOutputType()!;

    // Remove leading period if it exists.
    const requestType = protoCollection.getMessageCreator(
      requestTypePath.replace(/^\./, ''),
    )!;
    const responseType = protoCollection.getMessageCreator(
      responseTypePath.replace(/^\./, ''),
    )!;

    return {
      name: descriptor.getName()!,
      serverStreaming: descriptor.getServerStreaming()!,
      clientStreaming: descriptor.getClientStreaming()!,
      requestType: requestType,
      responseType: responseType,
      protoDescriptor: descriptor,
    };
  }

  static fromProtoDescriptor(
    descriptor: MethodDescriptorProto,
    protoCollection: ProtoCollection,
    service: Service,
  ) {
    return new Method(
      service,
      Method.protoDescriptorToRPCMethodDescriptor(descriptor, protoCollection),
    );
  }

  get type(): MethodType {
    if (this.clientStreaming && this.serverStreaming) {
      return MethodType.BIDIRECTIONAL_STREAMING;
    } else if (this.clientStreaming && !this.serverStreaming) {
      return MethodType.CLIENT_STREAMING;
    } else if (!this.clientStreaming && this.serverStreaming) {
      return MethodType.SERVER_STREAMING;
    } else if (!this.clientStreaming && !this.serverStreaming) {
      return MethodType.UNARY;
    }
    throw Error('Unhandled streaming condition');
  }
}
