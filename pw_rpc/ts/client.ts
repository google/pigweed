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

/** Provides a pw_rpc client for TypeScript. */

import {Library} from 'pigweed/pw_protobuf_compiler/ts/proto_lib';

import {Channel, Service} from './descriptors';
import {MethodStub, methodStubFactory} from './method';
import {PendingCalls} from './rpc_classes';

/**
 * Object for managing RPC service and contained methods.
 */
class ServiceClient {
  private service: Service;
  private methods: MethodStub[] = [];
  private methodsByName = new Map<string, MethodStub>();

  constructor(client: Client, channel: Channel, service: Service) {
    this.service = service;
    const methods = service.methods;
    methods.forEach((method) => {
      const stub = methodStubFactory(client.rpcs, channel, method);
      this.methods.push(stub);
      this.methodsByName.set(method.name, stub);
    });
  }

  method(methodName: string): MethodStub|undefined {
    return this.methodsByName.get(methodName);
  }
}

/**
 * Object for managing RPC channel and contained services.
 */
class ChannelClient {
  readonly channel: Channel;
  private services = new Map<string, ServiceClient>();

  constructor(client: Client, channel: Channel, services: Service[]) {
    this.channel = channel;
    services.forEach((service) => {
      const serviceClient = new ServiceClient(client, this.channel, service);
      this.services.set(service.name, serviceClient);
    });
  }

  private service(serviceName: string): ServiceClient|undefined {
    return this.services.get(serviceName);
  }

  /**
   * Find a method stub via its full name.
   *
   * For example:
   * `method = client.channel().methodStub('the.package.AService.AMethod');`
   *
   */
  methodStub(name: string): MethodStub|undefined {
    const index = name.lastIndexOf('.');
    if (index <= 0) {
      console.error(`Malformed method name: ${name}`);
      return undefined;
    }
    const serviceName = name.slice(0, index);
    const methodName = name.slice(index + 1);
    const method = this.service(serviceName)?.method(methodName);
    if (method === undefined) {
      console.error(`Method not found: ${name}`);
      return undefined;
    }
    return method;
  }
}

/**
 * RPCs are invoked through a MethodStub. These can be found by name via
 * methodStub(string name).
 *
 * ```
 * method = client.channel(1).methodStub('the.package.FooService.SomeMethod')
 * call = method.invoke(request);
 * ```
 */
export class Client {
  private services = new Map<number, Service>();
  private channelsById = new Map<number, ChannelClient>();
  readonly rpcs: PendingCalls;

  constructor(channels: Channel[], services: Service[]) {
    this.rpcs = new PendingCalls();
    services.forEach((service) => {
      this.services.set(service.id, service);
    });

    channels.forEach((channel) => {
      this.channelsById.set(
          channel.id, new ChannelClient(this, channel, services));
    });
  }

  /**
   * Creates a client from a set of Channels and a library of Protos.
   *
   * @param {Channel[]} channels List of possible channels to use.
   * @param {Library} protoSet Library containing protos defining RPC services
   * and methods.
   */
  static fromProtoSet(channels: Channel[], protoSet: Library): Client {
    let services: Service[] = [];
    const descriptors = protoSet.fileDescriptorSet.getFileList();
    descriptors.forEach((fileDescriptor) => {
      const packageName = fileDescriptor.getPackage()!;
      fileDescriptor.getServiceList().forEach((serviceDescriptor) => {
        services = services.concat(
            new Service(serviceDescriptor, protoSet, packageName));
      });
    });

    return new Client(channels, services)
  }

  /**
   * Finds the channel with the provided id. Returns undefined if there are no
   * channels or no channel with a matching id.
   *
   * @param {number?} id If no id is specified, returns the first channel.
   */
  channel(id?: number): ChannelClient|undefined {
    if (id === undefined) {
      return this.channelsById.values().next().value;
    }
    return this.channelsById.get(id);
  }
}
