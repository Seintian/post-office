# Progetto-SO_24-25_Network

![CI](https://github.com/seintian/post-office/actions/workflows/ci.yml/badge.svg)

```txt
<project-root>
├── app
│   ├── config                    # Configuration files (.ini, .yaml)
│   │   ├── explode.ini
│   │   ├── timeout.ini
│   │   └── ssl_cert.pem          # future SSL certs/keys
│   ├── docs                      # Design docs, usage, protocol specs
│   │   ├── design_report.md
│   │   ├── protocol_spec.md
│   │   ├── ssl_key_management.md
│   │   └── tuning_guide.md
│   ├── include
│   │   ├── postoffice            # Public API headers
│   │   │   ├── core.h            # core business logic API
│   │   │   ├── net.h             # networking API
│   │   │   ├── crypto.h          # TLS/auth API
│   │   │   ├── storage.h         # persistence API
│   │   │   └── perf.h            # performance API
│   │   └── thirdparty            # bundled library headers
│   ├── libs                      # Embedded third‑party code
│   │   ├── hashtable
│   │   ├── inih
│   │   ├── log_c
│   │   ├── libfort
│   │   └── unity
│   ├── src
│   │   ├── core                  # Business logic processes
│   │   │   ├── main.c            # Simulation dashboard (Main)
│   │   │   ├── director.c        # Director (Server)
│   │   │   ├── user.c            # User client
│   │   │   ├── worker.c          # Worker client
│   │   │   ├── ticket_issuer.c   # Ticket Issuer client
│   │   │   └── users_manager.c   # Users Manager service/client
│   │   ├── net                   # Networking layer
│   │   │   ├── socket.c          # socket setup & teardown
│   │   │   ├── framing.c         # message framing/parsing
│   │   │   ├── poller.c          # epoll/kqueue wrapper
│   │   │   └── protocol.c        # PostOffice protocol encode/decode
│   │   ├── crypto                # Security and authentication
│   │   │   ├── tls.c             # TLS handshake & context
│   │   │   ├── auth.c            # user authentication
│   │   │   └── rng.c             # secure RNG wrappers
│   │   ├── storage               # Persistence layer
│   │   │   ├── db_lmdb.c         # LMDB integration
│   │   │   ├── logstore.c        # append‑only log impl
│   │   │   └── index.c           # index & compaction
│   │   ├── perf                  # Performance optimizations
│   │   │   ├── ringbuf.c         # lock‑free ring buffer
│   │   │   ├── batcher.c         # message batching
│   │   │   └── zerocopy.c        # zero‑copy routines
│   │   └── utils                 # Shared helpers
│   │       ├── configs.c         # config parsing
│   │   │   ├── logger.c          # logging wrapper
│   │   │   └── files.c           # file utilities
│   └── Makefile                  # Build targets: core/demo/tests/docs
├── tests                         # Unit & integration tests
│   ├── core                      # tests for director/user/worker/etc.
│   ├── net                       # echo server/client, framing tests
│   ├── crypto                    # TLS handshake & auth edge cases
│   ├── storage                   # DB recovery & compaction tests
│   ├── perf                      # throughput/latency benchmarks
│   └── utils                     # config, logging, file utils tests
└── project                       # Project paperwork, drafts, PDFs
    └── 20241126-Draft-Progetto_SO_2024_25.pdf
```

## License

This project is licensed under the GNU General Public License v3.0.  
See the [LICENSE](LICENSE) file for details.
