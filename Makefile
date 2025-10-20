include Config.mk

CLIENT_MK := Client.mk
DAEMON_MK := Daemon.mk

all: client daemon

client:
	@echo "\nBuilding client"
	$(MAKE) -f $(CLIENT_MK) release

daemon:
	@echo "\nBuilding daemon"
	$(MAKE) -f $(DAEMON_MK) release

debug:
	@echo "\nBuilding daemon (debug)"
	$(MAKE) -f $(DAEMON_MK) debug
	@echo "\nBuilding client (debug)"
	$(MAKE) -f $(CLIENT_MK) debug

install:
	@echo "\nInstalling Client"
	$(MAKE) -f $(CLIENT_MK) install
	@echo "\nInstalling Daemon"
	$(MAKE) -f $(DAEMON_MK) install

uninstall:
	@echo "\nUninstalling Client"
	$(MAKE) -f $(CLIENT_MK) uninstall
	@echo "\nUninstalling Daemon"
	$(MAKE) -f $(DAEMON_MK) uninstall

.PHONY: clean
clean:
	@echo "\nCleaning Client and Daemon"
	$(MAKE) -f $(CLIENT_MK) clean
	$(MAKE) -f $(DAEMON_MK) clean

.PHONY: dist
dist:
	tar czf $(CLIENT_NAME)-$(DAEMON_MAJOR_VERSION).$(DAEMON_MINOR_VERSION).$(DAEMON_PATCH_VERSION).tar.gz	\
	src include Makefile Config.mk Client.mk Daemon.mk debian data

.PHONY: help
help:
	@echo "Usage:"
	@echo "  make [target]"
	@echo
	@echo "Targets:"
	@echo "  all         Build client + daemon (release)"
	@echo "  client      Build only client (release)"
	@echo "  daemon      Build only daemon (release)"
	@echo "  debug       Build both in debug mode"
	@echo "  install     Install both"
	@echo "  uninstall   Uninstall both"
	@echo "  clean       Remove all generated files"
	@echo "  help        Show this message"