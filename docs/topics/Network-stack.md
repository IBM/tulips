# Summary

## Origins

The project started on the foundation laid by Adams Dunkels with the [uIP](https://github.com/adamdunkels/uip) TCP/IP
stack.

I had past experience over 10 years ago with his previous work ([lwIP](https://savannah.nongnu.org/projects/lwip/)). It grew
a lot in terms of complexity and features over the years, to the point where its
author himself decided to rewrite it from scratch. This is how `uIP` came to.

`uIP` is a very lightweight TCP/IP stack. It was originally designed to run on
8-bit micro-controllers. The code is terse while readable, the data structures
are well sized, and it has a sufficiently large protocol support to be interesting
(ARP, ICMP, IP, TCP). 

## What it became

I reworked the entire stack to be as memory-dense and functionally compact as
possible. Important data structures are precisely sized for cache locality and
function call depth is limited to its minimum.

I migrated the code to C++ to better integrate with Streams' transport
architecture, cleaned-up some unnecessary redundancies and added a high-level
API suitable for Streams.

Finally, I added support for various performance feature such as TCP
segmentation, delayed TCP ACK, vectored I/O, checksum offloading, TCP
segmentation offloading, and most importantly made sure it played nice with
Linux.

# Protocol pipeline

The protocols are implemented using combinations of processors and producers.
With this combination, protocols can be arranged in pipelines like below:

<p align=center>
<img src="https://github.com/IBM/tulips/blob/master/docs/rsrcs/stack_pipeline.svg" width=80%>
</p>

One end of the pipeline is always a hardware device (`ofed`, `shm`, ...). The
other end of the pipeline is a top-level protocol such as ARP, ICMP or TCP.

# Supported protocols

## Ethernet

The Ethernet layer adds support to process and produce Ethernet frames. On
the processing end, it extracts the address and protocol information from the
header and calls the processor corresponding to the protocol. On the producing
end, it fills the header's information before passing the frame to the next
producer.

The Ethernet layer has no dependencies.

**NOTE** VLAN is not supported.

## ARP

The ARP layer adds support to handle ARP requests and responses. The layer
implementation maintains a translation table internally. Requests are triggered
by external applications when a particular translation is missing. Responses are
handled on demand.

The ARP layer depends on the Ethernet layer and the IPv4 layer.

## ICMPv4

The ICMPv4 layer adds support to handle ICMPv4 requests and responses. ICMPv4
requests are triggered by external applications. Responses are handled on
demand.

The ICMPv4 layer depends on the Ethernet layer, the IPv4 layer and the ARP layer.

## IPv4

The IPv4 layer adds support to process and produce IPv4 frames. On the
processing end, it extracts the IP port, length, and protocol information from
the header and calls the processor corresponding to the protocol. On the
producer end, it fills the header's information before passing the frame to the
next producer.

The IPv4 layer depends on the Ethernet layer.

**NOTE** IP segmentation is not supported; IP options are not supported.

## TCPv4

The TCPv4 layer adds support to process and produce TCPv4 frames, as well as
manage TCP sessions.

### Client interface

The TCP layer exports a session management interface that can be used by external
clients:
```cpp
class Processor : public transport::Processor
{
 public:

  Processor(transport::Device & device, ethernet::Producer & eth,
            ipv4::Producer & ip4, EventHandler & h, const size_t nconn);

  /* ... */

  Status connect(ethernet::Address const & rhwaddr, ipv4::Address const & ripaddr,
                 const Port rport, const uint8_t opts, Connection::ID & id);

  Status abort(Connection::ID const & id);
  Status close(Connection::ID const & id);

  Status send(Connection::ID const & id, const uint32_t len,
              const uint8_t * const data, uint32_t & off);

  Status send(Connection::ID const & id, const uint16_t hlen,
              const uint8_t * const header, const uint32_t len,
              const uint8_t * const data, uint32_t & off);

  /* ... */
};
```
Through the TCP layer, external clients can connect, abort, and close
connections. They can also send data outside of the main event loop.

### Event handling

When created, TCP layers require amongst other arguments a reference to an event
handler. `EventHandler` is an interface used by the TCP layer to forward events
to external applications:
```cpp
class EventHandler
{
 public:

  /*
   * Called when the connection c is connected.
   */
  virtual void onConnected(Connection & c) = 0;

  /*
   * Called when the connection c is aborted.
   */
  virtual void onAborted(Connection & c) = 0;

  /*
   * Called when new data on c has been received.
   */
  virtual Action onNewData(Connection & c, const uint8_t * const data,
                           const uint32_t len) = 0;

  /*
   * Called when new data on c has been received and a response can be sent.
   */
  virtual Action onNewData(Connection & c, const uint8_t * const data,
                           const uint32_t len, const uint32_t alen,
                           uint8_t * const sdata, uint32_t & slen) = 0;
  /*
   * Called when the connection c has been closed.
   */
  virtual void onClosed(Connection & c) = 0;
};
```
These events are used by external applications, clients and servers both.
Connection events are forwarded to the `onConnected()`, `onAborted()`, and
`onClosed()` callbacks. Data reception is forwarded to the `onNewdata()`
callbacks.

### Connections

The TCP layer maintains session information through the `Connection` class. The
`Connection` class is central to the implementation of the TCP layer and has
been finely tuned to fit within 2 Intel cache lines or 1 POWER cache line
(128B).

### Optimizations

The TCP layer supports several performance optimizations that can be enabled or
disabled on a per-connection basis.

#### Nagle's algorithm

[Nagle's algorithm](https://en.wikipedia.org/wiki/Nagle%27s_algorithm) is fully supported in conjunction with the segmentation
optimization discussed below. Nagle's algorithm can be disabled by using the
`Connection::NO_DELAY` option on TCP connections.

#### Delayed ACKs

[Delayed ACKs](https://en.wikipedia.org/wiki/TCP_delayed_acknowledgment) are partially supported. When enabled, the layer will wait until
after the `onNewData()` callback has been called to acknowledge received data.
Combining ACKs for multiple received data frames is not supported. Unlike other
implementations, delayed ACKs are disabled by default.

#### Segmentation

Ethernet network devices are not able to send more than 1 MTU worth of data. To
cope with this limitation, TCP implementations accept to send larger payloads by
segmenting those payloads into MTU-sized payloads and sequentially send them.
When implemented in software this feature is very costly.

<p align=center>
<img src="https://github.com/IBM/tulips/blob/master/docs/rsrcs/segmentation.svg" width=70%>
</p>

Modern NICs offer an optimization named TCP Segmentation Offload (TSO) that
allow the TCP stack to give to the hardware payloads larger than its MTU. It
would then segment these payloads into MTU-sized payloads without any CPU
intervention.

The TCP layer fully supports TSO. It also provides a software version of the
symmetric operation, called Large Receive Offload (LRO), to properly interface
with TCP stacks that perform TSO. This software LRO can be deactivated if a
hardware version of LRO becomes available.

#### Asynchronous in-flight segments

The TCP protocol allows endpoints to send as many segments as possible as long
as the remote peer advertise a large enough receive window. The TCP layer
supports multiple asynchronous segments before an ACK is received.

<p align=center>
<img src="https://github.com/IBM/tulips/blob/master/docs/rsrcs/asyncsegs.svg" width=80%>
</p>

The default configuration can have up to 4 asynchronous segments. The stack can
be configured to have up to 32 asynchronous segments. When combined with
Mellanox TSO, which allows up to 256KB segments, the TCP stack allows up to 8MB
of in-flight data.

### Linux compatibility

Linux TCP implementation has several interesting quirks that had to be supported
to make hybrid channels work.

#### PSH/ACK and FIN/ACK

The TCP protocol does not require endpoints to append an ACK to PSH or FIN as
long as the previous segment has been acknowledge. However, the Linux TCP stack
requires PSH and FIN to both carry the ACK flag or would simply ignore them.

#### Aggregated ACKs

Linux's TCP stack uses an internal timer to aggregate the ACKs of multiple
received segments. As a result, ACKs falling between expected ACK number
boundaries may be received. The TCP stack supports such scenarios.

#### Out-of-order segments and window resize update

Every once in a while the Linux TCP stack may also process segments out of order
and request a retransmission. It would also send a duplicated ACK with a window
size update with the expectation of a retransmission. The TCP layer supports
bith of these scenarios.
