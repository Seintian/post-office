# Post Office

![CI](https://github.com/seintian/post-office/actions/workflows/ci.yml/badge.svg)
[![Maintainability](https://qlty.sh/gh/Seintian/projects/post-office/maintainability.svg)](https://qlty.sh/gh/Seintian/projects/post-office)
[![Code Coverage](https://qlty.sh/gh/Seintian/projects/post-office/coverage.svg)](https://qlty.sh/gh/Seintian/projects/post-office)

A high-performance, multi-process simulation of a post office environment, demonstrating simplified OS concepts like IPC, signaling, shared memory, and ring-based architecture.

## Architecture Overview

For a deep architectural description (process model, subsystems, data flows, storage design, performance/metrics, error handling, extensibility guidelines, and glossary) see the dedicated [ARCHITECTURE.md](ARCHITECTURE.md).

## Getting Started

### Prerequisites

- Linux environment (tested on Linux 6.x)
- GCC (supporting C11/GNU11)
- Make
- Doxygen (optional, for documentation)
- Graphviz (optional, for Doxygen graphs)

### Building and Running

The project includes a comprehensive `Makefile` in the `app` directory.

1. **Start the Simulation**:
   Builds and runs the main process, which orchestrates the simulation.
   ```bash
   cd app
   make start
   ```

2. **Start with TUI**:
   Runs the simulation with a Text User Interface for real-time visualization.
   ```bash
   cd app
   make start-tui
   ```

3. **Development Build**:
   Builds with debug flags and runs tests.
   ```bash
   cd app
   make dev
   ```

4. **Running Tests**:
   ```bash
   cd app
   make test
   ```

### Configuration

Configuration files are located in `app/config/`:
- `explode.ini`: high-load simulation scenario.
- `timeout.ini`: scenario testing timeout handling.

You can specify a configuration file via the `CONFIG` environment variable or when prompted by `make start`.

├── app
│   ├── config/              # Runtime configurations
│   ├── docs/                # Reports and technical guides
│   ├── include/             # Public headers (Priority Queue, Net, Perf)
│   │
│   ├── libs/                # Libraries arranged by stability/dependency rings
│   │   ├── ring0/           # Fundamental: random, vector, thirdparty (inih, lmdb, etc.)
│   │   ├── ring1/           # Core Utils: hashtable, priority_queue, sysinfo
│   │   ├── ring2/           # Instrumentation: metrics, log, perf core
│   │   ├── ring3/           # Storage/Crash: storage, backtrace, zerocopy
│   │   └── ring4/           # Application-Level: net, tui
│   │
│   ├── src/                 # Application Source
│   │   ├── core/
│   │   │   ├── main/        # Entry point
│   │   │   └── simulation/  # Simulation (director, worker, work_broker, etc.)
│   │   └── utils/           # App-specific utilities
│   │
│   ├── tests/               # Unit and integration tests
│   └── Makefile             # Main build script
│
├── ARCHITECTURE.md          # Multi-process orchestration map
└── README.md                # Overview and quick start
```

## Advanced Features

- **Dynamic Load Balancing**: Real-time worker reassignment based on queue imbalance.
- **Tiered Quality of Service**: Priority-based scheduling for VIP users.
- **Work Broker Ecosystem**: Multi-threaded distribution layer replacing static issuance.
- **Indexed Min-Heaps**: Specialized data structures for efficient task management.

## Network Protocol

The simulations uses a custom binary protocol for IPC:

```c
/*
┌──────────────────────────────────────────────────────────┐
│  2B: Version │ 1B: MsgType │ 1B: Flags │ 4B: PayloadLen  │
└──────────────────────────────────────────────────────────┘
*/
```

## License

This project is licensed under the GNU General Public License v3.0.  
See the [LICENSE](LICENSE) file for details.
