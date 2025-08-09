LLIBUDEV := -ludev
GLIB_CFLAGS := $(shell pkg-config --cflags glib-2.0)
GLIB_LIBS := $(shell pkg-config --libs glib-2.0)
LIBNOTIFY_LIBS := $(shell pkg-config --libs libnotify)
LIBNOTIFY_CFLAGS := $(shell pkg-config --cflags libnotify)
LIBACL_LIBS := $(shell pkg-config --libs libacl)
# GCCFLAGS := $(LLIBUDEV) -Wall -Wextra -Werror -pedantic -std=c99
# CC := gcc

# all: udev.c
# 	$(CC) udev.c -o balrog $(GCCFLAGS)

# clean:
# 	rm -f balrog

DAEMON_NAME           = balrog
DAEMON_MAJOR_VERSION  = 0
DAEMON_MINOR_VERSION  = 0
DAEMON_PATCH_VERSION  = *hash
#variants:  hash - set PATCH_VERSION == commit hash (12 digits)
#variants: *hash - set PATCH_VERSION == * + commit hash (12 digits),
#                  * will be set if files in repo are changed
#variants:  xxx  - set PATCH_VERSION == xxx (your variant)

DAEMON_PID_FILE_NAME  = /var/run/$(DAEMON_NAME)/$(DAEMON_NAME).pid
DAEMON_LOG_FILE_NAME  = /var/log/$(DAEMON_NAME)/$(DAEMON_NAME).log
DAEMON_CMD_PIPE_NAME  = /var/run/$(DAEMON_NAME)/$(DAEMON_NAME).cmd
DDAEMON_MONITOR_LOG_FILE_NAME = /var/log/$(DAEMON_NAME)/monitor.log
DDAEMON_MONITOR_PID_FILE_NAME = /var/run/$(DAEMON_NAME)/monitor.pid
DAEMON_NO_CHDIR       = 1
DAEMON_NO_FORK        = 0
DAEMON_NO_CLOSE_STDIO = 1


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

CFLAGS     = -DDAEMON_NAME='"$(DAEMON_NAME)"'
CFLAGS    += -DDAEMON_MAJOR_VERSION=$(DAEMON_MAJOR_VERSION)
CFLAGS    += -DDAEMON_MINOR_VERSION=$(DAEMON_MINOR_VERSION)
CFLAGS    += -DDAEMON_PATCH_VERSION=$(DAEMON_PATCH_VERSION)
CFLAGS    += -DDAEMON_PID_FILE_NAME='"$(DAEMON_PID_FILE_NAME)"'
CFLAGS    += -DDAEMON_LOG_FILE_NAME='"$(DAEMON_LOG_FILE_NAME)"'
CFLAGS	  += -DDAEMON_MONITOR_LOG_FILE_NAME='"$(DDAEMON_MONITOR_LOG_FILE_NAME)"'
CFLAGS	  += -DDAEMON_MONITOR_PID_FILE_NAME='"$(DDAEMON_MONITOR_PID_FILE_NAME)"'
CFLAGS    += -DDAEMON_CMD_PIPE_NAME='"$(DAEMON_CMD_PIPE_NAME)"'
CFLAGS    += -DDAEMON_NO_CHDIR=$(DAEMON_NO_CHDIR)
CFLAGS    += -DDAEMON_NO_FORK=$(DAEMON_NO_FORK)
CFLAGS    += -DDAEMON_NO_CLOSE_STDIO=$(DAEMON_NO_CLOSE_STDIO)
CFLAGS    += -DCOMMIT_HASH='"$(COMMIT_HASH)"'
CFLAGS    += -DCHANGED_FILES=$(CHANGED_FILES)

CFLAGS    += -I$(COMMON_DIR)
CFLAGS    += -O2  -Wall  -pipe -Wextra -pedantic -std=c99
CFLAGS    += -lpthread $(GLIB_LIBS) $(LIBNOTIFY_LIBS) $(LIBACL_LIBS)
CFLAGS	  += $(LLIBUDEV)

CC        ?=  gcc

CFILES = $(wildcard *.c)
SOURCES  = $(CFILES)

OBJECTS  := $(patsubst %.c,  %.o, $(SOURCES) )

# patron suffix for debug objects
DEBUG_SUFFIX   = debug
DEBUG_OBJECTS := $(patsubst %.o, %_$(DEBUG_SUFFIX).o, $(OBJECTS) )


.PHONY: all
all: debug release

.PHONY: release
release: CFLAGS := -s  $(CFLAGS) # strip binary
release: $(DAEMON_NAME) # build in release mode

.PHONY: debug
debug: DAEMON_NO_CLOSE_STDIO = 1
debug: CFLAGS := -DDEBUG  -g  $(CFLAGS) # add debug flag and debug symbols
debug: $(DAEMON_NAME)_$(DEBUG_SUFFIX) # call build in debug mode

# release mode
$(DAEMON_NAME): .depend $(OBJECTS)
	$(call build_bin, $(OBJECTS))

# debug mode
$(DAEMON_NAME)_$(DEBUG_SUFFIX): .depend $(DEBUG_OBJECTS)
	$(call build_bin, $(DEBUG_OBJECTS))

# Build release objects
%.o: %.c
	$(build_object)


# Build debug objects
%_$(DEBUG_SUFFIX).o: %.c
	$(build_object)

.PHONY: install
install:
	install -D $(DAEMON_NAME) /usr/local/bin/$(DAEMON_NAME)
	@echo "Installed $(DAEMON_NAME) to /usr/local/bin/$(DAEMON_NAME)"

.PHONY: uninstall
uninstall:
	rm -f /usr/local/bin/$(DAEMON_NAME)
	@echo "Uninstalled $(DAEMON_NAME) from /usr/local/bin/$(DAEMON_NAME)"

.PHONY: clean
clean:
	-@rm -f $(DAEMON_NAME)
	-@rm -f $(DAEMON_NAME)_$(DEBUG_SUFFIX)
	-@rm -f $(OBJECTS)
	-@rm -f $(DEBUG_OBJECTS)
	-@rm -f .depend
	-@rm -f *.*~

.depend:
	-@rm -f .depend
	@echo "Generating dependencies..."
	@for src in $(SOURCES) ; do  \
        echo "  [depend]  $$src" ; \
        $(CC) $(GLIB_CFLAGS) $(LIBNOTIFY_CFLAGS) $(CFLAGS) -MT ".depend $${src%.*}.o $${src%.*}_$(DEBUG_SUFFIX).o" -MM $$src >> .depend ; \
    done


ifeq "$(findstring $(MAKECMDGOALS),clean distclean)"  ""
    include $(wildcard .depend)
endif

# Common commands
BUILD_ECHO = echo "\n  [build]  $@:"

define build_object
    @$(BUILD_ECHO)
    $(CC) -c $< -o $@ $(GLIB_CFLAGS) $(LIBNOTIFY_CFLAGS) $(CFLAGS)
endef

define build_bin
    @$(BUILD_ECHO)
    $(CC)  $1 -o $@ $(GLIB_CFLAGS) $(LIBNOTIFY_CFLAGS) $(CFLAGS)
    @echo "\n---- Compiled $@ ver $(DAEMON_MAJOR_VERSION).$(DAEMON_MINOR_VERSION).$(DAEMON_PATCH_VERSION) ----\n"
endef

.PHONY: help
help:
	@echo "make [command]"
	@echo "command is:"
	@echo "   all     -  build daemon in release and debug mode"
	@echo "   debug   -  build in debug mode (#define DEBUG 1)"
	@echo "   release -  build in release mode (strip)"
	@echo "   clean   -  remove all generated files"
	@echo "   help    -  this help"