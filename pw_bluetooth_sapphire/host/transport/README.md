# transport

This library implements an abstraction for communicating with a device that speaks the Bluetooth HCI (Host-Controller-Interface) protocol.

In particular, it allows the sending and receiving of HCI control packets (Commands, Responses and Events), and the sending and receiving of ACL (Asynchronous Connection-oriented Link) packets.

In addition to the abstraction, it provides an implementation of this HCI transport for a Fuchsia driver that speaks bt-hci over a FIDL channel.

It does not provide any additional functionality or behavior related to the controller, beyond sending and receiving packets over the HCI transport. For example, it has no knowledge of active connections or current discovery state.
