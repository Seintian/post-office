# Simulation Statistics Format

## Overview

This document describes the format and structure of statistics collected during the simulation. These statistics provide insight into the performance and behavior of the post office simulation, tracking metrics for each service type and overall operation.

## Core Statistics Structure

The simulation maintains statistics primarily through two key structures:

### Per-Service Queue Statistics (`queue_status_t`)
Each service type queue tracks:

| Metric | Field | Description |
|--------|-------|-------------|
| Waiting Count | `waiting_count` | Current number of users in the queue |
| Total Served | `total_served` | Cumulative number of customers served |
| Last Ticket | `last_finished_ticket` | The ticket number most recently completed |

### Global Statistics (`global_stats_t`)
The system aggregates:

| Metric | Field | Description |
|--------|-------|-------------|
| Total Tickets Issued | `total_tickets_issued` | Global count of all tickets issued |
| Total Services Completed | `total_services_completed` | Global count of services finished |
| Total Users Spawned | `total_users_spawned` | Total users created by Users Manager |
| Connected Users | `connected_users` | Current number of users attached to SHM |

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
