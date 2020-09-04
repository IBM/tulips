Tulips' uses OpenSSL to provide secure `Client` and `Server` classes. Both
classes implements the common client and server interfaces and can therefore be
used interchangeably with the non-secure classes.

# Overview

The `SecureClient` and `SecureServer` classes support `SSLv3`, `TLSv1`,
`TLSv1.1` and `TLSv1.2` encryption methods. The cipher list is limited to the
most secure flavors of `AES`.

# Implementation details

## Handshaking and encryption

SSL-related operations such as handshaking and encryption are implemented as
wrapper around the non-secure classes. Part of the protocol is also implemented
inside SSL-specific delegates.

## Low-level I/O operations

Tulips implements its own high-performance BIO to interface with OpenSSL's
low-level read and write operations. This BIO is based on a memory-mapped
circular buffer and minimize as much as possible redundant copies.

# Performance

The NIC cards used where ConnectX-5.

<p align=center>
<img src="https://github.com/IBM/tulips/blob/master/docs/exps/usr_krn_ssl.svg" width=100%>
</p>
