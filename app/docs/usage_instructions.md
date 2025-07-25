# Simulation Project - Usage Instructions

This document provides detailed instructions on how to use the simulation project, explaining its components, command-line options, and interactive features.

## Table of Contents

- [Overview](#overview)
- [Building the Project](#building-the-project)
- [Running Modes](#running-modes)
    1. [Full Application](#1-full-application)
    2. [Simulation Only](#2-simulation-only)
- [Command-Line Options](#command-line-options)
- [Configuration Files](#configuration-files)
- [Interactive Features](#interactive-features)
- [Crash Handling](#crash-handling)
- [Logging System](#logging-system)
- [Troubleshooting](#troubleshooting)

## Overview

The simulation project is a multi-process application composed of several cooperating executables, libraries, and utilities:

- **Main**: The complete application with an interactive menu interface for managing simulations, inspecting logs, and more
- **Director**: Orchestrates child processes for simulation and terminates when the simulation is complete
- **Worker**: Performs simulation tasks based on directives
- **User**: Simulates user interaction with the system
- **Ticket Issuer**: Manages ticket allocation for simulation events
- **Users Manager**: Adds users when called

Supporting modules include a logger library, utilities for argument parsing and string handling, crash handling capabilities, INI-based configuration, and a terminal-based GUI for user interaction.

## Generating the documentation

To generate the documentation with Doxygen, use the `make` command:

```bash
make doc
```

**NOTE**: some third-party software is required: `doxygen` and `Graphviz/dot`. This generates the documentation in two formats: LaTeX and HTML. To transform the LaTeX files into a single PDF file, write this:

```bash
cd /path/to/repo_root/app/docs/latex
make
```

**NOTE**: some third-party software is required: `pdfTex`. This translates `.tex` files into `.pdf`. The output, here, will be a single `refman.pdf` of several pages.

## Building the Project

To build the project, navigate to the project root directory and use the `make` command:

```bash
cd /path/to/repo_root/app
make all
```

This will compile all executables without starting the application.

**NOTE**: Be sure to run the `make` commands from the [app/](../) folder.

## Running Modes

The application can be run in two different modes:

### 1. Full Application

The Main process is the full application and can be run with:

```bash
make start
```

Or:

```bash
make # `start` is the default target
```

You can specify a configuration file with either command:

```bash
make start CONFIG="config/timeout.ini"
```

Or:

```bash
make CONFIG="config/timeout.ini"
```

Predefined configurations:

```bash
make timeout   #: make CONFIG="config/timeout.ini"
make explode   #: make CONFIG="config/explode.ini"
```

The Main process provides all the application's functionality, including:

- Log inspection and management
- Configuration file viewing and management
- The ability to launch simulations

### 2. Simulation Only

To run only the simulation component (the Director process) directly, bypassing the Main process:

```bash
make simulation
```

**NOTE**: The configuration file will be asked by `make` before running the executable.

This mode directly launches the Director process, which will:

1. Run the simulation
2. Handle all simulation-related processes and communications
3. Terminate when the simulation is complete

Unlike the Main process, the Director process does not provide an interactive menu interface and only exists for the duration of the simulation.

If you don't provide a configuration file, `make` will ask you to enter one in a `bash` interactive session (just a `read`).

## Command-Line Options

Both the main executables accept the following command-line options:

- `-h, --help`: Display a help message and exit
- `-l, --loglevel LEVEL`: Set log level (`TRACE`|`DEBUG`|`INFO`|`WARN`|`ERROR`|`FATAL`)
- `-c, --config FILE`: Specify the configuration file

Example:

```bash
./bin/SO_Proj-main --loglevel DEBUG --config config/timeout.ini
```

## Configuration Files

The application uses INI configuration files located in the `config/` directory:

- [timeout.ini](../config/timeout.ini): Configuration with timeout settings
- [explode.ini](../config/explode.ini): Configuration with explosion settings

To create a custom configuration, copy one of the existing files and modify it as needed.

## Interactive Features

When running the Main process, you can use the interactive menu to:

1. Launch simulations (when a configuration file is specified)
2. Inspect logs
3. View configuration files
4. View the last simulation statistics in a tabular form (calls the system `$PAGER`)
5. Exit the application

The menu is navigated using keyboard inputs: press the up and down arrow keys to navigate it and the Spacebar or the Enter key to select its options.

## Crash Handling

On fatal signals (e.g., `SIGSEGV`, `SIGABRT`), the application:

- Writes a memory map snapshot to [`PROJECT_MEMSEGS_SNAPSHOTS_DIR`](../include/shared/project_macros.h#L91)
- Captures a backtrace up to 15 frames ([`MAX_BACKTRACE_SIZE`](../include/shared/crash_handling.h#L34))
- Dumps the CPU registers' contents (architectures supported: `x86-64`, `aarch64`)
- Generates a human-readable crash report under [`PROJECT_CRASH_REPORTS_DIR`](../include/shared/project_macros.h#L88)
- Exits re-raising that fatal signal

**NOTE**: if the current platform isn't actually supported by the application, a warning message will be written into the crash report. It _should_ compile anyway.

## Logging System

The application uses a centralized logging system with the following levels:

- `TRACE`: Detailed function call tracking
- `DEBUG`: Debugging information
- `INFO`: General information messages
- `WARN`: Warning messages
- `ERROR`: Error messages
- `FATAL`: Critical errors

Logs are written to the `logs/` directory; some are also displayed in the terminal.

## Troubleshooting

If the application crashes or behaves unexpectedly:

1. Check the crash reports in the `crash_reports/` directory
2. Examine the logs in the `logs/` directory
3. Verify your configuration file
4. Try running with a higher log level for more detailed information:

    ```bash
    make start CONFIG="config/timeout.ini" LOG_LEVEL="TRACE"
    ```

If you encounter persistent issues, consider cleaning the repo and recompile everything with:

```bash
make dev
```

This will clean the build, generate a `compile_commands.json` file for better IDE integration, and run tests to ensure everything is working correctly.

**NOTE**: some third-party software is required: `bear`, used to populate the `compile_commands.json` file that helps with IDE's plugins for linting. If you don't want this file but only run the tests after the complete recompilation, simply run:

```bash
make clean && make test
```
