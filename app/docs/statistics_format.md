# Simulation Statistics Format

## Overview

This document describes the format and structure of statistics collected during the simulation. These statistics provide insight into the performance and behavior of the post office simulation, tracking metrics for each service type and overall operation.

## Core Statistics Structure

The simulation maintains statistics primarily through two key structures:

### Per‑Service Statistics Record

Each service type maintains a record with counters for:

| Metric | Description |
|--------|-------------|
| Serviced Users | Number of users who successfully completed the service |
| Issued Services | Total number of service issuances (a user may request multiple times) |
| Dropped/Not Issued Services | Requests that could not be fulfilled (queue exit / capacity) |
| Cumulative Waiting Time | Sum of waiting durations (ns) across all requests (fulfilled + dropped) |
| Cumulative Service Time | Sum of active service handling durations (ns) for fulfilled requests |

### Global Aggregation Structure

The global aggregation holds:

| Element | Description |
|---------|-------------|
| Per‑Service Array | Collection of the per‑service records plus one synthetic global aggregate |
| Active Operators (Today) | Count of operators who performed at least one issuance during the current day |
| Operator Pauses (Total) | Total pauses taken (day or simulation cumulative depending on snapshot) |
| Users Awaiting | Current number of users present in any waiting queue at snapshot time |

## Service Types

The simulation tracks statistics for the following service types (defined in `service_t` enum):

| Service ID | Service Name | Description | Service Time (units) |
|------------|--------------|-------------|---------------------|
| 0 | `SERVICE_PACKAGES` | Package sending and collection | 10 |
| 1 | `SERVICE_LETTERS` | Letter and registered mail handling | 8 |
| 2 | `SERVICE_BANCOPOSTA` | Bancoposta withdrawals and deposits | 6 |
| 3 | `SERVICE_PAYMENTS` | Postal payment slips | 8 |
| 4 | `SERVICE_FINANCIAL_PRODUCTS` | Financial product purchases | 20 |
| 5 | `SERVICE_WATCHES_AND_BRACELETS` | Watch and bracelet sales | 20 |

## Data Collection and Synchronization

Statistical data is collected in real-time during the simulation and is stored in shared memory accessible by multiple processes. The following mechanisms ensure data integrity:

- All statistic fields use atomic operations (`atomic_uint`) to prevent race conditions
- A dedicated synchronization primitive coordinates safe access to statistics between processes (implementation detail intentionally unnamed here)
- The Director process initializes all statistics to zero at simulation start

## Handling of Statistics

The Director process manages the statistics, which are stored in shared memory. Other processes will access these statistics through a synchronization mechanism (name and type may evolve; legacy specific semaphore names were removed).
Previously documented CSV persistence via a `save_on_csv()` helper is not present in the current codebase. Statistics are accumulated in shared memory structures; CSV or external export is a planned extension point (see director telemetry module roadmap).

## Derived Metrics

From the collected statistics, the following metrics can be derived:

- **Average Waiting Time**: cumulative waiting time ÷ (issued + dropped requests)
- **Average Service Time**: cumulative service time ÷ issued requests
- **Service Success Rate**: issued ÷ (issued + dropped)
- **Operator Efficiency**: Ratio of active operator time to total simulation time

## Simulation State

The simulation state container includes additional information related to the overall status of the simulation:

| State | Description |
|-------|-------------|
| `SIM_NOT_STARTED` | Simulation has not started yet |
| `SIM_STARTING` | Simulation is starting |
| `SIM_RUNNING` | Simulation is running |
| `SIM_ENDED_TIMEOUT` | Simulation ended due to timeout |
| `SIM_ENDED_EXPLODED` | Simulation exploded: too many users awaiting |
| `SIM_ENDED_SIGNALED` | Simulation got interrupted by a signal |
| `SIM_ENDED_ERROR` | Simulation ended due to an error |

Once it's changed to `SIM_RUNNING`, whatever value gets dictates the end of the simulation for all the Director's children.
