# IE Host bsdsocket.library

This ROM-resident `bsdsocket.library` is specific to `m68k-ie`. It exposes the
standard AROS bsdsocket v4 vectors 5 to 50 and forwards the supported v1 socket
work to the IntuitionEngine host socket MMIO block at `0xF2500-0xF257F`.

The library is a host bridge, not the full AROSTCP stack. Each supported call
builds one fixed request descriptor in guest memory, writes `IE_SOCK_REQ_PTR`
and `IE_SOCK_REQ_LEN`, then writes `IE_SOCK_CMD`. Payload buffers and sockaddr
buffers are passed as guest pointers and are copied in bulk by the host. The
maximum payload per `send`, `sendto`, `recv` or `recvfrom` call is 64 KiB.

Supported v1 calls include IPv4 TCP and UDP socket operations, common socket
options, `WaitSelect`, errno handling, `Inet_NtoA`, `inet_addr`, resolver calls,
`gethostname`, `SocketBaseTagList` and `GetSocketEvents`.

Raw sockets, packet filters, route and interface management, monitoring APIs and
advanced Roadshow extensions are out of scope for v1 and return `EOPNOTSUPP`.
Guest-visible non-blocking state is owned by the host device. Host sockets must
remain non-blocking internally, and blocking guest operations must be bounded by
the host `WaitSelect` path so the emulator cannot stall indefinitely.
