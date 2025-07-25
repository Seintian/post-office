# Simulation Statistics Format

## Overview

This document describes the format and structure of statistics collected during the simulation. These statistics provide insight into the performance and behavior of the post office simulation, tracking metrics for each service type and overall operation.

## Core Statistics Structure

The simulation maintains statistics primarily through two key structures:

### `service_stats_t` (Per-Service Statistics)

Each service type tracks the following metrics:

| Field | Type | Description |
|-------|------|-------------|
| `nof_serviced_users` | `atomic_uint` | Number of users who received this service |
| `nof_issued_services` | `atomic_uint` | Number of successfully issued services (multiple services can be requested per user) |
| `nof_not_issued_services` | `atomic_uint` | Number of requested services that could not be issued |
| `waiting_time` | `atomic_uint` | Total accumulated waiting time for all users of this service |
| `issuing_service_time` | `atomic_uint` | Total time spent issuing this service across all requests |

### `sim_stats_t` (Global Statistics)

The global statistics container includes:

| Field | Type | Description |
|-------|------|-------------|
| `stats_by_service` | `service_stats_t[NOF_SERVICES + 1]` | Array of statistics for each service type (+ global total stats) |
| `nof_active_operators` | `atomic_uint` | Number of active worker operators during the simulation |
| `nof_operators_pauses` | `atomic_uint` | Total number of pauses taken by operators |
| `nof_users_awaiting` | `atomic_uint` | Current count of users still waiting in any service queue |

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
- A dedicated semaphore (`stats_ready_sem`) synchronizes access to statistics between processes
- The Director process initializes all statistics to zero at simulation start

## Handling of Statistics

The Director process manages the statistics, which are stored in shared memory. Other processes can access these statistics after acquiring the appropriate semaphore lock (`stats_ready_sem`).
Appends all metrics to 2 CSV files via the `save_on_csv()` function for post-run analysis.

## Derived Metrics

From the collected statistics, the following metrics can be derived:

- **Average Waiting Time**: `waiting_time / (nof_issued_services + nof_not_issued_services)`
- **Average Service Time**: `issuing_service_time / nof_issued_services`
- **Service Success Rate**: `nof_issued_services / (nof_issued_services + nof_not_issued_services)`
- **Operator Efficiency**: Ratio of active operator time to total simulation time

## Simulation State

The simulation state (`sim_state_t`) contains additional information related to the overall status of the simulation:

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
