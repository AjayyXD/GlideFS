#include "common.h"
#include "cli.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0 || strcmp(cmd, "help") == 0) {
        print_help(argv[0]);
        return 0;
    }
    if (strcmp(cmd, "--version") == 0 || strcmp(cmd, "-v") == 0) {
        print_version();
        return 0;
    }
    if (strcmp(cmd, "init") == 0) {
        return cmd_init(argc, argv);
    }
    if (strcmp(cmd, "share") == 0) {
        return cmd_share(argc, argv);
    }
    if (strcmp(cmd, "unshare") == 0) {
        return cmd_unshare(argc, argv);
    }
    if (strcmp(cmd, "status") == 0) {
        return cmd_status(argc, argv);
    }
    if (strcmp(cmd, "connect") == 0) {
        return cmd_connect(argc, argv);
    }
    if (strcmp(cmd, "disconnect") == 0) {
        return cmd_disconnect(argc, argv);
    }
    if (strcmp(cmd, "deps") == 0) {
        return cmd_deps(argc, argv);
    }

    log_err("Unknown command: %s", cmd);
    print_help(argv[0]);
    return 1;
}
