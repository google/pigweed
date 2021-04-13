# hci-spec

The `hci-spec` library defines convenient and reliable C++ types, values and constants
defining the HCI protocol as specified in the Bluetooth HCI Specification (Core Spec v5.2, Vol 4,
Part E).

In particular:

 * `protocol.h` provides definitions of the opcodes and packet types that make up the HCI protocol,
 such as HCI Command types and HCI event types, for both BR/EDR and Low Energy.
 * `constants.h` provides definitions of the constant values defined in the HCI specification
 * `defaults.h` provides typical default values listed in the HCI specification for certain commands

## A Note on Dependencies

This library is aimed to have minimal dependencies, especially on platform-specific code, such that
it could feasible be widely used across multiple platforms in future. Dependencies should be kept
to a minimum.

## Future work

The design of this library - as a direct representation of types and values from the specification -
could enable machine generation of the library from a concise canonical representation of
specification types in future. If this path were taken, this library - along with equivalents in
other languages - could be automatically generated from a single core definition.
