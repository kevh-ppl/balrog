#include <libudev.h>
#include <sys/select.h>

#include "balrog_udev.h"
#include "daemon.h"

// Define the help string for balrog-usb-utility
static const char *help_str = DAEMON_NAME
    "\n"
    " Version: " DAEMON_VERSION_STR
    "\n\n"
#ifdef DEBUG
    " Build mode: debug\n"
#else
    " Build mode: release\n"
#endif
    " Build date: " __DATE__
    "\n"
    " Build time: " __TIME__
    "\n"
    " Options:                      description:\n\n"
    "  -v,  --version              Display version\n"
    "  -h,  --help                 Display this help\n\n";

int main() { do_enumerate(); }