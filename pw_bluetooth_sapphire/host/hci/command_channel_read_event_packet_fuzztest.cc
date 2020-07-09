#include "src/connectivity/bluetooth/core/bt-host/hci/command_channel.h"

#include "slab_allocators.h"

// Prevent "undefined symbol: __zircon_driver_rec__" error.
BT_DECLARE_FAKE_DRIVER();

namespace bt::hci {

void fuzz(const uint8_t* data, size_t size) {
  // Allocate a buffer for the event. Since we don't know the size beforehand
  // we allocate the largest possible buffer.
  auto packet = EventPacket::New(slab_allocators::kLargeControlPayloadSize);
  if (!packet) {
    return;
  }
  zx::channel a;
  zx::channel b;
  zx_status_t status = zx::channel::create(0u, &a, &b);
  if (status != ZX_OK) {
    return;
  }
  a.write(0u, data, size, nullptr, 0);
  CommandChannel::ReadEventPacketFromChannel(b, packet);
}

}  // namespace bt::hci

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  bt::hci::fuzz(data, size);
  return 0;
}
