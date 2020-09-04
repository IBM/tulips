# Summary

The transport layer proposes a set of handy abstractions to deal with producing
and consuming data flows. It is made of three basic classes: `Producer`,
`Processor`, and `Device`. Several transport devices are implemented using these
abstractions, most prominently the `ofed` device and the `pcap` device.

# Architecture

## Producer

The role of a producer is to handle the production of data across various levels
of the I/O stack. It has the following interface:
```cpp
class Producer {
 public:

  virtual ~Producer() { }

  /**
   * @return the producer's segment size.
   */
  virtual uint32_t mss() const = 0;

  /*
   * Prepare an asynchronous send buffer to use in a future commit. The buffer
   * is at least of the size of mss().
   *
   * @param buf a reference to an uint8_t pointer to hold the new buffer.
   *
   * @return the status of the operation.
   */
  virtual Status prepare(uint8_t * & buf) = 0;

  /*
   * Commit a prepared buffer.
   *
   * @param len the length of the contained data.
   * @param buf the previously prepared buffer.
   * @param mss the mss to use in case of segmentation offload.
   *
   * @return the status of the operation.
   */
  virtual Status commit(const uint32_t len, uint8_t * const buf,
                           const uint16_t mss = 0) = 0;
};
```
The couple of methods `prepare()` and `commit()` are designed to overlap as
much as possible the overhead of allocating and sending I/O buffers. A send
buffer is expected to be always ready to be used and it is reallocated
immediately after being sent. 

The `mss()` function is designed to discover the actual `mss` of the producer.
this is necessary when the underlying hardware support segmentation offload. In
that case, a MSS larger than the hardware MTU can be reported by the hardware.

Symmetrically, the `mss` argument of the `commit()` function is used to let
the segmentation offload engine know the size of the final segments. This is
necessary as the MSS of the peer can be renegotiated over the course of a TCP
session.

## Processor

The role of a processor is to handle an incoming piece of data without copy. It
has the following interface:
```cpp
class Processor
{
 public:

  virtual ~Processor() { }

  /**
   * Run the processor when data is not available. This is usually called
   * periodically as a result of a timer event.
   *
   * @return the status of the operation.
   */
  virtual Status run() = 0;

  /**
   * Process an incoming piece of data. The processing must be done without copy
   * as much as possible.
   *
   * @param len the length of the piece of data.
   * @param data the piece of data.
   *
   * @return the status of the operation.
   */
  virtual Status process(const uint16_t len, const uint8_t * const data) = 0;
}
```
The key method of this interface is `process()`. It is called whenever a piece
of data has been received and needs to be processed. The method `run()` is
present to allow a processor to be run periodically in the absence of incoming
data. This is necessary to allow the execution of periodic events such as
automated retransmissions in TCP.

## Device

A device acts as a bridge between a low-level link and upper layers of the
network stack. It acts as a producer calls upon processor as shown in the
following interface:
```cpp
class Device : public Producer
{
  /* ... */

  /**
   * Poll the device input queues for activity. Call upon the rcv processor
   * when data is available. This operations is non-blocking.
   *
   * @param rcv the processor to handle the incoming data.
   *
   * @return the status of the operation.
   */
  virtual Status poll(Processor & rcv) = 0;

  /**
   * Wait on the device input queues for new data with a ns timeout. Call upon
   * the rcv processor when data is available. This operation is blocking
   *
   * @param rcv the processor to handle the incoming data.
   * @param ns the timeout of the operation.
   *
   * @return the status of the operation.
   */
  virtual Status wait(Processor & rcv, const uint64_t ns) = 0;

  /* ... */
};
```
The `poll()` and `wait()` methods are the cornerstone of this interface. The
`poll()` method checks if any incoming data is available and calls upon the
processor passed in argument to process it. This method is non-blocking and must
be very fast. The `wait()` method waits for incoming data and calls upon the
processor passed in argument to process it. It is blocking and can time out.

Devices can be chained together to implement interesting functions. A device
that takes another device as argument is called a pseudo-device. the best
example of such a device is the PCAP pseudo-device that writes all sent and
received packets to a PCAP file.

# Implementations

## Hardware devices

### OFED

The OFED transport implementation drives Mellanox ConnectX-class of devices. It
implements the `Device` interface using the OFED Verbs. It leverages all of the
hardware offload features provided by the CX4/CX5 generation of NICs:

* TCP/IP Checksum Offload (TCO)
* TCP Segmentation Offload (TSO)

It also has all the necessary wiring to support Large Receive Offload (LRO) if
that feature is ever brought to the userspace API.

### NPIPE

The NPIPE device uses name pipes as data conduits. It is used for debugging
purposes.

### TAP

The TAP device uses OpenBSD's TUN/TAP devices as data conduits. It is used for
debugging purposes.

### SHM

The SHM device uses lock-free FIFO as data conduits. It is used for
single-process, multi-thread executions.

### LIST

The LIST device uses C++'s `std::list` and `new`/`delete` facilities as data
conduits. It is used for debugging purposes.

## Pseudo-devices

### PCAP

The PCAP pseudo-device provides packet logging support to any devices. Packet
sent and received are written in a PCAP file that can later be used with
`tcpdump`, `tshark` or `wireshark`.

### CHECK

The CHECK pseudo-device checks if any empty packet has been received. It is
used to debug new devices.

### ERASE

The ERASE pseudo-device erases sent and received buffers after use. It is used
to look for potential memory corruptions.
