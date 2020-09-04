# Hardware

## IP/TCP checksum offloading

IP and TCP checksum offloading is a required feature. It does not seem to be supported by the `ConnectX3-VPI` generation of cards. A `ConnectX3-Pro` or superior is required.

## Flow steering (DMFS)

### L2-only filtering

These settings are only available for L2-only filtering. L2-only filtering is only available in configurations where the userspace ARP system is wanted or when only one PE OS instance is deployed.

In `/etc/modprobe.d/mlnx.conf`:

```
options mlx4_core log_num_mgm_entry_size=-39
```

This options sets the `log_num_mgm_entry_size` 5-bit bitfield to `10111` which enforces DMFS, disables IPoIB DMFS, enables A0 DMFS (L2-only, 30% increase in packet throughput), and optimizes for destination-based rules.

### L2/L3/L4 filtering

In `/etc/modprobe.d/mlnx.conf`:

```
options mlx4_core log_num_mgm_entry_size=-35
```

# Diagnostic

## Useful commands

### Ethtool

#### Device's information

```
$ ethtool -I <device>
driver: mlx4_en
version: 4.2-1.2.0
firmware-version: 2.40.7000
expansion-rom-version:
bus-info: 0000:0b:00.0
supports-statistics: yes
supports-test: yes
supports-eeprom-access: no
supports-register-dump: no
supports-priv-flags: yes
```

#### Device's offload features

```
$ ethtool -k <device>
```

#### Device's private flags

##### L2-only filtering

```
$ ethtool --show-priv-flags <device>
Private flags for <device>:
blueflame                     : on
phv-bit                       : off
qcn_disable_32_14_4_e         : off
mlx4_flow_steering_ethernet_l2: on
mlx4_flow_steering_ipv4       : off
mlx4_flow_steering_tcp        : off
mlx4_flow_steering_udp        : off
disable_mc_loopback           : off
rx-copy                       : off
```

##### L2/L3/L4 filtering

```
$ ethtool --show-priv-flags <device>
Private flags for <device>:
blueflame                     : on
phv-bit                       : off
qcn_disable_32_14_4_e         : off
mlx4_flow_steering_ethernet_l2: on
mlx4_flow_steering_ipv4       : on
mlx4_flow_steering_tcp        : on
mlx4_flow_steering_udp        : on
disable_mc_loopback           : off
rx-copy                       : off
```

### ibv_devinfo

```
$ ibv_devinfo
hca_id: mlx4_0
        transport:                      InfiniBand (0)
        fw_ver:                         2.40.7000
        node_guid:                      7cfe:9003:00be:d010
        sys_image_guid:                 7cfe:9003:00be:d013
        vendor_id:                      0x02c9
        vendor_part_id:                 4099
        hw_ver:                         0x1
        board_id:                       MT_1090120019
        phys_port_cnt:                  2
        Device ports:
                port:   1
                        state:                  PORT_ACTIVE (4)
                        max_mtu:                4096 (5)
                        active_mtu:             1024 (3)
                        sm_lid:                 0
                        port_lid:               0
                        port_lmc:               0x00
                        link_layer:             Ethernet

                port:   2
                        state:                  PORT_ACTIVE (4)
                        max_mtu:                4096 (5)
                        active_mtu:             1024 (3)
                        sm_lid:                 0
                        port_lid:               0
                        port_lmc:               0x00
                        link_layer:             Ethernet
```

### ibstat

```
$ ibstat
CA 'mlx4_0'
        CA type: MT4099
        Number of ports: 2
        Firmware version: 2.40.7000
        Hardware version: 1
        Node GUID: 0x7cfe900300bed010
        System image GUID: 0x7cfe900300bed013
        Port 1:
                State: Active
                Physical state: LinkUp
                Rate: 40
                Base lid: 0
                LMC: 0
                SM lid: 0
                Capability mask: 0x04010000
                Port GUID: 0x7efe90fffebed011
                Link layer: Ethernet
        Port 2:
                State: Active
                Physical state: LinkUp
                Rate: 40
                Base lid: 0
                LMC: 0
                SM lid: 0
                Capability mask: 0x04010000
                Port GUID: 0x7efe90fffebed012
                Link layer: Ethernet
```

### mlxup

```
$ mlxup
Querying Mellanox devices firmware ...

Device #1:
----------

  Device Type:      ConnectX3
  Part Number:      MCX354A-FCB_A2-A5
  Description:      ConnectX-3 VPI adapter card; dual-port QSFP; FDR IB (56Gb/s) and 40GigE; PCIe3.0 x8 8GT/s; RoHS R6
  PSID:             MT_1090120019
  PCI Device Name:  0000:0b:00.0
  Port1 MAC:        7cfe90bed011
  Port2 MAC:        7cfe90bed012
  Versions:         Current        Available
     FW             2.40.7000      2.40.7000
     PXE            3.4.0746       3.4.0746

  Status:           Up to date
```