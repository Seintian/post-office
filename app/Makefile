MAKE                   = make
CC                     = gcc

# Try to find bash; if not found, fall back to /bin/sh
BASH := $(shell command -v bash 2>/dev/null)
ifeq ($(BASH),)
	SHELL := /bin/sh
	READ_FLAGS :=
else
	SHELL := $(BASH)
	# enable readline (tab-complete, arrow-keys) when bash is available
	READ_FLAGS := -e -i "config/timeout.ini"
endif

# *** Directories *** #

# Generated Directories
DOCS_DIR               = docs
LOGS_DIR               = logs
CRASH_REPORTS_DIR      = crash_reports

# Work Directories
CONFIG_DIR             = config
BUILD_DIR              = build
BIN_DIR                = bin
TESTS_DIR              = tests
INCLUDE_DIR            = include
POSTOFFICE_INCLUDE_DIR = $(INCLUDE_DIR)/postoffice
THIRDPARTY_INCLUDE_DIR = $(INCLUDE_DIR)/thirdparty
LIBS_DIR               = libs
POSTOFFICE_LIBS_DIR    = $(LIBS_DIR)/postoffice
THIRDPARTY_LIBS_DIR    = $(LIBS_DIR)/thirdparty
SRC_DIR                = src

# Libraries Directories
## Post Office Libraries
HASHTABLE_DIR          = $(POSTOFFICE_LIBS_DIR)/hashtable
HASHSET_DIR            = $(POSTOFFICE_LIBS_DIR)/hashset
NET_DIR                = $(POSTOFFICE_LIBS_DIR)/net
PERF_DIR               = $(POSTOFFICE_LIBS_DIR)/perf
METRICS_DIR            = $(POSTOFFICE_LIBS_DIR)/metrics
PRIME_DIR              = $(POSTOFFICE_LIBS_DIR)/prime
STORAGE_DIR            = $(POSTOFFICE_LIBS_DIR)/storage
RANDOM_DIR             = $(POSTOFFICE_LIBS_DIR)/random
TUI_LIB_DIR            = $(POSTOFFICE_LIBS_DIR)/tui
LOG_DIR                = $(POSTOFFICE_LIBS_DIR)/log
SYSINFO_DIR            = $(POSTOFFICE_LIBS_DIR)/sysinfo
BACKTRACE_DIR          = $(POSTOFFICE_LIBS_DIR)/backtrace
VECTOR_DIR             = $(POSTOFFICE_LIBS_DIR)/vector

## Third-party Libraries
INIH_DIR               = $(THIRDPARTY_LIBS_DIR)/inih
LIBFORT_DIR            = $(THIRDPARTY_LIBS_DIR)/libfort
LMDB_DIR               = $(THIRDPARTY_LIBS_DIR)/lmdb
LOG_C_DIR              = $(THIRDPARTY_LIBS_DIR)/log_c
UNITY_DIR              = $(THIRDPARTY_LIBS_DIR)/unity

## Core source tree root
CORE_SRC_DIR           = $(SRC_DIR)/core
# Utils source directory
UTILS_DIR              = $(SRC_DIR)/utils
# Processes' sources directories
MAIN_DIR               = $(CORE_SRC_DIR)/main
MAIN_SIM_DIR           = $(MAIN_DIR)/simulation
SIMULATION_DIR         = $(CORE_SRC_DIR)/simulation
DIRECTOR_DIR           = $(SIMULATION_DIR)/director
TICKET_ISSUER_DIR      = $(SIMULATION_DIR)/ticket_issuer
USERS_MANAGER_DIR      = $(SIMULATION_DIR)/users_manager
WORKER_DIR             = $(SIMULATION_DIR)/worker
USER_DIR               = $(SIMULATION_DIR)/user
SIM_IPC_DIR            = $(SIMULATION_DIR)/ipc

# Configuration files
TIMEOUT_INI_PATH       = $(CONFIG_DIR)/timeout.ini
EXPLODE_INI_PATH       = $(CONFIG_DIR)/explode.ini

# *** Compiler flags *** #

INCLUDES_I = -I$(INCLUDE_DIR) -I$(POSTOFFICE_INCLUDE_DIR) -I$(THIRDPARTY_INCLUDE_DIR)
LIBS_I     = -I$(POSTOFFICE_LIBS_DIR) -I$(POSTOFFICE_LIBS_DIR)/tui -I$(THIRDPARTY_LIBS_DIR)
CORE_I     = -I$(CORE_SRC_DIR)/main -I$(CORE_SRC_DIR)/simulation
SRC_I      = -I$(SRC_DIR)

APP_MAIN_TUI_LIB_DIR   = $(CORE_SRC_DIR)/main/tui
ALL_I              = $(INCLUDES_I) $(LIBS_I) $(CORE_I) $(SRC_I)

#! set to `true` to enable debug flags
DEV = true

# Security/Harden flags (always enabled)
SECURITY_FLAGS = -fstack-protector-strong -fstack-clash-protection \
-D_FORTIFY_SOURCE=2 -D__USE_FORTIFY_LEVEL=2

# Utility flags (always enabled)
UTILITY_FLAGS = -g -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -fno-omit-frame-pointer \
-fasynchronous-unwind-tables -fno-strict-aliasing

# Optimization flags (always enabled)
OPT_FLAGS = -ffunction-sections -fdata-sections

# Warning flags (only for debug builds)
WARNING_FLAGS = -Wall -Wextra -Werror -Wvla -Wshadow -Wconversion -Wformat=2 \
-Wstrict-overflow -Wcast-align -Wswitch-enum -Wunreachable-code \
-Wnull-dereference -Wdouble-promotion -Wdeprecated-declarations \
-Wformat-security -Wformat-nonliteral

# Debugging and sanitizer flags (only for debug builds)
DEBUGGING_FLAGS = --coverage -O1
SANITIZERS_FLAGS = -fsanitize=address,alignment,shift,null,object-size,undefined,float-divide-by-zero,signed-integer-overflow

# Production flags
PRODUCTION_FLAGS = -O3 -DNDEBUG

NON_DEBUGGING_FLAGS = $(UTILITY_FLAGS) $(OPT_FLAGS) $(SECURITY_FLAGS)

# Third-party libs need optimization to satisfy _FORTIFY_SOURCE checks
THIRDPARTY_CFLAGS = $(NON_DEBUGGING_FLAGS) -O2

# Set CFLAGS: Production always gets utility + optimization + security.
CFLAGS = $(NON_DEBUGGING_FLAGS)
ifeq ($(DEV),true)
	CFLAGS += $(WARNING_FLAGS) $(DEBUGGING_FLAGS) $(SANITIZERS_FLAGS)
else
	CFLAGS += $(PRODUCTION_FLAGS)
endif

# Linker flags
LDFLAGS = -ldl -lrt -lpthread -lm -lncursesw -flto=auto -Wl,--gc-sections \
-fPIE -pie -Wl,-z,relro,-z,now -rdynamic
PROFILER = # strace -e trace=all -o strace.log

# *** Processes *** #

# Process Types
# 1. main: Entry point
# 2. director: Process that manages the entire simulation
# 3. ticket_issuer: Process that manages the ticket issuing system
# 4. users_manager: Process that manages the users processes
# 5. worker: Process that invests the worker's role
# 6. user: Process that invests the user's role
PROJECT_PREFIX 	= post_office_
PROCESSES 		= main director ticket_issuer users_manager worker user
PROCESSES 	   := $(addprefix $(PROJECT_PREFIX), $(PROCESSES))

# Prompt the configuration file to run the simulation with, if not set when make was called
CONFIG ?= $(shell \
	read $(READ_FLAGS) \
		-p "< Makefile: CONFIG not set. Please enter the path for configuration: " CONFIG; \
		printf "%s" "$$CONFIG";)

# Default logger level (TRACE, DEBUG, INFO, WARN, ERROR, FATAL)
LOG_LEVEL ?= TRACE

# Arguments for main process
MAIN_ARGS = --config "$(CONFIG)" --loglevel "$(LOG_LEVEL)"
MAIN_TUI_ARGS = $(MAIN_ARGS) --tui
MAIN_EXEC = $(PROFILER) $(BIN_DIR)/$(PROJECT_PREFIX)main

# Arguments for director process
DIRECTOR_ARGS = --config "$(CONFIG)" --loglevel "$(LOG_LEVEL)" --headless
DIRECTOR_EXEC = $(BIN_DIR)/$(PROJECT_PREFIX)director

# Executables for simulation processes
TICKET_ISSUER_EXEC = $(BIN_DIR)/$(PROJECT_PREFIX)ticket_issuer
WORKER_EXEC = $(BIN_DIR)/$(PROJECT_PREFIX)worker
USERS_MANAGER_EXEC = $(BIN_DIR)/$(PROJECT_PREFIX)users_manager

# Arguments for user process
USER_ARGS =
USER_EXEC = $(BIN_DIR)/$(PROJECT_PREFIX)user

# Arguments for test main process
TEST_MAIN_ARGS = -v # Unity verbose
TEST_MAIN_EXEC = $(BIN_DIR)/$(PROJECT_PREFIX)test_main

# *** Source Files *** #

# Libraries Files
## Post Office Libraries

HASHTABLE_SRC   = $(wildcard $(HASHTABLE_DIR)/*.c)
HASHSET_SRC     = $(wildcard $(HASHSET_DIR)/*.c)
NET_SRC         = $(wildcard $(NET_DIR)/*.c)
PERF_SRC        = $(wildcard $(PERF_DIR)/*.c)
METRICS_SRC     = $(wildcard $(METRICS_DIR)/*.c)
PRIME_SRC       = $(wildcard $(PRIME_DIR)/*.c)
STORAGE_SRC     = $(wildcard $(STORAGE_DIR)/*.c)
UTILS_SRC       = $(wildcard $(UTILS_DIR)/*.c)
RANDOM_SRC      = $(wildcard $(RANDOM_DIR)/*.c)
LOG_SRC         = $(wildcard $(LOG_DIR)/*.c)
SYSINFO_SRC     = $(wildcard $(SYSINFO_DIR)/*.c)
BACKTRACE_SRC     = $(wildcard $(BACKTRACE_DIR)/*.c)
VECTOR_SRC        = $(wildcard $(VECTOR_DIR)/*.c)

## Third-party Libraries
INIH_SRC        = $(wildcard $(INIH_DIR)/*.c)
LIBFORT_SRC	    = $(wildcard $(LIBFORT_DIR)/*.c)
LMDB_SRC	    = $(wildcard $(LMDB_DIR)/*.c)
LOG_C_SRC       = $(wildcard $(LOG_C_DIR)/*.c)
UNITY_SRC       = $(wildcard $(UNITY_DIR)/*.c)

# Source Files for each process
## NOTE:
# Use non-recursive collection for MAIN and DIRECTOR to avoid feeding nested
# paths to static pattern rules (which only allow a single-directory stem).
# Nested trees (main/tui, director/ctrl_bridge) are handled separately below.
MAIN_APP_SRC          = $(wildcard $(MAIN_DIR)/*.c)
MAIN_SIM_SRC          = $(wildcard $(MAIN_SIM_DIR)/*.c)
TUI_SRC_UNNORMALIZED  = $(shell find $(TUI_LIB_DIR) -name '*.c')
APP_MAIN_TUI_SRC_UNNORMALIZED = $(shell find $(APP_MAIN_TUI_LIB_DIR) -name '*.c')
TUI_SRC               = $(patsubst ./%,%,$(TUI_SRC_UNNORMALIZED))
APP_MAIN_TUI_SRC      = $(patsubst ./%,%,$(APP_MAIN_TUI_SRC_UNNORMALIZED))
DIRECTOR_APP_SRC      = $(wildcard $(DIRECTOR_DIR)/*.c)
# Director control bridge (nested) sources (kept separate to preserve directory layout)
DIRECTOR_BRIDGE_DIR   = $(DIRECTOR_DIR)/ctrl_bridge
DIRECTOR_BRIDGE_SRC   = $(wildcard $(DIRECTOR_BRIDGE_DIR)/*.c)
# Director scheduler (nested) sources
DIRECTOR_SCHED_DIR   = $(DIRECTOR_DIR)/schedule
DIRECTOR_SCHED_SRC   = $(wildcard $(DIRECTOR_SCHED_DIR)/*.c)
TICKET_ISSUER_APP_SRC = $(shell find $(TICKET_ISSUER_DIR) -name '*.c')
USERS_MANAGER_APP_SRC = $(shell find $(USERS_MANAGER_DIR) -name '*.c')
WORKER_APP_SRC        = $(shell find $(WORKER_DIR) -name '*.c')
USER_APP_SRC          = $(shell find $(USER_DIR) -name '*.c')
SIM_IPC_SRC           = $(wildcard $(SIM_IPC_DIR)/*.c)
TEST_SRC              = $(shell find $(TESTS_DIR) -name '*.c') \
						$(filter-out $(MAIN_DIR)/main.c,$(MAIN_APP_SRC)) \
						$(filter-out $(DIRECTOR_DIR)/director.c,$(DIRECTOR_APP_SRC)) \
						$(filter-out $(TICKET_ISSUER_DIR)/ticket_issuer.c,$(TICKET_ISSUER_APP_SRC)) \
						$(filter-out $(USERS_MANAGER_DIR)/users_manager.c,$(USERS_MANAGER_APP_SRC)) \
						$(filter-out $(WORKER_DIR)/worker.c,$(WORKER_APP_SRC)) \
						$(filter-out $(USER_DIR)/user.c,$(USER_APP_SRC)) \


# *** Object Files *** #

# Object Files
## Post Office Libraries

HASHTABLE_OBJS      = $(patsubst $(HASHTABLE_DIR)/%.c,$(BUILD_DIR)/hashtable/%.o,$(HASHTABLE_SRC))
HASHSET_OBJS        = $(patsubst $(HASHSET_DIR)/%.c,$(BUILD_DIR)/hashset/%.o,$(HASHSET_SRC))
NET_OBJS            = $(patsubst $(NET_DIR)/%.c,$(BUILD_DIR)/net/%.o,$(NET_SRC))
PERF_OBJS           = $(patsubst $(PERF_DIR)/%.c,$(BUILD_DIR)/perf/%.o,$(PERF_SRC))
METRICS_OBJS        = $(patsubst $(METRICS_DIR)/%.c,$(BUILD_DIR)/metrics/%.o,$(METRICS_SRC))
PRIME_OBJS          = $(patsubst $(PRIME_DIR)/%.c,$(BUILD_DIR)/prime/%.o,$(PRIME_SRC))
STORAGE_OBJS        = $(patsubst $(STORAGE_DIR)/%.c,$(BUILD_DIR)/storage/%.o,$(STORAGE_SRC))
UTILS_OBJS          = $(patsubst $(UTILS_DIR)/%.c,$(BUILD_DIR)/utils/%.o,$(UTILS_SRC))
RANDOM_OBJS         = $(patsubst $(RANDOM_DIR)/%.c,$(BUILD_DIR)/random/%.o,$(RANDOM_SRC))
LOG_OBJS            = $(patsubst $(LOG_DIR)/%.c,$(BUILD_DIR)/log/%.o,$(LOG_SRC))
SYSINFO_OBJS        = $(patsubst $(SYSINFO_DIR)/%.c,$(BUILD_DIR)/sysinfo/%.o,$(SYSINFO_SRC))
BACKTRACE_OBJS      = $(patsubst $(BACKTRACE_DIR)/%.c,$(BUILD_DIR)/backtrace/%.o,$(BACKTRACE_SRC))
VECTOR_OBJS         = $(patsubst $(VECTOR_DIR)/%.c,$(BUILD_DIR)/vector/%.o,$(VECTOR_SRC))

## Third-party Libraries
INIH_OBJS           = $(patsubst $(INIH_DIR)/%.c,$(BUILD_DIR)/inih/%.o,$(INIH_SRC))
LOG_C_OBJS          = $(patsubst $(LOG_C_DIR)/%.c,$(BUILD_DIR)/log_c/%.o,$(LOG_C_SRC))
UNITY_OBJS          = $(patsubst $(UNITY_DIR)/%.c,$(BUILD_DIR)/unity/%.o,$(UNITY_SRC))
LMDB_OBJS			= $(patsubst $(LMDB_DIR)/%.c,$(BUILD_DIR)/lmdb/%.o,$(LMDB_SRC))
LIBFORT_OBJS        = $(patsubst $(LIBFORT_DIR)/%.c,$(BUILD_DIR)/libfort/%.o,$(LIBFORT_SRC))

# Common library objects needed by most executables
DEFAULTS_OBJS       = $(INIH_OBJS) $(UTILS_OBJS) $(RANDOM_OBJS) $(HASHTABLE_OBJS) \
                      $(PRIME_OBJS) $(LOG_C_OBJS) $(METRICS_OBJS) \
					  $(NET_OBJS) $(PERF_OBJS) $(STORAGE_OBJS) \
					  $(LMDB_OBJS) $(HASHSET_OBJS) $(LOG_OBJS) $(SYSINFO_OBJS) \
					  $(BACKTRACE_OBJS) $(VECTOR_OBJS)

# Application-specific objects
MAIN_APP_OBJS 			= $(patsubst $(MAIN_DIR)/%.c,$(BUILD_DIR)/main/%.o,$(MAIN_APP_SRC))
MAIN_SIM_OBJS 			= $(patsubst $(MAIN_SIM_DIR)/%.c,$(BUILD_DIR)/main/simulation/%.o,$(MAIN_SIM_SRC))
TUI_OBJS              = $(patsubst $(TUI_LIB_DIR)/%.c,$(BUILD_DIR)/tui/%.o,$(TUI_SRC))
APP_MAIN_TUI_OBJS     = $(patsubst $(APP_MAIN_TUI_LIB_DIR)/%.c,$(BUILD_DIR)/app_tui/%.o,$(APP_MAIN_TUI_SRC))
DIRECTOR_APP_OBJS 		= $(patsubst $(DIRECTOR_DIR)/%.c,$(BUILD_DIR)/director/%.o,$(DIRECTOR_APP_SRC))
DIRECTOR_BRIDGE_OBJS     = $(patsubst $(DIRECTOR_BRIDGE_DIR)/%.c,$(BUILD_DIR)/director/ctrl_bridge/%.o,$(DIRECTOR_BRIDGE_SRC))
DIRECTOR_SCHED_OBJS      = $(patsubst $(DIRECTOR_SCHED_DIR)/%.c,$(BUILD_DIR)/director/schedule/%.o,$(DIRECTOR_SCHED_SRC))
TICKET_ISSUER_APP_OBJS 	= $(patsubst $(TICKET_ISSUER_DIR)/%.c,$(BUILD_DIR)/ticket_issuer/%.o,$(TICKET_ISSUER_APP_SRC))
USERS_MANAGER_APP_OBJS 	= $(patsubst $(USERS_MANAGER_DIR)/%.c,$(BUILD_DIR)/users_manager/%.o,$(USERS_MANAGER_APP_SRC))
WORKER_APP_OBJS 		= $(patsubst $(WORKER_DIR)/%.c,$(BUILD_DIR)/worker/%.o,$(WORKER_APP_SRC))
USER_APP_OBJS 			= $(patsubst $(USER_DIR)/%.c,$(BUILD_DIR)/user/%.o,$(USER_APP_SRC))
SIM_IPC_OBJS          = $(patsubst $(SIM_IPC_DIR)/%.c,$(BUILD_DIR)/sim_ipc/%.o,$(SIM_IPC_SRC))
TEST_ONLY_SRCS         = $(shell find $(TESTS_DIR) -name '*.c')
TEST_OBJS              = $(patsubst $(TESTS_DIR)/%.c,$(BUILD_DIR)/tests/%.o,$(TEST_ONLY_SRCS))

# Full dependency list for each final executable
MAIN_OBJS           = $(MAIN_APP_OBJS) $(MAIN_SIM_OBJS) $(TUI_OBJS) $(APP_MAIN_TUI_OBJS) $(DEFAULTS_OBJS) $(LIBFORT_OBJS)
DIRECTOR_OBJS 		= $(DIRECTOR_APP_OBJS) $(DIRECTOR_BRIDGE_OBJS) $(DIRECTOR_SCHED_OBJS) $(SIM_IPC_OBJS) $(DEFAULTS_OBJS) $(LMDB_OBJS)
TICKET_ISSUER_OBJS 	= $(TICKET_ISSUER_APP_OBJS) $(SIM_IPC_OBJS) $(DEFAULTS_OBJS) $(LMDB_OBJS)
USERS_MANAGER_OBJS 	= $(USERS_MANAGER_APP_OBJS) $(SIM_IPC_OBJS) $(DEFAULTS_OBJS) $(LMDB_OBJS)
WORKER_OBJS 		= $(WORKER_APP_OBJS) $(SIM_IPC_OBJS) $(DEFAULTS_OBJS)
USER_OBJS 			= $(USER_APP_OBJS) $(SIM_IPC_OBJS) $(DEFAULTS_OBJS)
USER_LIB_OBJS       = $(filter-out $(BUILD_DIR)/user/user.o, $(USER_APP_OBJS))

# Application objects (except entrypoints) to link into tests
APP_ALL_OBJS       = $(MAIN_APP_OBJS) $(MAIN_SIM_OBJS) $(TUI_OBJS) $(APP_MAIN_TUI_OBJS) $(DIRECTOR_APP_OBJS) $(DIRECTOR_BRIDGE_OBJS) $(DIRECTOR_SCHED_OBJS) \
                     $(TICKET_ISSUER_APP_OBJS) $(USERS_MANAGER_APP_OBJS) $(WORKER_APP_OBJS) $(USER_APP_OBJS)
APP_ENTRYPOINT_OBJS = $(BUILD_DIR)/main/main.o $(BUILD_DIR)/director/director.o $(BUILD_DIR)/ticket_issuer/ticket_issuer.o \
                      $(BUILD_DIR)/users_manager/users_manager.o $(BUILD_DIR)/worker/worker.o $(BUILD_DIR)/user/user.o
APP_COMMON_OBJS    = $(filter-out $(APP_ENTRYPOINT_OBJS),$(APP_ALL_OBJS))

TEST_MAIN_OBJS 		= $(TEST_OBJS) $(APP_COMMON_OBJS) $(SIM_IPC_OBJS) $(DEFAULTS_OBJS) $(UNITY_OBJS)

# *** Targets *** #

# Targets for executables
ALL_TARGETS = $(addprefix $(BIN_DIR)/, $(PROCESSES))
# Remove Main process from the list of targets
SIMULATION_TARGETS = $(filter-out $(MAIN_EXEC), $(ALL_TARGETS))

# Rules
.PHONY: start simulation all _simulation-only dev test compile_commands doc clean-doc _clean-build _clean-logs _clean-crash-reports clean help clean-coverage start-tui

# Default target: start the application automatically
start:
	@$(MAKE) all -j
	@echo "> Makefile: Running the main process..."
	@echo
	@($(MAIN_EXEC) $(MAIN_ARGS) && \
	(echo "> Makefile: Main process exited successfully.")) || \
		(EXIT_CODE=$$? && echo "> Makefile: ERROR: Main process failed with exit code $$EXIT_CODE"; exit $$EXIT_CODE)

# Start the main process with TUI
start-tui:
	@$(MAKE) all -j
	@echo "> Makefile: Running the main process with TUI..."
	@echo
	@($(MAIN_EXEC) $(MAIN_TUI_ARGS) && \
	(echo "> Makefile: Main process with TUI exited successfully.")) || \
		(EXIT_CODE=$$? && echo "> Makefile: ERROR: Main process with TUI failed with exit code $$EXIT_CODE"; exit $$EXIT_CODE)

# Run the simulation
simulation: _simulation-only
	@echo "> Makefile: Running the simulation (Director process)..."
	@$(DIRECTOR_EXEC) $(DIRECTOR_ARGS) || \
		(EXIT_CODE=$$? && echo "> Makefile: ERROR: Director process failed with exit code $$EXIT_CODE"; exit $$EXIT_CODE)

# Run the users manager to add `N_NEW_USERS` users (sends SIGUSR1)
add-users:
	@echo "> Makefile: Sending add-users signal..."
	@pkill -USR1 -f "bin/post_office_users_manager" || echo "> Makefile: ERROR: Users Manager not running"

# Remove `N_NEW_USERS` users (sends SIGUSR2)
rem-users:
	@echo "> Makefile: Sending remove-users signal..."
	@pkill -USR2 -f "bin/post_office_users_manager" || echo "> Makefile: ERROR: Users Manager not running"

# Run the simulation with TIMEOUT configuration
timeout:
	@echo "> Makefile: Running simulation with TIMEOUT configuration..."
	@$(MAKE) start CONFIG="$(TIMEOUT_INI_PATH)"

# Run the simulation with EXPLODE configuration
explode:
	@echo "> Makefile: Running simulation with EXPLODE configuration..."
	@$(MAKE) start CONFIG="$(EXPLODE_INI_PATH)"

# Compile all executables
all: $(ALL_TARGETS)

# In case of a sudden unhandled crash of the application, not to
# remove manually all the leftovers from the previous run.
reset-env:
	@echo "> Makefile: Resetting the environment for a new run..."
	@if pgrep -a -f '^$(BIN_DIR)/$(PROJECT_PREFIX)'; then \
		echo "> Makefile: These are the processes that should be killed."; \
		read -p "< Makefile: Do you want to kill them? (y/N) " yn; \
		if [ "$$yn" = "y" ] || [ "$$yn" = "Y" ]; then \
			pkill -f -KILL '^$(BIN_DIR)/$(PROJECT_PREFIX)'; \
			echo "> Makefile: Killed all matching processes."; \
		else \
			echo "> Makefile: No processes were killed."; \
		fi \
	else \
		echo "> Makefile: No matching processes found."; \
	fi
	@ipcs
	@echo "> Makefile: These are the IPC resources that should be removed."
	@read -p "< Makefile: Do you want to remove them? (y/N) " yn; \
	if [ "$$yn" = "y" ] || [ "$$yn" = "Y" ]; then \
		ipcrm -a; \
		echo "> Makefile: Removed all IPC resources."; \
	else \
		echo "> Makefile: No IPC resources were removed."; \
	fi
	@echo "> Makefile: Environment reset complete!"

# Compile only simulation executables
_simulation-only: $(SIMULATION_TARGETS)

compile_commands: clean
	@echo "> Makefile: Generating LSPs' compile_commands.json..."
	@bear -- $(MAKE) -j all

# Build for development
dev: clean
	@echo "> Makefile: Building for development..."
	@$(MAKE) compile_commands
	@echo "> Makefile: Running tests..."
	@$(MAKE) test
	@echo "> Makefile: Ready for development!"

# Build and run tests
test: $(TEST_MAIN_EXEC)
	@echo "> Makefile: Running tests..."
	@$(TEST_MAIN_EXEC) $(TEST_MAIN_ARGS)

# Rule for generating Doxygen documentation
doc:
	@echo "> Makefile: Make sure to have Graphviz/dot installed."
	@echo "> Makefile: Generating Doxygen documentation..."
	doxygen Doxyfile
	@echo "> Makefile: Doxygen documentation generated in LaTeX and HTML formats."
	@echo "> Makefile: To generate the documentation in PDF, run: cd $(DOCS_DIR)/latex && make"

# Clean Doxygen documentation
clean-doc:
	@echo "> Makefile: Cleaning Doxygen documentation..."
	rm -rf $(DOCS_DIR)/html $(DOCS_DIR)/latex

# Clean build artifacts
_clean-build:
	@echo "> Makefile: Cleaning build artifacts..."
	rm -rf $(BUILD_DIR) $(BIN_DIR) compile_commands.json

# Clean logs
_clean-logs:
	@echo "> Makefile: Cleaning logs..."
	rm -rf $(LOGS_DIR)

# Clean crash reports
_clean-crash-reports:
	@echo "> Makefile: Cleaning crash reports..."
	rm -rf $(CRASH_REPORTS_DIR)

# Clean all
clean: _clean-build _clean-logs _clean-crash-reports clean-doc
	@echo "> Makefile: Cleaned all non-repo files."

# Remove stale coverage artifacts to avoid gcov overwrite warnings
.PHONY: clean-coverage
clean-coverage:
	@echo "> Makefile: Cleaning coverage artifacts (*.gcda/*.gcno, coverage.info)..."
	@find $(BUILD_DIR) -type f \( -name '*.gcda' -o -name '*.gcno' \) -print -delete || true
	@rm -f $(BUILD_DIR)/coverage.info || true
	@echo "> Makefile: Coverage artifacts cleaned."

# Display help message
help:
	@echo "Usage: make [start|start-tui|simulation|add-users|timeout|explode|all|reset-env|doc|clean-doc|clean|dev|test|help]"
	@echo "    start: Start the application (default target)"
	@echo "    start-tui: Start the application with Text User Interface (TUI)"
	@echo "    simulation: Run the simulation (Director process)"
	@echo "    add-users: Run the Users Manager to add new users"
	@echo "    timeout: Run the simulation with TIMEOUT configuration"
	@echo "    explode: Run the simulation with EXPLODE configuration"
	@echo "    all: Compile all executables (no tests)"
	@echo "    reset-env: Reset the environment for a new run (clean FIFOs, kill processes, remove IPC resources)"
	@echo "    doc: Generate Doxygen documentation"
	@echo "    clean-doc: Clean Doxygen documentation"
	@echo "    clean: Remove non-repo files"
	@echo "    dev: Compile for development (clean, compile_commands, test)"
	@echo "    test: Compile and run test executable"
	@echo "    clean-coverage: Remove stale gcov (*.gcda/*.gcno) and coverage.info files"
	@echo "    help: Display this help message"
	@echo "    tools: Build tools under tools/ (helper utilities)"
	@echo "    format: Run clang-format on all .c/.h files under this directory (recursively)"
	@echo "    tidy:   Run clang-tidy on first-party sources (read-only analysis)"
	@echo "    tidy-fix: Apply clang-tidy auto-fixes to first-party sources (braces, else-after-return, simplify-bool)"

# *** Rules *** #

# Create executables
$(MAIN_EXEC): $(MAIN_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(ALL_I) -o $@ $^ $(LDFLAGS)

$(DIRECTOR_EXEC): $(DIRECTOR_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(ALL_I) -o $@ $^ $(LDFLAGS)

$(TICKET_ISSUER_EXEC): $(TICKET_ISSUER_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(ALL_I) -o $@ $^ $(LDFLAGS)

$(USERS_MANAGER_EXEC): $(USERS_MANAGER_OBJS) $(USER_LIB_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(ALL_I) -o $@ $^ $(LDFLAGS)

$(WORKER_EXEC): $(WORKER_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(ALL_I) -o $@ $^ $(LDFLAGS)

$(USER_EXEC): $(USER_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(ALL_I) -o $@ $^ $(LDFLAGS)

$(TEST_MAIN_EXEC): $(TEST_MAIN_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(ALL_I) -o $@ $^ $(LDFLAGS)

# --- Static Pattern Rules for Compilation ---
# This is the robust way to compile object files. Each rule is explicitly
# tied to the list of objects it's responsible for building.

# Rule for main application objects
$(BUILD_DIR)/main/%.o: $(MAIN_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES_I) $(CORE_I) $(SRC_I) -c $< -o $@

# Rule for main simulation objects
$(BUILD_DIR)/main/simulation/%.o: $(MAIN_SIM_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES_I) $(CORE_I) $(SRC_I) -c $< -o $@

# Rule for director application objects
$(BUILD_DIR)/director/%.o: $(DIRECTOR_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES_I) $(CORE_I) $(SRC_I) -c $< -o $@

# Rule for director control bridge objects
$(BUILD_DIR)/director/ctrl_bridge/%.o: $(DIRECTOR_BRIDGE_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES_I) $(CORE_I) $(SRC_I) -c $< -o $@

# Rule for director scheduler objects
$(BUILD_DIR)/director/schedule/%.o: $(DIRECTOR_SCHED_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES_I) $(CORE_I) $(SRC_I) -c $< -o $@

# Rule for ticket_issuer application objects
$(BUILD_DIR)/ticket_issuer/%.o: $(TICKET_ISSUER_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES_I) $(CORE_I) $(SRC_I) -c $< -o $@

# Rule for users_manager application objects
$(BUILD_DIR)/users_manager/%.o: $(USERS_MANAGER_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES_I) $(CORE_I) $(SRC_I) -c $< -o $@

# Rule for utils objects
$(BUILD_DIR)/utils/%.o: $(UTILS_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES_I) $(CORE_I) $(SRC_I) -I$(INCLUDE_DIR) -c $< -o $@

# Rule for worker application objects
$(BUILD_DIR)/worker/%.o: $(WORKER_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES_I) $(CORE_I) $(SRC_I) -c $< -o $@

# Rule for user application objects
$(BUILD_DIR)/user/%.o: $(USER_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES_I) $(CORE_I) $(SRC_I) -c $< -o $@

# Rule for simulation IPC objects
$(BUILD_DIR)/sim_ipc/%.o: $(SIM_IPC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES_I) $(CORE_I) $(SRC_I) -c $< -o $@

# Rule for shared TUI library objects
$(BUILD_DIR)/tui/%.o: $(TUI_LIB_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(ALL_I) -c $< -o $@

# Rule for app main TUI objects
$(BUILD_DIR)/app_tui/%.o: $(APP_MAIN_TUI_LIB_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(ALL_I) -c $< -o $@

# Rule for our internal libraries
$(BUILD_DIR)/hashtable/%.o: $(HASHTABLE_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(ALL_I) -c $< -o $@

$(BUILD_DIR)/hashset/%.o: $(HASHSET_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(ALL_I) -c $< -o $@

$(BUILD_DIR)/net/%.o: $(NET_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(ALL_I) -c $< -o $@

$(BUILD_DIR)/perf/%.o: $(PERF_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(ALL_I) -c $< -o $@

$(BUILD_DIR)/metrics/%.o: $(METRICS_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(ALL_I) -c $< -o $@

$(BUILD_DIR)/prime/%.o: $(PRIME_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(ALL_I) -c $< -o $@

$(BUILD_DIR)/storage/%.o: $(STORAGE_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(ALL_I) -c $< -o $@

$(BUILD_DIR)/utils/%.o: $(UTILS_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES_I) $(CORE_I) $(SRC_I) -c $< -o $@

$(BUILD_DIR)/random/%.o: $(RANDOM_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(ALL_I) -c $< -o $@

$(BUILD_DIR)/log/%.o: $(LOG_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(ALL_I) -c $< -o $@

$(BUILD_DIR)/sysinfo/%.o: $(SYSINFO_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(ALL_I) -c $< -o $@

$(BUILD_DIR)/backtrace/%.o: $(BACKTRACE_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(ALL_I) -c $< -o $@

$(BUILD_DIR)/vector/%.o: $(VECTOR_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(ALL_I) -c $< -o $@

# Rule for all test files
$(BUILD_DIR)/tests/%.o: $(TESTS_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(ALL_I) -DUNITY_FIXTURE_NO_EXTRAS -c $< -o $@

# Rules for third-party libraries
$(BUILD_DIR)/inih/%.o: $(INIH_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(ALL_I) -DINI_ALLOW_NO_VALUE -DINI_STOP_ON_FIRST_ERROR -DINI_ALLOW_MULTILINE -DINI_ALLOW_REALLOC -DINI_USE_STACK=0 -DINI_MAX_LINE=1024 -c $< -o $@

$(BUILD_DIR)/libfort/%.o: $(LIBFORT_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(THIRDPARTY_CFLAGS) $(ALL_I) -c $< -o $@

$(BUILD_DIR)/lmdb/%.o: $(LMDB_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(THIRDPARTY_CFLAGS) $(ALL_I) -c $< -o $@

$(BUILD_DIR)/log_c/%.o: $(LOG_C_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(ALL_I) -DLOG_USE_COLOR -c $< -o $@

$(BUILD_DIR)/unity/%.o: $(UNITY_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(ALL_I) -c $< -o $@

# --- End of Static Pattern Rules ---

# Include dependencies generated by -MMD
-include $(shell find $(BUILD_DIR) -name '*.d' 2>/dev/null)

# Create directories if not exist
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# --- Tools ---

# Helper development utilities (single-source C files in tools/, binaries in bin/)
TOOLS_DIR     := tools
TOOL_SRCS     := $(wildcard $(TOOLS_DIR)/*.c)
TOOL_NAMES    := $(basename $(notdir $(TOOL_SRCS)))
TOOL_BINS     := $(addprefix $(BIN_DIR)/, $(TOOL_NAMES))

.PHONY: tools
tools: $(TOOL_BINS)
	@echo "> Makefile: Built tools into $(BIN_DIR): $(notdir $(TOOL_BINS))"

# Link each tool against our default library objects so headers resolve and symbols link.
# Example: random_hist depends on utils/random; sysinfo_dump on sysinfo; logger_smoke on log/perf.
$(BIN_DIR)/%: $(TOOLS_DIR)/%.c $(DEFAULTS_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(ALL_I) -I$(INCLUDE_DIR) $< $(DEFAULTS_OBJS) -o $@

.PHONY: clean-tools
clean-tools:
	@echo "> Makefile: Scanning $(TOOLS_DIR) for non-.c files..."
	@TMP=$$(mktemp); \
	find $(TOOLS_DIR) -type f ! -name '*.c' -print | tee $$TMP; \
	if [ ! -s $$TMP ]; then \
		echo "> Makefile: Nothing to delete."; \
		rm -f $$TMP; \
		exit 0; \
	fi; \
	read -p "< Makefile: Delete the files listed above? (y/N) " yn; \
	if [ "$$yn" = "y" ] || [ "$$yn" = "Y" ]; then \
		xargs -r -d '\n' rm -f -- < $$TMP; \
		echo "> Makefile: Deleted non-.c files under $(TOOLS_DIR)."; \
	else \
		echo "> Makefile: Aborted."; \
	fi; \
	rm -f $$TMP

# --- Formatting ---

.PHONY: format
format:
	@echo "> Makefile: Formatting all .c/.h files..."
	@if ! command -v clang-format >/dev/null 2>&1; then \
		echo "> Makefile: ERROR: clang-format not found. Please install it and retry."; \
		exit 1; \
	fi
	@FILES="$$(find . -type f \( -name '*.c' -o -name '*.h' \))"; \
	if [ -z "$$FILES" ]; then \
		echo "> Makefile: No .c/.h files found."; \
	else \
		echo "> Makefile: Running: clang-format -i $$FILES"; \
		clang-format -i $$FILES; \
		echo "> Makefile: clang-format completed for all files."; \
	fi

# --- Clang-Tidy (readability refactors) ---
.PHONY: tidy
tidy:
	@echo "> Makefile: Running clang-tidy (read-only analysis) on first-party sources..."
	@if ! command -v clang-tidy >/dev/null 2>&1; then \
		echo "> Makefile: ERROR: clang-tidy not found. Please install it and retry."; \
		exit 1; \
	fi
	@SRCS=$$(find . \
	 -path './libs/thirdparty' -prune -o \
	 -path './include/thirdparty' -prune -o \
	 -name '*.c' -print); \
	 [ -n "$$SRCS" ] || { echo "> Makefile: No C sources found."; exit 0; }; \
	 echo "> Makefile: Files:"; echo "$$SRCS" | tr ' ' '\n'; \
	 clang-tidy --format-style=file --config-file=.clang-tidy $$SRCS -- -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -Iinclude -Iinclude/postoffice -Ilibs/postoffice -Isrc -Ilibs/thirdparty -Iinclude/thirdparty || true; \
	 echo "> Makefile: clang-tidy completed."

.PHONY: tidy-fix
tidy-fix:
	@echo "> Makefile: Applying clang-tidy auto-fixes on first-party sources..."
	@if ! command -v clang-tidy >/dev/null 2>&1; then \
		echo "> Makefile: ERROR: clang-tidy not found. Please install it and retry."; \
		exit 1; \
	fi
	@SRCS=$$(find . \
	 -path './libs/thirdparty' -prune -o \
	 -path './include/thirdparty' -prune -o \
	 -name '*.c' -print); \
	 [ -n "$$SRCS" ] || { echo "> Makefile: No C sources found."; exit 0; }; \
	 echo "> Makefile: Files:"; echo "$$SRCS" | tr ' ' '\n'; \
	 clang-tidy -fix -fix-errors --format-style=file --config-file=.clang-tidy $$SRCS -- -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -Iinclude -Iinclude/postoffice -Ilibs/postoffice -Isrc -Ilibs/thirdparty -Iinclude/thirdparty || true; \
	 echo "> Makefile: clang-tidy fixes applied. Consider running 'make -C app format' afterwards."
