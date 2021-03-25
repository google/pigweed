# bt-host

## Test

`$ fx test //src/connectivity/bluetooth/core/bt-host`

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
    low_energy_discovery_manager:
       state
       paused
       failed_count
       scan_interval_ms
       scan_window_ms
    metrics:
        bredr:
            open_l2cap_channel_requests
            outgoing_connection_requests
            pair_requests
            request_discoverable_events
            request_discovery_events
            set_connectable_false_events
            set_connectable_true_events
        le:
            outgoing_connection_requests
            pair_requests
            start_advertising_events
            start_discovery_events
            stop_advertising_events
    l2cap:
        logical_links:
          logical_link_0x0:
            handle
            link_type
            flush_timeout_ms
            channels:
              channel_0x0:
                local_id
                remote_id
                psm
        services:
          service_0x0:
            psm
    peer_cache:
        metrics:
            bredr:
                bond_failure_events
                bond_success_events
                connection_events
                disconnection_events
            le:
                bond_failure_events
                bond_success_events
                connection_events
                disconnection_events
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
                services
            le_data:
                connection_state
                bonded
                features
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
    low_energy_connection_manager:
        recent_connection_failures
        pending_requests:
            pending_request_0x0:
                peer_id
                callbacks
        outbound_connector:
            peer_id
            is_outbound
            connection_attempt
            state
        connections:
            connection_0x0:
                peer_id
                peer_address
                ref_count
    bredr_connection_manager:
        connection_requests:
            request_0x0:
                peer_id
                has_incoming
                callbacks
        connections:
            connection_0x0:
                peer_id
        last_disconnected:
            0:
                peer_id
                duration_s
                @time
```
