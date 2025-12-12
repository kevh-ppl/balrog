include Config.mk

LDLIBS := -lpthread $(GLIB_LIBS) $(LIBNOTIFY_LIBS) $(LIBACL_LIBS) $(LLIBUDEV)

CFLAGS	  ?= $(DIRHEADERS)
CFLAGS	  += $(COMPILER_CONFIG_CFLAGS)
CFLAGS	  += $(COMMON_INCLUDE_DIR)
CFLAGS	  += $(CONFIG_C_FLAGS)
CFLAGS	  += $(GLIB_CFLAGS)
CFLAGS	  += $(LIBNOTIFY_CFLAGS)

CFILES := $(wildcard src/daemon/*.c)
SOURCES := $(CFILES) $(CFILES_COMMON)
BASENAMES := $(notdir $(basename $(SOURCES)))

RELEASE_OBJECTS ?= \
	$(patsubst src/common/%.c, $(OUTPUT_DIR_RELEASE)/common/%.o, $(CFILES_COMMON))

RELEASE_OBJECTS += \
	$(patsubst src/daemon/%.c, $(OUTPUT_DIR_RELEASE)/daemon/%.o, $(CFILES))

DEBUG_OBJECTS ?= \
	$(patsubst src/common/%.c, $(OUTPUT_DIR_DEBUG)/common/%_$(DEBUG_SUFFIX).o, $(CFILES_COMMON))

DEBUG_OBJECTS += \
	$(patsubst src/daemon/%.c, $(OUTPUT_DIR_DEBUG)/daemon/%_$(DEBUG_SUFFIX).o, $(CFILES))


.PHONY: all
all: debug release

.PHONY: release
release: CFLAGS := -s $(CFLAGS) # strip binary
release: $(DAEMON_NAME) # build in release mode
release: install

.PHONY: debug
debug: DAEMON_NO_CLOSE_STDIO = 1
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

$(OUTPUT_DIR_RELEASE)/common/%.o: src/common/%.c | $(OUTPUT_DIR_RELEASE)/common
	$(build_object)

# Build debug objects
$(OUTPUT_DIR_DEBUG)/daemon/%_$(DEBUG_SUFFIX).o: src/daemon/%.c | $(OUTPUT_DIR_DEBUG)/daemon
	$(build_object)

$(OUTPUT_DIR_DEBUG)/common/%_$(DEBUG_SUFFIX).o: src/common/%.c | $(OUTPUT_DIR_DEBUG)/common
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
	@for src in $(CFILES) ; do  \
		base=$$(basename $$src .c) \
        echo "  [depend]  $$src" ; \
        $(CC) $(CFLAGS) -MT \
		".depend $(OUTPUT_DIR_RELEASE)/daemon/$$base.o $(OUTPUT_DIR_DEBUG)/daemon/$$base_$(DEBUG_SUFFIX).o" \
		-MM $$src >> .depend ; \
    done

	@for src in $(CFILES_COMMON) ; do  \
		base=$$(basename $$src .c) \
        echo "  [depend]  $$src" ; \
        $(CC) $(CFLAGS) -MT \
		".depend $(OUTPUT_DIR_RELEASE)/common/$$base.o $(OUTPUT_DIR_DEBUG)/common/$$base_$(DEBUG_SUFFIX).o" \
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