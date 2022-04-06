# Security Manager (SM)
This directory contains Fuchsia's implementation of the Security Manager Protocol (SMP) from the Bluetooth Core Specification v5.2 Volume 3 Part H. At a high level, SM works to ensure the integrity and privacy of sensitive communication over Bluetooth. Internally, this is done through the generation, distribution, and usage of Bluetooth security keys. SMP is the standard protocol for security management of Bluetooth LE transports. For dual-mode devices, SMP also provides means for managing BR/EDR transport security.


## Feature support
Description                                                | Level of Support
-----------------------------------------------------------|---------------------------
SMP over BR/EDR                                            | Not Supported
GAP LE Security Mode 1 (encrypted security levels)         | Supported
GAP LE Security Mode 2 (unencrypted, data-signed security) | Not Supported
GAP LE Security Mode 3 (Broadcast_Code encrypted security) | Not Supported
GAP LE Secure Connections Only Mode                        | Supported
Legacy Pairing                                             | Supported
Secure Connections Pairing                                 | Supported
Out of Band Pairing                                        | Not Supported
Identity Resolving Key (IRK) Exchange                      | Supported
Connection Signature Resolving Key (CSRK) Exchange         | Not Supported
Cross-Transport Key Generation                             | Not Supported
SMP Security Request                                       | Supported

See Core Specification v5.2 Volume 3 Part C Section 10.2 for more information about the GAP LE Security Modes.

## Library interface
The Fuchsia SMP library exposes its functionality through the [`SecurityManager`](/src/connectivity/bluetooth/core/bt-host/sm/security_manager.h) abstract class. Each Bluetooth connection is expected to instantiate its own `SecurityManager`, i.e. there is no singleton which manages security for all of Fuchsia Bluetooth. For production, this is done through the `SecurityManagerImpl::Create` factory function, as `SecurityManagerImpl` provides the production implementation of the `SecurityManager` interface. **The SM library expects that clients will only directly instantiate `SecurityManagers`, not any other internal classes. All callbacks related to SMP, including HCI link and L2CAP channel callbacks, must be run on the same thread as the instantiated SecurityManager.** The public interface of SecurityManager is the intended public interface for the SM library as a whole. Documentation can be found in the [`SecurityManager` header](/src/connectivity/bluetooth/core/bt-host/sm/security_manager.h).


### Interface concepts
**Security upgrade** - While most of the code in [this library](/src/connectivity/bluetooth/core/bt-host/sm/) relates to pairing, SM does not allow clients to directly start pairing. Instead, clients request a security upgrade to a certain level. SM tracks the current security properties of the link (i.e. encrypted / authenticated / encryption key size), and may (or may not) determine that pairing and/or link encryption are required to bring the link to the client's desired security level. In response to a security upgrade request, clients are only told whether their request was fulfilled along with the current security properties of the link, not any specifics e.g. about whether their request was directly responsible for pairing.

**PairingDelegate** - Pairing with a peer may give that device access to sensitive information and/or capabilities, so it is good practice (and in some cases, required) to condition pairing on explicit user input. Thus SM must be able to display input prompts and handle user input during pairing. The [`bt-host`](/src/connectivity/bluetooth/core/bt-host/README.md) driver is device-agnostic, so SM cannot directly display output or query for user input from an unknown device. Instead, SM uses the `bt-host`-internal [`sm::Delegate`](/src/connectivity/bluetooth/core/bt-host/sm/delegate.h) class to display information and request user input, which eventually bubbles up to the system [PairingDelegate FIDL protocol](/sdk/fidl/fuchsia.bluetooth.sys/pairing_delegate.fidl).

**Link-layer encryption and SM** - A reasonable, but incorrect, assumption is that SM is directly responsible for encrypting BLE data. SM, the Bluetooth controller, and `hci::Connection` all play roles in encrypting data with the encryption key. SM is responsible for generating (and optionally storing/bonding) the key for the link through pairing. The Bluetooth controller is responsible for validating that both devices agree on the key and then using the key to actually (en|de)crypt data over the link. `SecurityManager` takes a pointer to [`hci::Connection`](/src/connectivity/bluetooth/core/bt-host/hci/connection.h) in its constructor, which serves as the bridge between the two. SM assigns the encryption key to `hci::Connection`, which stores it internally. It then responds to HCI encryption key request events from the controller with this key. **The `hci::Connection` LE encryption key should only ever be modified by SM**.

### Testing
The SM library exposes the `TestSecurityManager` test double through its `sm:testing` GN target. For SM-dependent code, this test double can be used for more deterministic/simpler unit tests. Clients can obtain instances through the `TestSecurityManager::Create` factory function (analogous to production usage of `SecurityManagerImpl::Create`).


## Implementation details
The remainder of this document is aimed at developers who plan to change SM, and explains how the protocol is implemented in Fuchsia.


### Ownership and source hierarchy
This section aims to give a high-level understanding of how the various files and classes in the sm/ directory are related.

#### Stateful pairing classes
Each of these files represents a single stateful class which implements a portion of the SMP pairing state machine. Indentation is used to indicate ownership, with the `SecurityManager` class at the top level - this is consistent with the expectation that only the `SecurityManager` class should be directly instantiated by non-SM code. While the `*Phase*` classes are always owned by `SecurityManager`, the SecurityManager uses a `std::variant` to store the current class, meaning that only 1 `*Phase*` class is ever present at once. Documentation for each class can be found in the linked header file.

- [`SecurityManager`](/src/connectivity/bluetooth/core/bt-host/sm/security_manager.h)
  - [`PairingChannel`](/src/connectivity/bluetooth/core/bt-host/sm/pairing_channel.h)
  - [`SecurityRequestPhase`](/src/connectivity/bluetooth/core/bt-host/sm/security_request_phase.h)
  - [`Phase1`](/src/connectivity/bluetooth/core/bt-host/sm/phase_1.h)
  - [`Phase2Legacy`](/src/connectivity/bluetooth/core/bt-host/sm/phase_2_legacy.h)
  - [`Phase2SecureConnections`](/src/connectivity/bluetooth/core/bt-host/sm/phase_2_secure_connections.h)
    - [`ScStage1JustWorksNumericComparison`](/src/connectivity/bluetooth/core/bt-host/sm/sc_stage_1_just_works_numeric_comparison.h)
    - [`ScStage1Passkey`](/src/connectivity/bluetooth/core/bt-host/sm/sc_stage_1_passkey.h)
  - [`Phase3`](/src/connectivity/bluetooth/core/bt-host/sm/phase_3.h)

#### Abstract pairing classes
These are abstract classes subclassed by many of the "stateful pairing classes".
* [`PairingPhase`](/src/connectivity/bluetooth/core/bt-host/sm/pairing_phase.h) - `SecurityRequestPhase`, `Phase1`, `Phase2Legacy`, `Phase2SecureConnections`, and `Phase3` subclass this class. `PairingPhase` provides interfaces and functionality relevant to all phases of pairing.
* [`ScStage1`](/src/connectivity/bluetooth/core/bt-host/sm/sc_stage_1.h) - `ScStage1JustWorksNumericComparison` and `ScStage1Passkey` subclass this pure interface. `ScStage1` provides methods for `Phase2SecureConnections` to polymorphically interact with both of the Stage 1 classes.

#### Utility files:
These files provide commonly-used functionality in SM. They easily could be (and sometimes are) used outside of SM. These files contain definitions, pure functions of their input, or small structs/classes with little internal state:
  * [`delegate.h`](/src/connectivity/bluetooth/core/bt-host/sm/delegate.h) - details the interface required by the `SecurityManager` constructor for SMP to interact with the rest of the BT stack.
  * [`ecdh_key.h`](/src/connectivity/bluetooth/core/bt-host/sm/ecdh_key.h) - utility C++ wrapper class for BoringSSL ECDH key functionality used in Secure Connections pairing.
  * [`packet.h`](/src/connectivity/bluetooth/core/bt-host/sm/packet.h) - SM-specific packet parsing and writing classes.
  * [`error.h`](/src/connectivity/bluetooth/core/bt-host/sm/error.h) - SM-specific version of the `bt-host` [`Error`](/src/connectivity/bluetooth/core/bt-host/common/error.h) class and associated type aliases.
  * [`smp.h`](/src/connectivity/bluetooth/core/bt-host/sm/smp.h) - definitions of Security Manager Protocol constants from the specification section (see Section 3 of the SM spec section).
  * [`types.h`](/src/connectivity/bluetooth/core/bt-host/sm/types.h) - definitions of structs and enums used in SM code that are not part of the SMP specification.
  * [`util.h`](/src/connectivity/bluetooth/core/bt-host/sm/util.h) - cryptographic primitives and other pure-function utilities used in the SMP stack.

#### Test files:
Besides the `bt-host` standard `<source_file_stem>_unittest.cc` files, SM provides the following test helpers:
* [`TestSecurityManager(Factory)`](/src/connectivity/bluetooth/core/bt-host/sm/test_security_manager.h) - a test implementation of SM's public interface, `SecurityManager`. Provides a `TestSecurityManagerFactory` with a `CreateSm` method which stores a reference to the created `TestSecurityManager`. A `TestSecurityManagerFactory` can be used to inject `TestSecurityManager`s into production code while keeping them accessible by unit tests. The current implementation is a very minimal test spy. It provides basic argument snooping and stub responses for a few methods and noop implementations for others. More functionality may be added as necessary to expand test coverage of SM-dependent code.
* [`FakeListener`](/src/connectivity/bluetooth/core/bt-host/sm/fake_phase_listener.h) - fakes an implementation of the `PairingPhase::Listener` interface, which the `PairingPhase` subclasses require for unit testing.
