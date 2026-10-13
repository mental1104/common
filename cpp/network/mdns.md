# mDNS Service Discovery

## Design

`mental1104::network::MdnsClient` provides a platform-independent abstraction for DNS-SD based service discovery.

Current identity model:

- service type: identifies the service category
- instance name: identifies an mDNS published instance
- identity key: stored in TXT record and used for stable device lookup
- SRV/A/AAAA records: provide network endpoint

Example:

```
_file-manager._tcp.local

TXT:
  id=robot-001
  version=1

SRV:
  port=8080

A:
  192.168.1.10
```

## Backend plan

The public API intentionally does not depend on a specific implementation.

- Linux: Avahi backend
- macOS / Windows: Bonjour DNS-SD backend

The application layer only consumes service discovery results and should use HTTP, gRPC, WebRTC or other protocols for actual communication.
