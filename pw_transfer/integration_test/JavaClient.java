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

import com.google.common.collect.ImmutableList;
import com.google.protobuf.TextFormat;
import dev.pigweed.pw_hdlc.Decoder;
import dev.pigweed.pw_hdlc.Encoder;
import dev.pigweed.pw_hdlc.Frame;
import dev.pigweed.pw_log.Logger;
import dev.pigweed.pw_rpc.Channel;
import dev.pigweed.pw_rpc.ChannelOutputException;
import dev.pigweed.pw_rpc.Client;
import dev.pigweed.pw_transfer.Manager;
import dev.pigweed.pw_transfer.TransferParameters;
import dev.pigweed.pw_transfer.TransferService;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import pw.transfer.ConfigProtos;

public class JavaClient {
  private static final String SERVICE = "pw.transfer.Transfer";
  private static final Logger logger = Logger.forClass(Client.class);

  private static final int CHANNEL_ID = 1;
  private static final long RPC_HDLC_ADDRESS = 'R';
  private static final String HOSTNAME = "localhost";

  private HdlcRpcChannelOutput channelOutput;
  private Client rpcClient;
  private HdlcParseThread parseThread;

  public JavaClient(OutputStream writer, InputStream reader) {
    this.channelOutput = new HdlcRpcChannelOutput(writer, RPC_HDLC_ADDRESS);

    this.rpcClient = Client.create(ImmutableList.of(new Channel(CHANNEL_ID, this.channelOutput)),
        ImmutableList.of(TransferService.get()));

    this.parseThread = new HdlcParseThread(reader, this.rpcClient);
  }

  void startClient() {
    parseThread.start();
  }

  Client getRpcClient() {
    return this.rpcClient;
  }

  private class HdlcRpcChannelOutput implements Channel.Output {
    private final OutputStream writer;
    private final long address;

    public HdlcRpcChannelOutput(OutputStream writer, long address) {
      this.writer = writer;
      this.address = address;
    }

    public void send(byte[] packet) throws ChannelOutputException {
      try {
        Encoder.writeUiFrame(this.address, ByteBuffer.wrap(packet), this.writer);
      } catch (IOException e) {
        throw new ChannelOutputException("Failed to write HDLC UI Frame", e);
      }
    }
  }

  private class HdlcParseThread extends Thread {
    private InputStream reader;
    private RpcOnComplete frame_handler;
    private Decoder decoder;

    private class RpcOnComplete implements Decoder.OnCompleteFrame {
      private final Client rpc_client;

      public RpcOnComplete(Client rpc_client) {
        this.rpc_client = rpc_client;
      }

      public void onCompleteFrame(Frame frame) {
        if (frame.getAddress() == RPC_HDLC_ADDRESS) {
          this.rpc_client.processPacket(frame.getPayload());
        }
      }
    }

    public HdlcParseThread(InputStream reader, Client rpc_client) {
      this.reader = reader;
      this.frame_handler = new RpcOnComplete(rpc_client);
      this.decoder = new Decoder(this.frame_handler);
    }

    public void run() {
      while (true) {
        int val = 0;
        try {
          val = this.reader.read();
        } catch (IOException e) {
          logger.atSevere().log("HDLC parse thread read failed");
          System.exit(1);
        }
        this.decoder.process((byte) val);
      }
    }
  }

  public static ConfigProtos.ClientConfig ParseConfigFrom(InputStream reader) throws IOException {
    byte[] buffer = new byte[reader.available()];
    reader.read(buffer);
    ConfigProtos.ClientConfig.Builder config_builder = ConfigProtos.ClientConfig.newBuilder();
    TextFormat.merge(new String(buffer, "UTF8"), config_builder);
    if (config_builder.getChunkTimeoutMs() == 0) {
      throw new AssertionError("chunk_timeout_ms may not be 0");
    }
    if (config_builder.getInitialChunkTimeoutMs() == 0) {
      throw new AssertionError("initial_chunk_timeout_ms may not be 0");
    }
    if (config_builder.getMaxRetries() == 0) {
      throw new AssertionError("max_retries may not be 0");
    }
    return config_builder.build();
  }

  public static void main(String[] args) {
    if (args.length != 1) {
      logger.atSevere().log("Usage: PORT");
      System.exit(1);
    }

    // The port is provided directly as a commandline argument.
    int port = Integer.parseInt(args[0]);

    ConfigProtos.ClientConfig config = null;
    try {
      config = ParseConfigFrom(System.in);
    } catch (Exception e) {
      logger.atSevere().log("Failed to parse config file from stdin");
      System.exit(1);
    }
    int resourceId = config.getResourceId();
    Path fileName = Paths.get(config.getFile());

    if (Files.notExists(fileName)) {
      logger.atSevere().log("Input file `%s` does not exist", fileName);
    }
    byte[] data = null;
    try {
      data = Files.readAllBytes(fileName);
    } catch (IOException e) {
      logger.atSevere().log("Failed to read input file `%s`", fileName);
      System.exit(1);
    }

    Socket socket = null;
    try {
      socket = new Socket(HOSTNAME, port);
    } catch (Exception e) {
      logger.atSevere().log("Failed to connect to %s:%d", HOSTNAME, port);
      System.exit(1);
    }

    InputStream reader = null;
    OutputStream writer = null;

    try {
      writer = socket.getOutputStream();
      reader = socket.getInputStream();
    } catch (Exception e) {
      logger.atSevere().log("Failed to open socket streams");
      System.exit(1);
    }

    JavaClient hdlc_rpc_client = new JavaClient(writer, reader);

    ExecutorService workQueue = Executors.newFixedThreadPool(1);

    hdlc_rpc_client.startClient();

    Manager transferManager = new Manager(
        hdlc_rpc_client.getRpcClient().method(CHANNEL_ID, TransferService.get().name() + "/Read"),
        hdlc_rpc_client.getRpcClient().method(CHANNEL_ID, TransferService.get().name() + "/Write"),
        workQueue::execute,
        config.getChunkTimeoutMs(),
        config.getInitialChunkTimeoutMs(),
        config.getMaxRetries(),
        () -> false);

    try {
      transferManager.write(resourceId, data).get();
    } catch (Exception e) {
      logger.atSevere().log("Transfer failed");
      System.exit(1);
    }

    logger.atInfo().log("Transfer completed successfully");

    System.exit(0);
  }
}
