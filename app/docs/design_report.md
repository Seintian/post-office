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
- Controls the workflow of the simulation
- Maintains the global state and statistics
- Handles cleanup of resources on termination

#### Ticket Issuer

Manages ticket allocation for the various post office services:

- Maintains a queue for service requests
- Issues tickets to users based on availability
- Synchronizes with the director process

#### Worker

Specialized processes that:

- Represent post office employees
- Handle specific service types
- Take breaks during operation
- Process user requests

#### User

Processes that:

- Request services from the ticket issuer
- Wait for service to be available
- Have configurable service probability parameters
- Can make multiple service requests

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

All of these values are printed via `log_info()` and then appended to a CSV file for later analysis.

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

2. **`prime`**
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

### 1. Semaphores

Used for process synchronization, implemented with System V semaphores:

- `process_sync_sem`: Synchronizes processes at simulation start
- `worker_seats_sem`: Controls worker access to counters
- `stats_ready_sem`: Synchronizes access to statistics
- `service_issuance_sem`: Controls service issuance

### 2. Shared Memory

Used for sharing data between processes:

- `sim_stats`: Statistics for each service
- `sim_state`: Current state of the simulation
- `nof_processes`: Number of active processes
- `available_services`: Boolean array indicating service availability
- `state_lock`: Synchronizes access to simulation state using a `pthread rwlock`
- `sync_barrier`: Synchronizes all processes to start a new day altogether using a `pthread barrier`

### 3. Message Queues

Used for request handling:

- `ticket_issuer_queue`: Communication between users and ticket issuer
- `service_wait_queues`: Communication between users and workers for each service

## Implementation Details

### Initialization Process

1. The Director process:
    - Loads configuration from INI files
    - Initializes IPC resources
    - Creates child processes (Ticket Issuer, Workers, Users)
    - Synchronizes with all processes before starting simulation

2. Each child process:
    - Connects to IPC resources created by the Director
    - Decrements the process synchronization semaphore
    - Waits for Director to signal the start of simulation

### Simulation Flow

1. Director synchronizes all processes:
    - Assigns worker seats to services
    - Sets available services in shared memory
    - Starts a timer for the day's duration

2. Ticket Issuer:
    - Receives service requests from users
    - Checks service availability
    - Issues tickets for available services

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

The project implements a comprehensive logging system built around the `log.c` library, extended by a custom `logger` API to support the specific needs of a multi-process environment. A key design choice is the use of **separated logging processes** to prevent log corruption and centralize log handling.

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

3. **Separated Logging Processes**:
    - Prevents log corruption in multi-process environment
    - Centralizes log handling
    - Improves performance by offloading logging duties

4. **Simulation State in Shared Memory**:
    - Provides atomic access to simulation state
    - Allows all processes to react to state changes
    - Uses pthread rwlock for safe concurrent access

5. **Message Queues for Service Requests**:
    - Natural fit for queue-based systems like ticket issuing
    - Provides built-in synchronization
    - Allows priority-based processing

6. **Configuration via INI Files**:
    - Simple, human-readable format
    - Easy to modify simulation parameters
    - Separates configuration from code

## Conclusion

The project demonstrates a complex multi-process system using various IPC mechanisms to simulate a real-world post office environment. The architecture is modular, scalable, and robust, with careful attention to resource management, synchronization, and error handling.
