# Setup

## Hardware

* Machines are `C0321`, `C0323`, `B0503` and `B0505`.
* The switch is a Mellanox `SX6030`.
* Both `C` machines have a `ConnectX3-Pro` card.
* All machines have a `ConnectX5-VPI` card.
* They are cross-connected on port 0 (`ens8`).
* Port 1 (`ensd1`) is connected to the switch.

## Measured round-trip latencies

| Protocol | Cross-connected | Switched |
|:---------|:---------------:|:--------:|
| Raw      | 1632 ns         | 2170 ns  |
| TCP/IP   | 1771 ns         | 2314 ns  |

The round-trip switch overhead is about 500 ns, close to the advertised 250 ns one-way.

# Experiment

## Protocol

## Results

<p align=center>
<img src="https://github.com/IBM/tulips/blob/master/doc/exps/pkt_per_sec.svg" width=100%>
</p>

<p align=center>
<img src="https://github.com/IBM/tulips/blob/master/doc/exps/bits_per_sec.svg" width=100%>
</p>

