DIRHEADERS := -Iinclude

# LIBS
GLIB_LIBS := $(shell pkg-config --libs glib-2.0)
LIBNOTIFY_LIBS := $(shell pkg-config --libs libnotify)
LIBACL_LIBS := $(shell pkg-config --libs libacl)
LLIBUDEV := -ludev

# FLAGS
LIBNOTIFY_CFLAGS := $(shell pkg-config --cflags libnotify)
GLIB_CFLAGS := $(shell pkg-config --cflags glib-2.0)

CLIENT_NAME           = balrog
DAEMON_NAME           = balrogd
DAEMON_MAJOR_VERSION  = 0
DAEMON_MINOR_VERSION  = 0
DAEMON_PATCH_VERSION  = *hash
DAEMON_PID_FILE_NAME  = /var/run/$(DAEMON_NAME)/$(DAEMON_NAME).pid
DAEMON_LOG_FILE_NAME  = /var/log/$(DAEMON_NAME)/$(DAEMON_NAME).log
DAEMON_CMD_PIPE_NAME  = /var/run/$(DAEMON_NAME)/$(DAEMON_NAME).cmd
DAEMON_MONITOR_LOG_FILE_NAME = /var/log/$(DAEMON_NAME)/monitor.log
DAEMON_MONITOR_PID_FILE_NAME = /var/run/$(DAEMON_NAME)/monitor.pid
DAEMON_MONITOR_SOCKET_FILE_NAME = /var/run/$(DAEMON_NAME)/balrogd.sock
DAEMON_NO_CHDIR       = 1
DAEMON_NO_FORK        = 0
DAEMON_NO_CLOSE_STDIO = 1
#variants:  hash - set PATCH_VERSION == commit hash (12 digits)
#variants: *hash - set PATCH_VERSION == * + commit hash (12 digits),
#                  * will be set if files in repo are changed
#variants:  xxx  - set PATCH_VERSION == xxx (your variant)

# process PATCH_VERSION
COMMIT_HASH    = $(shell git rev-parse --short=12 HEAD)
CHANGED_FILES  = $(shell git status --short --untracked-files=no | grep -c M)

ifeq ($(strip $(DAEMON_PATCH_VERSION)), *hash)
    ifeq ($(strip $(CHANGED_FILES)), 0)
        DAEMON_PATCH_VERSION = $(COMMIT_HASH)
    else
        DAEMON_PATCH_VERSION = *$(COMMIT_HASH)
    endif
endif


ifeq ($(strip $(DAEMON_PATCH_VERSION)), hash)
    DAEMON_PATCH_VERSION = $(COMMIT_HASH)
endif

CC        ?=  gcc

CONFIG_C_FLAGS     = -DDAEMON_NAME='"$(DAEMON_NAME)"'
CONFIG_C_FLAGS    += -DDAEMON_MAJOR_VERSION=$(DAEMON_MAJOR_VERSION)
CONFIG_C_FLAGS    += -DDAEMON_MINOR_VERSION=$(DAEMON_MINOR_VERSION)
CONFIG_C_FLAGS    += -DDAEMON_PATCH_VERSION=$(DAEMON_PATCH_VERSION)
CONFIG_C_FLAGS    += -DDAEMON_PID_FILE_NAME='"$(DAEMON_PID_FILE_NAME)"'
CONFIG_C_FLAGS    += -DDAEMON_LOG_FILE_NAME='"$(DAEMON_LOG_FILE_NAME)"'
CONFIG_C_FLAGS	  += -DDAEMON_MONITOR_LOG_FILE_NAME='"$(DAEMON_MONITOR_LOG_FILE_NAME)"'
CONFIG_C_FLAGS	  += -DDAEMON_MONITOR_PID_FILE_NAME='"$(DAEMON_MONITOR_PID_FILE_NAME)"'
CONFIG_C_FLAGS	  += -DDAEMON_MONITOR_SOCKET_FILE_NAME='"$(DAEMON_MONITOR_SOCKET_FILE_NAME)"'
CONFIG_C_FLAGS    += -DDAEMON_CMD_PIPE_NAME='"$(DAEMON_CMD_PIPE_NAME)"'
CONFIG_C_FLAGS    += -DDAEMON_NO_CHDIR=$(DAEMON_NO_CHDIR)
CONFIG_C_FLAGS    += -DDAEMON_NO_FORK=$(DAEMON_NO_FORK)
CONFIG_C_FLAGS    += -DDAEMON_NO_CLOSE_STDIO=$(DAEMON_NO_CLOSE_STDIO)
CONFIG_C_FLAGS    += -DCOMMIT_HASH='"$(COMMIT_HASH)"'
CONFIG_C_FLAGS    += -DCHANGED_FILES=$(CHANGED_FILES)

COMPILER_CONFIG_CFLAGS    ?= -O2  -Wall  -pipe -Wextra -pedantic -std=c99

OUTPUT_DIR_RELEASE ?= output_release
OUTPUT_DIR_DEBUG ?= output_debug
COMMON_INCLUDE_DIR ?= include
COMMON_SRC_DIR     ?= src
DEBUG_SUFFIX   ?= debug



# INIT Common commands
BUILD_ECHO = echo "\n  [build]  $@:"

define build_object
    @$(BUILD_ECHO)
    $(CC) -c $< -o $@ $(CFLAGS) $(LDLIBS)
endef

define build_bin
    @$(BUILD_ECHO)
    $(CC)  $1 -o $@ $(CFLAGS) $(LDLIBS)
    @echo "\n---- Compiled $@ ver $(DAEMON_MAJOR_VERSION).$(DAEMON_MINOR_VERSION).$(DAEMON_PATCH_VERSION) ----\n"
endef

ifeq "$(findstring $(MAKECMDGOALS),clean distclean)"  ""
    include $(wildcard .depend)
endif

# Asegurar directorios de salida
$(OUTPUT_DIR_DEBUG):
	mkdir -p $@

$(OUTPUT_DIR_RELEASE):
	mkdir -p $@

release_common_dir_output: $(OUTPUT_DIR_DEBUG)
	mkdir -p $(OUTPUT_DIR_RELEASE)/common

debug_common_dir_output: $(OUTPUT_DIR_DEBUG)
	mkdir -p $(OUTPUT_DIR_DEBUG)/common

debug_client_dir_output: $(OUTPUT_DIR_DEBUG)
	mkdir -p $(OUTPUT_DIR_DEBUG)/client

release_client_dir_output: $(OUTPUT_DIR_DEBUG)
	mkdir -p $(OUTPUT_DIR_RELEASE)/client

debug_daemon_dir_output: $(OUTPUT_DIR_DEBUG)
	mkdir -p $(OUTPUT_DIR_DEBUG)/daemon

release_daemon_dir_output: $(OUTPUT_DIR_DEBUG)
	mkdir -p $(OUTPUT_DIR_RELEASE)/daemon

# END Common commands

# Targets por defecto
.PHONY: all client daemon debug clean install uninstall help