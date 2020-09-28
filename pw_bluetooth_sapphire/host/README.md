# bt-host

## Test

`$ fx test bt-host-unittests`

## Inspect

`bt-host` uses the [standard driver processes](https://fuchsia.googlesource.com/fuchsia/+/57edce1df72b148c33e8f219bddbd038cdbb861b/zircon/system/ulib/inspect/) to expose its inspect hierarchy
to the Fuchsia system.

### Usage

To query the current state of the `bt-host` Inspect hierarchy through `fx` tooling, run

`fx iquery show-file /dev/diagnostics/class/bt-host/000.inspect`

### Hierarchy
```
adapter:
    adapter_id
    hci_version
    bredr_max_num_packets
    bredr_max_data_length
    le_max_num_packets
    le_max_data_length
    lmp_features
    le_features
    peer_cache:
        peer_0x0:
            peer_id
            technology
            address
            connectable
            temporary
            features
            hci_version
            manufacturer
            bredr_data:
                connection_state
                bonded
            le_data:
                connection_state
                bonded
                features
    data_domain:
    sdp_server:
        record_0x2:
            record
            // TODO(fxbug.dev/51995): Migrate this to UIntArray when support is better.
            registered_psms:
                psm_0x0:
                    psm
                psm_0x1:
                    psm
        record_0x3:
            record
            registered_psms:
                (none)
```
