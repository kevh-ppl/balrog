CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

OUTPUT_HELPER := sand_help
OUTPUT_SETUP := sand_setup

.PHONY: all
all: $(OUTPUT_HELPER) $(OUTPUT_SETUP)

$(OUTPUT_HELPER): src/helpers/sandbox_root_helper.c
	$(CC) $(CFLAGS) -o $(OUTPUT_HELPER) src/helpers/sandbox_root_helper.c

$(OUTPUT_SETUP): src/helpers/sandbox_setup.c
	$(CC) $(CFLAGS) -o $(OUTPUT_SETUP) src/helpers/sandbox_setup.c

.PHONY: install
install: all
	install -D $(OUTPUT_HELPER) $(DESTDIR)/usr/local/bin/$(OUTPUT_HELPER)
	install -D $(OUTPUT_SETUP) $(DESTDIR)/usr/local/bin/$(OUTPUT_SETUP)
	@echo "\nAfter install: give the helper CAP_SYS_ADMIN or make it setuid root:\n"
	@echo "  sudo setcap cap_sys_admin+ep /usr/local/bin/$(OUTPUT_HELPER)"
	@echo "  # or (less recommended): sudo chown root:root /usr/local/bin/$(OUTPUT_HELPER) && sudo chmod 4755 /usr/local/bin/$(OUTPUT_HELPER)"

.PHONY: clean
clean:
	rm -f $(OUTPUT_HELPER) $(OUTPUT_SETUP)