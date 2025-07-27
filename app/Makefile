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
CORE_DIR               = $(POSTOFFICE_LIBS_DIR)/core
HASHTABLE_DIR          = $(POSTOFFICE_LIBS_DIR)/hashtable
NET_DIR                = $(POSTOFFICE_LIBS_DIR)/net
PERF_DIR			   = $(POSTOFFICE_LIBS_DIR)/perf
PRIME_DIR              = $(POSTOFFICE_LIBS_DIR)/prime
STORAGE_DIR			   = $(POSTOFFICE_LIBS_DIR)/storage
UTILS_DIR			   = $(POSTOFFICE_LIBS_DIR)/utils

## Third-party Libraries
INIH_DIR               = $(THIRDPARTY_LIBS_DIR)/inih
LIBFORT_DIR            = $(THIRDPARTY_LIBS_DIR)/libfort
LMDB_DIR               = $(THIRDPARTY_LIBS_DIR)/lmdb
LOG_C_DIR              = $(THIRDPARTY_LIBS_DIR)/log_c
UNITY_DIR              = $(THIRDPARTY_LIBS_DIR)/unity

# Processes' sources directories
MAIN_DIR               = $(SRC_DIR)/core/main
SIMULATION_DIR         = $(SRC_DIR)/core/simulation
DIRECTOR_DIR           = $(SIMULATION_DIR)/director
TICKET_ISSUER_DIR      = $(SIMULATION_DIR)/ticket_issuer
USERS_MANAGER_DIR      = $(SIMULATION_DIR)/users_manager
WORKER_DIR             = $(SIMULATION_DIR)/worker
USER_DIR               = $(SIMULATION_DIR)/user

# Configuration files
TIMEOUT_INI_PATH       = $(CONFIG_DIR)/timeout.ini
EXPLODE_INI_PATH       = $(CONFIG_DIR)/explode.ini

# *** Compiler flags *** #

INCLUDES = -I$(POSTOFFICE_INCLUDE_DIR) -I$(THIRDPARTY_INCLUDE_DIR) -I$(POSTOFFICE_LIBS_DIR) \
-I$(THIRDPARTY_LIBS_DIR) -I$(SRC_DIR) -I$(SIMULATION_DIR)

#! set to `true` to enable debug flags
DEV = true

# Security/Harden flags (always enabled)
SECURITY_FLAGS = -fstack-protector-strong -fstack-clash-protection \
-D_FORTIFY_SOURCE=2 -D__USE_FORTIFY_LEVEL=2

# Utility flags (always enabled)
#? -D_POSIX_C_SOURCE=200809L
UTILITY_FLAGS = -g -D_GNU_SOURCE -fno-omit-frame-pointer \
-fasynchronous-unwind-tables -fno-strict-aliasing

# Optimization flags (always enabled)
OPT_FLAGS = -ffunction-sections -fdata-sections

# Warning flags (only for debug builds)
WARNING_FLAGS = -Wall -Wextra -Werror -Wvla -Wshadow -Wconversion -Wformat=2 \
-Wstrict-overflow -Wcast-align -Wswitch-enum -Wunreachable-code \
-Wnull-dereference -Wdouble-promotion -Wdeprecated-declarations \
-Wformat-security -Wformat-nonliteral

# Debugging and sanitizer flags (only for debug builds)
DEBUGGING_FLAGS = --coverage -O1 -fsanitize=address,alignment,shift,null,object-size,undefined,float-divide-by-zero,signed-integer-overflow

# Production flags
PRODUCTION_FLAGS = -O3 -DNDEBUG

# Set CFLAGS: Production always gets utility + optimization + security.
CFLAGS = $(UTILITY_FLAGS) $(OPT_FLAGS) $(SECURITY_FLAGS)
ifeq ($(DEV),true)
	CFLAGS += $(WARNING_FLAGS) $(DEBUGGING_FLAGS)
else
	CFLAGS += $(PRODUCTION_FLAGS)
endif

# Linker flags
LDFLAGS = -lrt -lpthread -flto=auto -Wl,--gc-sections \
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

# Director PID (used by users manager)
DIRECTOR_PID ?= $(shell pidof -s "$(BIN_DIR)/$(PROJECT_PREFIX)director")

# Arguments for users manager process
USERS_MANAGER_ARGS = --loglevel "$(LOG_LEVEL)" --pid "$(DIRECTOR_PID)"
USERS_MANAGER_EXEC = $(BIN_DIR)/$(PROJECT_PREFIX)users_manager

# Arguments for main process
MAIN_ARGS = --config "$(CONFIG)" --loglevel "$(LOG_LEVEL)"
MAIN_EXEC = $(PROFILER) $(BIN_DIR)/$(PROJECT_PREFIX)main

# Arguments for director process
DIRECTOR_ARGS = --config "$(CONFIG)" --loglevel "$(LOG_LEVEL)"
DIRECTOR_EXEC = $(BIN_DIR)/$(PROJECT_PREFIX)director

# Arguments for ticket issuer process
TICKET_ISSUER_ARGS =
TICKET_ISSUER_EXEC = $(BIN_DIR)/$(PROJECT_PREFIX)ticket_issuer

# Arguments for worker process
WORKER_ARGS =
WORKER_EXEC = $(BIN_DIR)/$(PROJECT_PREFIX)worker

# Arguments for user process
USER_ARGS =
USER_EXEC = $(BIN_DIR)/$(PROJECT_PREFIX)user

# Arguments for test main process
TEST_MAIN_ARGS =
TEST_MAIN_EXEC = $(BIN_DIR)/$(PROJECT_PREFIX)test_main

INIH_FLAGS = -DINI_ALLOW_NO_VALUE -DINI_STOP_ON_FIRST_ERROR

# *** Source Files *** #

# Libraries Files
## Post Office Libraries
CORE_SRC        = $(wildcard $(CORE_DIR)/*.c)
HASHTABLE_SRC   = $(wildcard $(HASHTABLE_DIR)/*.c)
NET_SRC         = $(wildcard $(NET_DIR)/*.c)
PERF_SRC        = $(wildcard $(PERF_DIR)/*.c)
PRIME_SRC       = $(wildcard $(PRIME_DIR)/*.c)
STORAGE_SRC     = $(wildcard $(STORAGE_DIR)/*.c)
UTILS_SRC       = $(wildcard $(UTILS_DIR)/*.c)

## Third-party Libraries
INIH_SRC        = $(wildcard $(INIH_DIR)/*.c)
LIBFORT_SRC	    = $(wildcard $(LIBFORT_DIR)/*.c)
LMDB_SRC	    = $(wildcard $(LMDB_DIR)/*.c)
LOG_C_SRC       = $(wildcard $(LOG_C_DIR)/*.c)
UNITY_SRC       = $(wildcard $(UNITY_DIR)/*.c)

# Source Files for each process
MAIN_SRC            = $(wildcard $(MAIN_DIR)/*.c)
DIRECTOR_SRC        = $(wildcard $(DIRECTOR_DIR)/*.c)
TICKET_ISSUER_SRC   = $(wildcard $(TICKET_ISSUER_DIR)/*.c)
USERS_MANAGER_SRC   = $(wildcard $(USERS_MANAGER_DIR)/*.c)
WORKER_SRC          = $(wildcard $(WORKER_DIR)/*.c)
USER_SRC            = $(wildcard $(USER_DIR)/*.c)
TEST_MAIN_SRC       = $(wildcard $(TESTS_DIR)/**/**/*.c)
TEST_MAIN_SRC      += $(wildcard $(TESTS_DIR)/**/*.c)
TEST_MAIN_SRC      += $(wildcard $(TESTS_DIR)/*.c)

# *** Object Files *** #

# Object Files
## Post Office Libraries
CORE_OBJS           = $(CORE_SRC:$(CORE_DIR)/%.c=$(BUILD_DIR)/%.o)
HASHTABLE_OBJS      = $(HASHTABLE_SRC:$(HASHTABLE_DIR)/%.c=$(BUILD_DIR)/%.o)
NET_OBJS            = $(NET_SRC:$(NET_DIR)/%.c=$(BUILD_DIR)/%.o)
PERF_OBJS           = $(PERF_SRC:$(PERF_DIR)/%.c=$(BUILD_DIR)/%.o)
PRIME_OBJS          = $(PRIME_SRC:$(PRIME_DIR)/%.c=$(BUILD_DIR)/%.o)
STORAGE_OBJS        = $(STORAGE_SRC:$(STORAGE_DIR)/%.c=$(BUILD_DIR)/%.o)
UTILS_OBJS          = $(UTILS_SRC:$(UTILS_DIR)/%.c=$(BUILD_DIR)/%.o)

## Third-party Libraries
INIH_OBJS           = $(INIH_SRC:$(INIH_DIR)/%.c=$(BUILD_DIR)/%.o)
LOG_C_OBJS          = $(LOG_C_SRC:$(LOG_C_DIR)/%.c=$(BUILD_DIR)/%.o)
DEFAULTS_OBJS       = $(INIH_OBJS) $(UTILS_OBJS) $(HASHTABLE_OBJS) \
                      $(PRIME_OBJS) $(LOG_C_OBJS) $(CORE_OBJS) \
					  $(NET_OBJS) $(PERF_OBJS) $(STORAGE_OBJS)

UNITY_OBJS          = $(UNITY_SRC:$(UNITY_DIR)/%.c=$(BUILD_DIR)/%.o)
LMDB_OBJS			= $(LMDB_SRC:$(LMDB_DIR)/%.c=$(BUILD_DIR)/%.o)
LIBFORT_OBJS        = $(LIBFORT_SRC:$(LIBFORT_DIR)/%.c=$(BUILD_DIR)/%.o)

MAIN_OBJS 			= $(MAIN_SRC:$(MAIN_DIR)/%.c=$(BUILD_DIR)/%.o) $(DEFAULTS_OBJS) $(LIBFORT_OBJS)
DIRECTOR_OBJS 		= $(DIRECTOR_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) $(DEFAULTS_OBJS) $(LMDB_OBJS)
TICKET_ISSUER_OBJS 	= $(TICKET_ISSUER_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) $(DEFAULTS_OBJS) $(LMDB_OBJS)
USERS_MANAGER_OBJS 	= $(USERS_MANAGER_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) $(DEFAULTS_OBJS) $(LMDB_OBJS)
WORKER_OBJS 		= $(WORKER_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) $(DEFAULTS_OBJS)
USER_OBJS 			= $(USER_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) $(DEFAULTS_OBJS)
TEST_MAIN_OBJS 		= $(TEST_MAIN_SRC:$(TESTS_DIR)/%.c=$(BUILD_DIR)/%.o) $(DEFAULTS_OBJS) $(UNITY_OBJS)

# *** Targets *** #

# Targets for executables
ALL_TARGETS = $(addprefix $(BIN_DIR)/, $(PROCESSES))
# Remove Main process from the list of targets
SIMULATION_TARGETS = $(filter-out $(MAIN_EXEC), $(ALL_TARGETS))

# Rules
.PHONY: start simulation all _simulation-only dev test _compile_commands doc clean-doc _clean-build _clean-logs _clean-crash-reports clean help

# Default target: start the application automatically
start:
	@$(MAKE) all -j
	@echo "> Makefile: Running the main process..."
	@echo
	@($(MAIN_EXEC) $(MAIN_ARGS) && \
	(echo "> Makefile: Main process exited successfully.")) || \
		(EXIT_CODE=$$? && echo "> Makefile: ERROR: Main process failed with exit code $$EXIT_CODE"; exit $$EXIT_CODE)

# Run the simulation
simulation: _simulation-only
	@echo "> Makefile: Running the simulation (Director process)..."
	@$(DIRECTOR_EXEC) $(DIRECTOR_ARGS) || \
		(EXIT_CODE=$$? && echo "> Makefile: ERROR: Director process failed with exit code $$EXIT_CODE"; exit $$EXIT_CODE)

# Run the users manager to add `N_NEW_USERS` users
add-users: _simulation-only
	@if [ -z "$(DIRECTOR_PID)" ]; then \
		echo "> Makefile: ERROR: Director process is not running. Start the director before adding users."; \
		exit 1; \
	fi
	@echo "> Makefile: Running the Users Manager to add new users..."
	@$(USERS_MANAGER_EXEC) $(USERS_MANAGER_ARGS) || \
		(EXIT_CODE=$$? && echo "> Makefile: ERROR: Users Manager process failed with exit code $$EXIT_CODE"; exit $$EXIT_CODE)

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

# Build for development
dev: clean
	@echo "> Makefile: Building for development..."
	@$(MAKE) _compile_commands
	@echo "> Makefile: Running tests..."
	@$(MAKE) test
	@echo "> Makefile: Ready for development!"

# Build and run tests
test: $(TEST_MAIN_EXEC)
	@echo "> Makefile: Running tests..."
	@$(TEST_MAIN_EXEC) $(TEST_MAIN_ARGS)

# Rule for generating compile_commands.json
_compile_commands: clean
	@echo "> Makefile: Generating compile_commands.json..."
	bear -- $(MAKE) all -j

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

# Display help message
help:
	@echo "Usage: make [start|simulation|add-users|timeout|explode|all|reset-env|doc|clean-doc|clean|dev|test|help]"
	@echo "    start: Start the application (default target)"
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
	@echo "    help: Display this help message"

# *** Rules *** #

# Create executables
$(MAIN_EXEC): $(MAIN_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDES) -o $@ $^

$(DIRECTOR_EXEC): $(DIRECTOR_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDES) -o $@ $^

$(TICKET_ISSUER_EXEC): $(TICKET_ISSUER_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDES) -o $@ $^

$(USERS_MANAGER_EXEC): $(USERS_MANAGER_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDES) -o $@ $^

$(WORKER_EXEC): $(WORKER_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDES) -o $@ $^

$(USER_EXEC): $(USER_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDES) -o $@ $^

$(TEST_MAIN_EXEC): $(TEST_MAIN_OBJS) $(UNITY_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(TESTING_FLAGS) $(LDFLAGS) $(INCLUDES) -o $@ $^

# Compile object files (create *subdirectories* if not exist)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(MAIN_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(CORE_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(HASHTABLE_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(NET_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(PERF_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(PRIME_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(STORAGE_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(UTILS_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(INIH_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(INIH_FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(LIBFORT_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(LMDB_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(LOG_C_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES) -c $< -o $@ -DLOG_USE_COLOR

$(BUILD_DIR)/%.o: $(UNITY_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(TESTS_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP $(CFLAGS) $(INCLUDES) -c $< -o $@

# Include dependencies
-include $(BUILD_DIR)/*.d

# Create directories if not exist
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)
