include Config.mk

LDLIBS := -lpthread $(GLIB_LIBS) $(LIBNOTIFY_LIBS) $(LIBACL_LIBS) $(LLIBUDEV)

DIRHEADERS := -Iinclude/daemon

CFLAGS	  := $(DIRHEADERS)
CFLAGS	  += $(COMPILER_CONFIG_CFLAGS)
CFLAGS	  += $(CONFIG_C_FLAGS)

CFILES = $(wildcard src/daemon/*.c)
SOURCES  = $(CFILES)
BASENAMES = $(notdir $(basename $(SOURCES)))

RELEASE_OBJECTS := $(patsubst src/daemon/%.c, $(OUTPUT_DIR_RELEASE)/daemon/%.o, $(SOURCES))
DEBUG_OBJECTS := $(patsubst src/daemon/%.c, $(OUTPUT_DIR_DEBUG)/daemon/%_$(DEBUG_SUFFIX).o, $(SOURCES))

.PHONY: all
all: debug release

.PHONY: release
release: release_daemon_dir_output
release: CFLAGS := -s $(CFLAGS) # strip binary
release: $(DAEMON_NAME) # build in release mode
release: install

.PHONY: debug
debug: DAEMON_NO_CLOSE_STDIO = 1
debug: debug_daemon_dir_output
debug: CFLAGS := -DDEBUG  -g  $(CFLAGS) # add debug flag and debug symbols
debug: $(DAEMON_NAME)_$(DEBUG_SUFFIX) # call build in debug mode

# release mode
$(DAEMON_NAME): .depend $(RELEASE_OBJECTS)
	$(call build_bin, $(RELEASE_OBJECTS))

# debug mode
$(DAEMON_NAME)_$(DEBUG_SUFFIX): .depend $(DEBUG_OBJECTS)
	$(call build_bin, $(DEBUG_OBJECTS))

# Build release objects
$(OUTPUT_DIR_RELEASE)/daemon/%.o: src/daemon/%.c | $(OUTPUT_DIR_RELEASE)/daemon
	$(build_object)

# Build debug objects
$(OUTPUT_DIR_DEBUG)/daemon/%_$(DEBUG_SUFFIX).o: src/daemon/%.c | $(OUTPUT_DIR_DEBUG)/daemon
	$(build_object)

.PHONY: install
install:
# #DESTDIR viene del empaquetado DEB
	install -D $(DAEMON_NAME) $(DESTDIR)/usr/bin/$(DAEMON_NAME)
	install -D debian/balrog.service $(DESTDIR)/lib/systemd/user/balrog.service
	install -D data/mono_autortizo_54px.jpg $(DESTDIR)/usr/share/icons/hicolor/48x48/apps/mono_autorizo_54px.png	
	@echo "Installed $(DAEMON_NAME) to /usr/bin/$(DAEMON_NAME)"

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)/lib/systemd/user/balrog.service
	rm -f $(DESTDIR)/usr/share/icons/hicolor/48x48/apps/mono_autorizo_54px.png
	rm -f $(DESTDIR)/usr/bin/$(DAEMON_NAME)
	@echo "Uninstalled $(DAEMON_NAME) from /usr/bin/$(DAEMON_NAME)"

.PHONY: clean
clean:
	-@rm -f $(DAEMON_NAME)
	-@rm -f $(DAEMON_NAME)_$(DEBUG_SUFFIX)
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
		".depend $(OUTPUT_DIR_RELEASE)/daemon/$$base.o $(OUTPUT_DIR_DEBUG)/daemon/$$base_$(DEBUG_SUFFIX).o" \
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