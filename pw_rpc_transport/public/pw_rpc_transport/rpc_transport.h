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
#pragma once

#include "pw_bytes/span.h"
#include "pw_function/function.h"
#include "pw_status/status.h"

namespace pw::rpc {

// pw_rpc transport layer interfaces.

// Framed RPC data ready to be sent via `RpcFrameSender`. Consists of a header
// and a payload. Some RPC transport encodings may not require a header and put
// all of the framed data into the payload (in which case the header can be
// an empty span).
//
// A single RPC packet can be split into multiple RpcFrame's depending on the
// MTU of the transport.
//
// All frames for an RPC packet are expected to be sent and received in order
// without being interleaved by other packets' frames.
struct RpcFrame {
  ConstByteSpan header;
  ConstByteSpan payload;
};

// RpcFrameSender encapsulates the details of sending the packet over
// some communication channel (e.g. a hardware mailbox, shared memory, or a
// socket). It exposes its maximum transmission unit (MTU) size and generally
// should know how to send an `RpcFrame` of a size that is smaller or equal than
// the MTU.
class RpcFrameSender {
 public:
  virtual ~RpcFrameSender() = default;
  virtual size_t MaximumTransmissionUnit() const = 0;
  virtual Status Send(RpcFrame frame) = 0;
};

// Gets called by `RpcPacketEncoder` for each frame that it emits.
using OnRpcFrameEncodedCallback = pw::Function<Status(RpcFrame&)>;

// Gets called by `RpcPacketDecoder` for each RPC packet that it detects.
using OnRpcPacketDecodedCallback = pw::Function<void(ConstByteSpan)>;

// RpcPacketEncoder takes an RPC packet, the max frame size, splits the packet
// into frames not exceeding that size and calls the provided callback with
// each produced frame.
template <class Encoder>
class RpcPacketEncoder {
 public:
  Status Encode(ConstByteSpan rpc_packet,
                size_t max_frame_size,
                OnRpcFrameEncodedCallback&& callback) {
    return static_cast<Encoder*>(this)->Encode(
        rpc_packet, max_frame_size, std::move(callback));
  }
};

// RpcPacketDecoder finds and decodes RPC frames in the provided buffer. Once
// all frames for an RPC packet are decoded, the callback is invoked with a
// decoded RPC packet as an argument.
//
// Frames from the same RPC packet are expected to be received in order and
// without being interleaved with frames from any other packets.
template <class Decoder>
class RpcPacketDecoder {
 public:
  Status Decode(ConstByteSpan buffer, OnRpcPacketDecodedCallback&& callback) {
    return static_cast<Decoder*>(this)->Decode(buffer, std::move(callback));
  }
};

// Provides means of sending an RPC packet. A typical implementation ties
// transport and encoder together, although some implementations may not require
// any encoding (e.g. LocalRpcEgress).
class RpcEgressHandler {
 public:
  virtual ~RpcEgressHandler() = default;
  virtual Status SendRpcPacket(ConstByteSpan rpc_packet) = 0;
};

// Provides means of receiving a stream of RPC packets. A typical implementation
// ties transport and decoder together.
class RpcIngressHandler {
 public:
  virtual ~RpcIngressHandler() = default;
  virtual Status ProcessIncomingData(ConstByteSpan buffer) = 0;
};

// A decoded RPC packet is passed to RpcPacketProcessor for further handling.
class RpcPacketProcessor {
 public:
  virtual ~RpcPacketProcessor() = default;
  virtual Status ProcessRpcPacket(ConstByteSpan rpc_packet) = 0;
};
}  // namespace pw::rpc
