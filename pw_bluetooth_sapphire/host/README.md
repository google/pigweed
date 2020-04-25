# bt-host

## Inspect

The Inspect hierarchy is only exposed through the `Host::GetInspectVmo()` FIDL API.

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
```
