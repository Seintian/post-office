# Future TUI Implementation Plan

This document outlines the planned screens and features to be implemented in the TUI, beyond the existing Logs and Configuration screens.

## 1. Entities Screen (`screen_entities.c`)
**Goal**: Visualize individual simulation entities (Users, Workers) in real-time.

-   **Table View**: List all active entities with columns for:
    -   `ID`
    -   `Type` (User, Worker, Director)
    -   `State` (Idle, Working, Queued, etc.)
    -   `Location` / `Node`
-   **Filtering**: Filter by entity type or state (e.g., "Show only Idle Workers").
-   **Inspection**: Select an entity to view detailed properties (current task, history, resource usage).

## 2. Network / IPC Screen
**Goal**: Visualize the message passing and communication overhead between processes.

-   **Topology View**: Graph or list showing connected nodes (Director, Issuer, Workers).
-   **Traffic Stats**:
    -   Messages/sec (Sent/Received)
    -   Bandwidth usage (Bytes/sec)
    -   Error rates / Dropped packets
-   **Channel inspection**: View status of IPC channels (Buffer fill level, congestion).

## 3. Help / Shortcuts Screen
**Goal**: Provide in-app documentation for navigation and controls.

-   **Keybindings List**: Global keys (Tab switching, Quit) and Context-specific keys.
-   **Legend**: Explanation of icons or color codes used in the UI.
-   **Search**: Simple text search for commands.

## 4. Director Control Screen (`screen_director_ctrl.c`)
**Goal**: Advanced manual control over the simulation lifecycle.

-   **Simulation Controls**: Pause, Resume, Reset, Stop.
-   **Spawn Control**: Manually spawn specific entities or trigger events.
-   **Scenarios**: Load preset simulation scenarios (e.g., "Morning Rush", "System Overload").
