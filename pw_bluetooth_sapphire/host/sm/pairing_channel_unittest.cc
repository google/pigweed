#include "pairing_channel.h"

#include <memory>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel_test.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {
namespace sm {
namespace {

class FakeChannelHandler : public PairingChannel::Handler {
 public:
  FakeChannelHandler() : weak_ptr_factory_(this) {}

  void OnRxBFrame(ByteBufferPtr data) override {
    last_rx_data_ = std::move(data);
    frames_received_++;
  }

  void OnChannelClosed() override { channel_closed_count_++; }

  ByteBuffer* last_rx_data() { return last_rx_data_.get(); }

  int frames_received() const { return frames_received_; }
  int channel_closed_count() const { return channel_closed_count_; }
  fxl::WeakPtr<PairingChannel::Handler> as_weak_handler() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  ByteBufferPtr last_rx_data_ = nullptr;
  int frames_received_ = 0;
  int channel_closed_count_ = 0;
  fxl::WeakPtrFactory<PairingChannel::Handler> weak_ptr_factory_;
};

class SMP_PairingChannelTest : public l2cap::testing::FakeChannelTest {
 protected:
  void SetUp() override { NewPairingChannel(); }

  void TearDown() override { sm_chan_ = nullptr; }

  void NewPairingChannel(hci::Connection::LinkType ll_type = hci::Connection::LinkType::kLE) {
    l2cap::ChannelId cid =
        ll_type == hci::Connection::LinkType::kLE ? l2cap::kLESMPChannelId : l2cap::kSMPChannelId;
    ChannelOptions options(cid);
    options.link_type = ll_type;

    fake_chan_ = CreateFakeChannel(options);
    sm_chan_ = std::make_unique<PairingChannel>(fake_chan_);
  }

  PairingChannel* sm_chan() { return sm_chan_.get(); }
  l2cap::testing::FakeChannel* fake_chan() const { return fake_chan_.get(); }

 private:
  fbl::RefPtr<l2cap::testing::FakeChannel> fake_chan_;
  std::unique_ptr<PairingChannel> sm_chan_;
};

// This checks that PairingChannel doesn't crash when receiving events without a handler set.
TEST_F(SMP_PairingChannelTest, NoHandlerSetDataDropped) {
  ASSERT_TRUE(sm_chan());
  const auto kSmPacket = CreateStaticByteBuffer(kPairingFailed, ErrorCode::kPairingNotSupported);

  fake_chan()->Receive(kSmPacket);
  RunLoopUntilIdle();

  fake_chan()->Close();
  RunLoopUntilIdle();
}

TEST_F(SMP_PairingChannelTest, SetHandlerReceivesData) {
  ASSERT_TRUE(sm_chan());
  const auto kSmPacket1 = CreateStaticByteBuffer(kPairingFailed, ErrorCode::kPairingNotSupported);
  const auto kSmPacket2 = CreateStaticByteBuffer(kPairingFailed, ErrorCode::kConfirmValueFailed);
  FakeChannelHandler handler;
  sm_chan()->SetChannelHandler(handler.as_weak_handler());
  ASSERT_EQ(handler.last_rx_data(), nullptr);
  ASSERT_EQ(handler.frames_received(), 0);

  fake_chan()->Receive(kSmPacket1);
  RunLoopUntilIdle();
  ASSERT_NE(handler.last_rx_data(), nullptr);
  EXPECT_TRUE(ContainersEqual(*handler.last_rx_data(), kSmPacket1));
  ASSERT_EQ(handler.frames_received(), 1);

  fake_chan()->Receive(kSmPacket2);
  RunLoopUntilIdle();
  ASSERT_NE(handler.last_rx_data(), nullptr);
  EXPECT_TRUE(ContainersEqual(*handler.last_rx_data(), kSmPacket2));
  ASSERT_EQ(handler.frames_received(), 2);

  fake_chan()->Close();
  RunLoopUntilIdle();
  ASSERT_EQ(handler.channel_closed_count(), 1);
}

TEST_F(SMP_PairingChannelTest, ChangeHandlerNewHandlerReceivesData) {
  ASSERT_TRUE(sm_chan());
  const auto kSmPacket1 = CreateStaticByteBuffer(kPairingFailed, ErrorCode::kPairingNotSupported);
  const auto kSmPacket2 = CreateStaticByteBuffer(kPairingFailed, ErrorCode::kConfirmValueFailed);
  FakeChannelHandler handler;
  sm_chan()->SetChannelHandler(handler.as_weak_handler());
  ASSERT_EQ(handler.last_rx_data(), nullptr);
  ASSERT_EQ(handler.frames_received(), 0);

  fake_chan()->Receive(kSmPacket1);
  RunLoopUntilIdle();
  ASSERT_NE(handler.last_rx_data(), nullptr);
  EXPECT_TRUE(ContainersEqual(*handler.last_rx_data(), kSmPacket1));
  ASSERT_EQ(handler.frames_received(), 1);

  FakeChannelHandler new_handler;
  ASSERT_EQ(new_handler.last_rx_data(), nullptr);
  sm_chan()->SetChannelHandler(new_handler.as_weak_handler());
  fake_chan()->Receive(kSmPacket2);
  RunLoopUntilIdle();
  ASSERT_NE(new_handler.last_rx_data(), nullptr);
  EXPECT_TRUE(ContainersEqual(*new_handler.last_rx_data(), kSmPacket2));
  ASSERT_EQ(new_handler.frames_received(), 1);
  // Check handler's data hasn't changed.
  EXPECT_TRUE(ContainersEqual(*handler.last_rx_data(), kSmPacket1));
  ASSERT_EQ(handler.frames_received(), 1);

  fake_chan()->Close();
  RunLoopUntilIdle();
  ASSERT_EQ(new_handler.channel_closed_count(), 1);
  ASSERT_EQ(handler.channel_closed_count(), 0);
}
}  // namespace
}  // namespace sm
}  // namespace bt
