include Config.mk

LDLIBS := $(LIBNOTIFY_LIBS)
LDLIBS += $(GLIB_LIBS)

CFLAGS = $(DIRHEADERS)
CFLAGS += $(COMPILER_CONFIG_CFLAGS)
CFLAGS += $(CONFIG_C_FLAGS)
CFLAGS += $(LIBNOTIFY_CFLAGS)
CFLAGS += $(GLIB_CFLAGS)

CFILES := $(wildcard src/client/*.c)
CFILES_COMMON := $(wildcard src/common/*.c)
SOURCES := $(CFILES) $(CFILES_COMMON)
BASENAMES := $(notdir $(basename $(SOURCES)))

RELEASE_OBJECTS := \
	$(patsubst src/client/%.c, $(OUTPUT_DIR_RELEASE)/client/%.o, $(CFILES)) \
	$(patsubst src/common/%.c, $(OUTPUT_DIR_RELEASE)/common/%.o, $(CFILES_COMMON))

DEBUG_OBJECTS := \
	$(patsubst src/client/%.c, $(OUTPUT_DIR_DEBUG)/client/%_$(DEBUG_SUFFIX).o, $(CFILES)) \
	$(patsubst src/common/%.c, $(OUTPUT_DIR_DEBUG)/common/%_$(DEBUG_SUFFIX).o, $(CFILES_COMMON))

.PHONY: all
all: debug release

.PHONY: release
release: release_client_dir_output release_common_dir_output
release: CFLAGS := -s $(CFLAGS) # strip binary
release: $(CLIENT_NAME) # build in release mode
release: install

.PHONY: debug
debug: DAEMON_NO_CLOSE_STDIO = 1
debug: debug_client_dir_output debug_common_dir_output
debug: CFLAGS := -DDEBUG  -g  $(CFLAGS) # add debug flag and debug symbols
debug: $(CLIENT_NAME)_$(DEBUG_SUFFIX) # call build in debug mode

# release mode
$(CLIENT_NAME): .depend $(RELEASE_OBJECTS)
	$(call build_bin, $(RELEASE_OBJECTS))

# debug mode
$(CLIENT_NAME)_$(DEBUG_SUFFIX): .depend $(DEBUG_OBJECTS)
	$(call build_bin, $(DEBUG_OBJECTS))

# Build release objects
$(OUTPUT_DIR_RELEASE)/client/%.o: src/client/%.c | $(OUTPUT_DIR_RELEASE)/client
	$(build_object)

$(OUTPUT_DIR_RELEASE)/common/%.o: src/common/%.c | $(OUTPUT_DIR_RELEASE)/common
	$(build_object)

# Build debug objects
$(OUTPUT_DIR_DEBUG)/client/%_$(DEBUG_SUFFIX).o: src/client/%.c | $(OUTPUT_DIR_DEBUG)/client
	$(build_object)

$(OUTPUT_DIR_DEBUG)/common/%_$(DEBUG_SUFFIX).o: src/common/%.c | $(OUTPUT_DIR_DEBUG)/common
	$(build_object)

.PHONY: install
install:
# #DESTDIR viene del empaquetado DEB
	install -D $(CLIENT_NAME) $(DESTDIR)/usr/bin/$(CLIENT_NAME)
	@echo "Installed $(CLIENT_NAME) to /usr/bin/$(CLIENT_NAME)"

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)/usr/bin/$(CLIENT_NAME)
	@echo "Uninstalled $(CLIENT_NAME) from /usr/bin/$(CLIENT_NAME)"

.PHONY: clean
clean:
	-@rm -f $(CLIENT_NAME)
	-@rm -f $(CLIENT_NAME)_$(DEBUG_SUFFIX)
	-@rm -fr $(OUTPUT_DIR_RELEASE)
	-@rm -fr $(OUTPUT_DIR_DEBUG)
	-@rm -f .depend
	-@rm -f *.*~

.depend:
	-@rm -f .depend
	@echo "Generating dependencies..."
	@for src in $(SOURCES) ; do  \
		base=$$(basename $$src .c) \
        echo "  [depend]  $$src" ; \
        $(CC) $(CFLAGS) -MT \
		".depend $(OUTPUT_DIR_RELEASE)$$base.o $(OUTPUT_DIR_DEBUG)$$base_$(DEBUG_SUFFIX).o" \
		-MM $$src >> .depend ; \
    done

.PHONY: help
help:
	@echo "make [command]"
	@echo "command is:"
	@echo "   all     -  build daemon in release and debug mode"
	@echo "   debug   -  build in debug mode (#define DEBUG 1)"
	@echo "   release -  build in release mode (strip)"
	@echo "   clean   -  remove all generated files"
	@echo "   help    -  this help"