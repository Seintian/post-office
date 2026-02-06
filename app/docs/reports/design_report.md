# Project Design Report

This document provides a comprehensive overview of the project's architecture, components, and implementation choices.

## Overview

The project implements a multi-process simulation framework for a post office service system. The simulation models a post office with different services, workers, users, and tracks various statistics through a complex system of inter-process communication (IPC) mechanisms.

## Core Components

The project consists of two main components:

### 1. Main

The main entry point of the application that provides a user interface to:

- Validate and view the current simulation configuration
- Check logs from previous executions
- View statistics from previous executions in a tabular output
- Start the simulation

### 2. Simulation

The actual simulation environment consisting of several specialized processes:

#### Director

The central orchestrator process that:

- Initializes and manages all IPC resources
- Creates and coordinates child processes
- Implements **Dynamic Load Balancing** via worker reassignment
- Controls the workflow of the simulation and maintains global state
- Handles cleanup of resources on termination

#### Work Broker

Replaces the legacy Ticket Issuer to provide intelligent distribution:

- Manages **Priority Queues** for all post office services
- Distinguishes between **VIP** and standard service requests
- Interacts with workers to distribute tasks based on priority
- Synchronizes ticket sequences across the fleet

#### Worker

Specialized processes that:

- Represent post office employees
- Handle specific service types
- Take breaks during operation
- Process user requests

#### User

Processes that:

- Request services from the work broker
- Support for **Tiered Service Levels** (10% chance to be a VIP)
- Wait for service completion or office closure
- Configurable service probability and multi-request support

#### Users Manager

External process that adds new users to the simulation.
What happens "under the hood":

- It sends a signal to the Director.

It is the only option suitable to our environment, since the different PGIDs: there would be problems with permissions and PIDs (Director only acts towards children's PGID, not PIDs).

## Services

The simulation models a post office with six different services:

1. Package sending and collection (`SERVICE_PACKAGES`)
2. Letter and registered mail handling (`SERVICE_LETTERS`)
3. BancoBosta withdrawals and deposits (`SERVICE_BANCOPOSTA`)
4. Postal payment slips (`SERVICE_PAYMENTS`)
5. Financial product purchases (`SERVICE_FINANCIAL_PRODUCTS`)
6. Watch and bracelet sales (`SERVICE_WATCHES_AND_BRACELETS`)

## Statistics Tracking

At the end of each simulated day, the director prints both **per‐service** and **global** statistics. For each service (and then overall) we report:

- **Total serviced users**: the cumulative count of users who completed that service  
- **Average serviced users per day**: total serviced users ÷ number of days elapsed  
- **Total issued services**: how many times that service was actually provided  
- **Average issued services per day**: total issued services ÷ number of days elapsed  
- **Total not-issued services**: number of users who left the queue before being served  
- **Average not-issued services per day**: total not-issued services ÷ number of days elapsed  
- **Simulation‐wide average waiting time**: total waiting time (ns) ÷ (issued + not-issued services)  
- **Day-wide average waiting time**: waiting time that day (ns) ÷ (issued + not-issued services that day)  
- **Simulation‐wide average service time**: total service-issuance time (ns) ÷ total issued services  
- **Day-wide average service time**: service-issuance time that day (ns) ÷ issued services that day  
- **Worker-to-seat ratio** (only for services with at least one seat): (operators assigned that day) ÷ (seats for that service)  

After listing per‐service figures, the "Global statistics" section repeats the same metrics aggregated across all services.

Finally, per day the director also logs **worker statistics**:

- **Active operators today**: how many operators performed at least one service  
- **Active operators in simulation**: total distinct operators who ever worked  
- **Total pauses taken**: sum of all break events across operators  
- **Average pauses per active operator (day)**: total pauses today ÷ active operators today  

All of these values are printed via `log_info()`. (CSV export is planned for future iterations).

## Libraries and APIs

### Integrated Libraries

#### Third-Party Libraries

1. **`inih`**
    - Minimalistic INI file parser for configuration
    - Processes simulation configuration parameters
    - Supports multi-line entries and inline comments

2. **`unity`**
    - Lightweight unit testing framework
    - Used for verifying component functionality
    - Provides macros for defining test cases and assertions

3. **`log.c`**
    - Flexible logging library
    - Customized for multi-process logging
    - Supports different log levels (`TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL`)

4. **`libfort`**
    - Lightweight C library for building and printing ASCII/Unicode‑styled tables
    - Ideal for command‑line tools and logs where you need clear, human‑readable tabular output
    - Supports customizable borders, cell alignment, column padding, dynamic resizing and wide‑character (UTF‑8) content

#### Custom Libraries

1. **`hashtable`**
    - Generic hash table implementation
    - Provides key-value storage and retrieval
    - Supports various operations like insertion, removal and lookup

2. **`priority_queue`**
    - Indexed Min-Heap implementation for task scheduling
    - Supports $O(\log N)$ insertion and arbitrary removal
    - Uses a hashtable to track internal element positions for efficiency

3. **`prime`**
    - Simple library for prime number operations
    - Used in hashing algorithms
    - Provides functions to check if a number is prime and find the next prime

### APIs Implemented

1. **`logger`**
    - Built on top of `log.c`
    - Creates dedicated directories for logs
    - Configures callbacks for log file writing
    - Sets log levels from configuration files

2. **`ipc`**
    - Provides unified access to System V IPC resources
    - Implements abstractions for:
        - Semaphores
        - Pipes and named pipes (FIFOs)
        - Shared memory
        - Message queues

3. **`random`**
    - Wrapper for pseudorandom number generation
    - Provides type-specific random number functions
    - Ensures proper initialization of the random seed

## IPC Architecture

The simulation uses several IPC mechanisms to enable communication between processes:

### 1. Synchronization Primitives

Used for process coordination (start barriers, resource access limits, statistics publication). Specific legacy semaphore names have been removed; current implementation abstracts these details behind initialization helpers.

### 2. Shared Memory

Used for sharing data between processes:

- `sim_stats`: Statistics for each service
- `sim_state`: Current state of the simulation
- `nof_processes`: Number of active processes
- `available_services`: Boolean array indicating service availability
- `state_lock`: Synchronizes access to simulation state using a `pthread rwlock`
- `sync_barrier`: Synchronizes all processes to start a new day altogether using a `pthread barrier`

### 3. Message Passing Channels

Used for request handling between users, ticket issuer, and workers. Legacy explicit queue variable names were pruned; the design retains logical channels (issuer requests, per‑service wait queues) without codifying concrete identifiers here.

## Implementation Details

### Initialization Process

1. The Director process initializes configurations, sets up IPC, and spawns the **Work Broker**, Workers, and Users.
2. Users generate requests, designating them as **VIP** or standard based on simulated luck.
3. The **Work Broker** accepts requests and enqueues them into service-specific **Priority Queues**.
4. Workers request work items; the Broker pops the highest priority task (VIPs first, then FIFO).
5. The Director monitors queue depths in shared memory, performing **Worker Reassignment** if the imbalance exceeds thresholds.

3. Workers:
    - Wait for service requests
    - Process requests
    - Take breaks as configured
    - Update service statistics

4. Users:
    - Request services based on probability parameters
    - Wait for ticket issuance
    - Get serviced by workers
    - Make multiple requests as configured

5. Director signals the end of the day:
    - Resets worker seats
    - Updates simulation state
    - Continues to next day or ends simulation

### Error Handling and Cleanup

1. Signal handling:
    - Each process sets up handlers for terminating signals
    - Director propagates termination to child processes

2. Resource cleanup:
    - Director is responsible for cleaning up all IPC resources
    - Each process cleans up its own resources before termination
    - Cleanup is performed in reverse order of initialization

## Termination Conditions

The simulation can end under several conditions:

- `SIM_ENDED_TIMEOUT`: Simulation duration reached
- `SIM_ENDED_EXPLODED`: Too many users awaiting service (explode threshold exceeded)
- `SIM_ENDED_SIGNALED`: Received termination signal
- `SIM_ENDED_ERROR`: Encountered an unrecoverable error

## Logging Strategy

The project implements a comprehensive logging system built around the `log.c` library, extended by a custom `logger` API to support the specific needs of a multi-process environment. A key design choice is the use of **asynchronous logging threads** to prevent blocking the main simulation loops.

The system is initialized in each process via the `init_logger` macro, which configures the log level, file paths, and cleanup options. Log messages are formatted to include a timestamp, process ID (PID), log level, and source file location, providing essential context for debugging. The `logger` API abstracts file handling, using `flock` for atomic operations, and provides flags like `LOGGER_QUIET` to control console output and `CLEANUP_LOGS` to manage log rotation.

## Design Choices and Rationale

1. **Multi-Process Architecture**:
    - Simulates real-world distributed systems
    - Demonstrates IPC mechanisms in practice
    - Provides isolation between components

2. **Centralized IPC Management**:
    - Director handles all IPC resource creation and cleanup
    - Simplifies coordination and prevents resource leaks
    - Ensures consistent initialization order

3. **Asynchronous Logging**:
    - Uses background threads (`logger` library) to offload I/O
    - Lock-free ring buffer for high throughput
    - Prevents simulation stalls during heavy logging

4. **Simulation State in Shared Memory**:
    - Provides atomic access to simulation state
    - Allows all processes to react to state changes
    - Uses pthread rwlock for safe concurrent access

5. **Message Queues and Priority Queues**:
    - Combines socket-based messaging for coordination with internal priority heaps for scheduling
    - Provides structured, priority-aware task distribution
    - Allows for $O(\log N)$ scaling as the number of requests grows

6. **Configuration via INI Files**:
    - Simple, human-readable format
    - Easy to modify simulation parameters
    - Separates configuration from code

## Conclusion

The project demonstrates a complex multi-process system using various IPC mechanisms to simulate a real-world post office environment. The architecture is modular, scalable, and robust, with careful attention to resource management, synchronization, and error handling.
