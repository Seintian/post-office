# Post Office

![CI](https://github.com/seintian/post-office/actions/workflows/ci.yml/badge.svg)
[![Maintainability](https://qlty.sh/gh/Seintian/projects/post-office/maintainability.svg)](https://qlty.sh/gh/Seintian/projects/post-office)
[![Code Coverage](https://qlty.sh/gh/Seintian/projects/post-office/coverage.svg)](https://qlty.sh/gh/Seintian/projects/post-office)

## Network Protocol

```c
/*
┌──────────────────────────────────────────────────────────┐
│  2B: Version │ 1B: MsgType │ 1B: Flags │ 4B: PayloadLen  │
└──────────────────────────────────────────────────────────┘
*/

#pragma pack(push,1)
typedef struct {
    uint16_t version;      // 0x0001
    uint8_t  msg_type;     // enum above
    uint8_t  flags;        // bitmask
    uint32_t payload_len;  // network-order
} po_header_t;
#pragma pack(pop)
```

⟨PayloadLen⟩ bytes of payload (binary or JSON)

## Project Structure

```txt
<project-root>
├── app
│   ├── config/
│   │   ├── explode.ini
│   │   └── timeout.ini
│   │
│   ├── docs/
│   │   ├── design_report.md
│   │   ├── statistics_format.md
│   │   ├── tuning_guide.md
│   │   ├── usage_instructions.md
│   │   └── … (other docs)
│   │
│   ├── include/
│   │   └── postoffice/
│   │       ├── core/
│   │       │   ├── director.h
│   │       │   ├── main.h
│   │       │   ├── shared.h
│   │       │   ├── ticket_issuer.h
│   │       │   ├── user.h
│   │       │   ├── users_manager.h
│   │       │   └── worker.h
│   │       │
│   │       ├── net.h
│   │       ├── storage.h
│   │       ├── perf.h
│   │       └── utils/
│   │           ├── configs.h
│   │           ├── files.h
│   │           ├── logging.h
│   │           └── random.h
│   │
│   ├── libs/                       ← vendored third‑party code
│   │   ├── postoffice/            ← your implementation modules
│   │   │   ├── net/
│   │   │   │   ├── framing.c
│   │   │   │   ├── framing.h       ← internal parser header
│   │   │   │   ├── net.c
│   │   │   │   ├── poller.c
│   │   │   │   ├── poller.h
│   │   │   │   ├── protocol.c
│   │   │   │   ├── protocol.h
│   │   │   │   ├── socket.c
│   │   │   │   └── socket.h
│   │   │   │
│   │   │   ├── perf/
│   │   │   │   ├── batcher.c
│   │   │   │   ├── batcher.h
│   │   │   │   ├── ringbuf.c
│   │   │   │   ├── ringbuf.h
│   │   │   │   ├── zerocopy.c
│   │   │   │   └── zerocopy.h
│   │   │   │
│   │   │   ├── storage/
│   │   │   │   ├── db_lmdb.c
│   │   │   │   ├── db_lmdb.h
│   │   │   │   ├── index.c
│   │   │   │   ├── index.h
│   │   │   │   ├── logstore.c
│   │   │   │   ├── logstore.h
│   │   │   │   └── storage.c
│   │   │   │
│   │   │   └── utils/
│   │   │       ├── configs.c
│   │   │       ├── files.c
│   │   │       ├── logging.c
│   │   │       ├── random.c
│   │   │       └── (any internal headers)
│   │   │
│   │   └── thirdparty/            ← all vendored external libs
│   │       ├── inih/              ← ini.c, ini.h
│   │       ├── libfort/           ← fort.c, fort.h
│   │       ├── lmdb/              ← mdb.c, mdb.h, midl.c, midl.h
│   │       ├── log_c/             ← log.c, log.h
│   │       └── unity/             ← unity.c, unity.h, unity_internals.h
│   │
│   ├── src/                       ← "your" executables entrypoints
│   │   ├── core/
│   │   │   ├── main/              ← simulation dashboard
│   │   │   │   └── main.c
│   │   │   │
│   │   │   └── simulation/
│   │   │       ├── director/      ← Director process
│   │   │       │   └── director.c
│   │   │       │
│   │   │       ├── ticket_issuer/
│   │   │       │   └── ticket_issuer.c
│   │   │       │
│   │   │       ├── user/
│   │   │       │   └── user.c
│   │   │       │
│   │   │       ├── users_manager/
│   │   │       │   └── users_manager.c
│   │   │       │
│   │   │       └── worker/
│   │   │           └── worker.c
│   │   │
│   │   └── (no modules/ stubs here—tests should go under `tests/`)  
│   │
│   ├── tests/                     ← unit & integration tests
│   │   ├── core/                  ← tests for main, director, user, etc.
│   │   ├── net/                   ← echo server/client, framing tests
│   │   ├── perf/                  ← ringbuf, batcher, zerocopy tests
│   │   ├── storage/               ← LMDB & logstore tests
│   │   └── utils/                 ← configs, files, logging, random tests
│   │
│   ├── Makefile                   ← builds all src/core executables + libs + tests
│   └── Doxyfile
│  
└── project                       # Project paperwork, drafts, PDFs
    └── 20241126-Draft-Progetto_SO_2024_25.pdf
```

## License

This project is licensed under the GNU General Public License v3.0.  
See the [LICENSE](LICENSE) file for details.
