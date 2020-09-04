# Overview

Tulips' user interface has been designed to be both lightweight and efficient.
It takes on some of the socket interface concepts and implement them in a tight
event loop fashion suitable for optimization such as asynchronous and
zero-copy I/O.

# Client

## Interface

The client interface provides methods for an application to open and close
connections, and send and receive data:
```cpp
class Client
{
 public:

  /**
   * Open a new connection.
   *
   * @param id the new connection handle.
   *
   * @return the status of the operation.
   */
  virtual Status open(ID & id) = 0;

  /**
   * Connect a handle to a remote server using its IP and port.
   *
   * @param id the connection handle.
   * @param ripaddr the remote server's IP address.
   * @param rport the remote server's port.
   *
   * @return the status of the operation.
   */
  virtual Status connect(const ID id, stack::ipv4::Address const & ripaddr,
                         const stack::tcpv4::Port rport) = 0;

  /**
   * Abort a connection.
   *
   * @param id the connection's handle.
   *
   * @return the status of the operation.
   */
  virtual Status abort(const ID id) = 0;

  /**
   * Close a connection.
   *
   * @param id the connection's handle.
   *
   * @return the status of the operation.
   */
  virtual Status close(const ID id) = 0;

  /*
   * Check if a connection is closed or not.
   *
   * @param id the connection's handle.
   *
   * @return whether or not the connection is closed. True if the connection
   * does not exist.
   */
  virtual bool isClosed(const ID id) const = 0;

  /**
   * Send data through a connection. May send partial data.
   *
   * @param id the connection's handle.
   * @param len the length of the data.
   * @param data the data.
   * @param off the amount of data actually written.
   *
   * @return the status of the operation.
   */
  virtual Status send(const ID id, const uint32_t len,
                      const uint8_t * const data, uint32_t & off) = 0;

  /**
   * Send framed data through a connection. May send partial data, but makes
   * sure that the frame is not sent by itself.
   *
   * @param id the connection's handle.
   * @param hlen the length of the frame.
   * @param header the frame.
   * @param len the length of the data.
   * @param data the data.
   * @param off the amount of data actually written.
   *
   * @return the status of the operation.
   */
  virtual Status send(const ID id, const uint16_t hlen,
                      const uint8_t * const header, const uint32_t len,
                      const uint8_t * const data, uint32_t & off) = 0;

  /**
   * Get average latency for a connection.
   *
   * @param id the connection's handle.
   *
   * @return the average latency of the connection.
   */
  virtual system::Clock::Value averageLatency(const ID id) = 0;
}
```
Clients can have multiple connections. The exact amount of connections can be
passed to the constructor.

# Server interface

The server interface provides methods for an application to accept and close
connections, and send and receive data:
```cpp
class Server
{
 public:

  /**
   * Instruct the server to listen to a particular TCP port.
   *
   * @param port the port to listen too.
   * @param cookie user-defined data attached to the port.
   */
  virtual void listen(const stack::tcpv4::Port port, void * cookie) = 0;

  /**
   * Instruct the server to stop listening to a port.
   *
   * @param port the port to forget.
   */
  virtual void unlisten(const stack::tcpv4::Port port) = 0;

  /**
   * Close a connection.
   *
   * @param id the connection's handle.
   *
   * @return the status of the operation.
   */
  virtual Status close(const ID id) = 0;

  /*
   * Check if a connection is closed or not.
   *
   * @param id the connection's handle.
   *
   * @return whether or not the connection is closed. True if the connection
   * does not exist.
   */
  virtual bool isClosed(const ID id) const = 0;

  /**
   * Send data through a connection. May send partial data.
   *
   * @param id the connection's handle.
   * @param len the length of the data.
   * @param data the data.
   * @param off the amount of data actually written.
   *
   * @return the status of the operation.
   */
  virtual Status send(const ID id, const uint32_t len,
                      const uint8_t * const data, uint32_t & off) = 0;
};
```
Servers can handle multiple connections. The exact amount of connections can be
passed to the constructor.

# Delegate architecture

Clients and Servers use the `Delegate` model to notify the owning application of
various events:
```cpp
template<typename ID> struct Delegate
{
  /*
   * Callback when a connection has been established.
   *
   * @param id the connection's handle.
   * @param cookie a global user-defined state.
   * @param opts a reference to the connection's options to be altered.
   *
   * @return a user-defined state for the connection.
   */
  virtual void * onConnected(ID const & id, void * const cookie, uint8_t & opts) = 0;

  /**
   * Callback when a packet has been acked. The delegate is not permitted
   * to send a response.
   *
   * @param id the connection's handle.
   * @param cookie the connection's user-defined state.
   *
   * @return an action to be taken upon completion of the callback.
   */
  virtual Action onAcked(ID const & id, void * const cookie) = 0;

  /**
   * Callback when a packet has been acked. The delegate is permitted to
   * send a response.
   *
   * @param id the connection's handle.
   * @param cookie the connection's user-defined state.
   * @param alen the amount of data available in the response frame.
   * @param sdata a pointer to the response area in the frame.
   * @param slen the effective size of the response data written.
   *
   * @return an action to be taken upon completion of the callback.
   */
  virtual Action onAcked(ID const & id, void * const cookie,
                         const uint32_t alen, uint8_t * const sdata,
                         uint32_t & slen) = 0;

  /**
   * Callback when new data has been received. The delegate is not permitted
   * to send a response.
   *
   * @param id the connection's handle.
   * @param cookie the connection's user-defined state.
   * @param data the received data.
   * @param len the length of the received data.
   *
   * @return an action to be taken upon completion of the callback.
   */
  virtual Action onNewData(ID const & id, void * const cookie,
                           const uint8_t * const data, const uint32_t len) = 0;

  /**
   * Callback when new data has been received. The delegate is permitted to
   * send a response.
   *
   * @param id the connection's handle.
   * @param cookie the connection's user-defined state.
   * @param data the received data.
   * @param len the length of the received data.
   * @param alen the amount of data available in the response frame.
   * @param sdata a pointer to the response area in the frame.
   * @param slen the effective size of the response data written.
   *
   * @return an action to be taken upon completion of the callback.
   */
  virtual Action onNewData(ID const & id, void * const cookie,
                           const uint8_t * const data, const uint32_t len,
                           const uint32_t alen, uint8_t * const sdata,
                           uint32_t & slen) = 0;

  /*
   * Callback when a connection is closed.
   *
   * @param id the connection's handle.
   * @param cookie the connection's user-defined state.
   */
  virtual void onClosed(ID const & id, void * const cookie) = 0;
};
```
The `onConnected` and `onClosed` callbacks are used to notify the owner when a
connection is established or lost. When a connection is established the owner
can specify a pointer to some user-allocated data. This data is passed along
the other callbacks as a user-managed state. The owner must free that state in
the `onClosed` callback.

The `onConnected` callback allows the owner to specify flags to the new
connections to alter its behavior. The flags currently supported are
`Connection::NO_DELAY` to disable Nagle's algorithm and
`Connection::DELAYED_ACK` to enable delayed acknowledgements.

The `onAcked` callbacks are used to notify the owner that some data sent were
acknowledged by the peer. The second flavor of `onAcked` allow the owner to send
back some data synchronously. This is very useful to handle partial writes.

The first flavor of `onNewData` is called when no response is allowed to be sent
back. This happens if their is no more room in the send buffer available. The
window length of the remote peer is taken into account as well.

The second flavor of `onNewData` is called when a response is allowed to be sent
back. In that case, the amount of data the user is allowed to write is passed to
the callback, along with a pointer to the response area in the send buffer and a
return value for the amount of bytes written. If more than `alen` is passed to
`slen` the final amount is capped.

For both versions of the `onAcked` and `onNewData` callback actions can be taken
upon the reception of data, such as closing or aborting the connection.

## Example

The simple example below shows the basics of running a user-space client:
```cpp
while (keep_running) {
  /*
   * The device and the client need to be polled frequently. The policy for each
   * may vary depending on the configuration.
   */
  switch (device->poll(client)) {
    case Status::Ok: {
      break;
    }
    case Status::NoDataAvailable: {
      client.run();
    }
    default: {
      std::cout << "Unknown error, aborting" << std::endl;
      keep_running = false;
      continue;
    }
  }
  /*
   * Implement a basic state machine that reflect the connection state. The
   * details of this state machine are application-dependent.
   */
  switch (state) {
    case State::Connect: {
      switch (client.connect(id, dst, port)) {
        case Status::Ok: {
          state = State::Run;
          break;
        }
        case Status::OperationInProgress: {
          break;
        }
        default: {
          keep_running = false;
          break;
        }
      }
      break;
    }
    case State::Run: {
      switch (client.send(id, len, data, res)) {
        case Status::Ok: {
          if (++sends == MAX_SENDS) {
            client.close(id);
            state = State::Closing;
          }
          break;
        }
        case Status::OperationInProgress: {
          break;
        }
        default: {
          std::cout << "TCP send error, stopping" << std::endl;
          keep_running = false;
          break;
        }
      }
      break;
    }
    case State::Closing: {
      if (client.close(id) == Status::NotConnected && client.isClosed(id)) {
        keep_running = false;
      }
      break;
    }
  }
}
```
The first and foremost step that clients need to take is to poll the device and
client stack. The frequency of that step depends on the application. For
instance, in the Streams transport implementation the device and stack are
polled only when we need to process an ACK or when we need to send more data
than the stack can currently handle.
