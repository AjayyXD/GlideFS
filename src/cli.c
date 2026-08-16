#include "common.h"
#include "cli.h"
#include "state.h"
#include "hotspot.h"
#include "share.h"
#include "dashboard.h"
#include "client.h"
#include "deps.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <getopt.h>
#include <limits.h>

void print_version(void) {
    printf("glidefsctl v%s\n", GLIDEFS_VERSION);
}

void print_help(const char *argv0) {
    printf("\nGlideFS - zero-configuration shared folder over a local hotspot\n\n");
    printf("Usage: sudo %s <command> [options]\n\n", argv0);
    printf("Host commands:\n");
    printf("  init                          Set up local GlideFS state\n");
    printf("  share <path> -n <name>        Share <path> under <name>, start hotspot + dashboard\n");
    printf("        [-s <ssid>] [-p <password>] [-d]\n");
    printf("  unshare                       Stop the active share, hotspot and dashboard\n");
    printf("  status                        Show current share status\n\n");
    printf("Client commands:\n");
    printf("  connect <name> -p <password>  Join the hotspot and mount the share\n");
    printf("        [-s <ssid>] [-m <mountpoint>]\n");
    printf("  disconnect <name> [-m <mountpoint>]   Unmount a previously connected share\n\n");
    printf("Other:\n");
    printf("  deps [--install] [--host|--client]   Check/install runtime dependencies\n");
    printf("  --help                        Show this help\n");
    printf("  --version                     Show version\n\n");
    printf("Examples:\n");
    printf("  sudo %s share ~/Projects -n team -s TeamNet -p sharedsecret\n", argv0);
    printf("  sudo %s connect team -s TeamNet -p sharedsecret\n", argv0);
    printf("  sudo %s unshare\n\n", argv0);
}

static void gen_random_password(char *out, size_t len) {
    static const char charset[] = "abcdefghjkmnpqrstuvwxyzABCDEFGHJKMNPQRSTUVWXYZ23456789";
    srand((unsigned int)(time(NULL) ^ getpid()));
    size_t i;
    for (i = 0; i + 1 < len; i++) {
        out[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    out[i] = '\0';
}

int cmd_init(int argc, char *argv[]) {
    (void)argc; (void)argv;
    char home[256];
    client_home_dir(home, sizeof(home));

    char dir[300];
    snprintf(dir, sizeof(dir), CLIENT_STATE_DIR_FMT, home);
    ensure_dir(dir);

    char idfile[340];
    snprintf(idfile, sizeof(idfile), "%s/id", dir);

    FILE *f = fopen(idfile, "r");
    if (f) {
        char id[64];
        if (fgets(id, sizeof(id), f)) {
            chomp(id);
            log_ok("GlideFS already initialized (node id: %s)", id);
        }
        fclose(f);
        return 0;
    }

    char hostname[128] = "node";
    gethostname(hostname, sizeof(hostname));
    srand((unsigned int)(time(NULL) ^ getpid()));
    char id[80];
    snprintf(id, sizeof(id), "%s-%04x", hostname, rand() % 0xFFFF);

    f = fopen(idfile, "w");
    if (f) {
        fprintf(f, "%s\n", id);
        fclose(f);
    }
    log_ok("GlideFS initialized. Node id: %s", id);
    log_info("You can now run 'glidefsctl share <path> -n <name>' to host a folder,");
    log_info("or 'glidefsctl connect <name> -p <password>' to join one.");
    return 0;
}

int cmd_share(int argc, char *argv[]) {
    require_root(argv[0]);

    if (argc < 3) {
        log_err("Usage: %s share <path> -n <name> [-s <ssid>] [-p <password>] [-d]", argv[0]);
        return 1;
    }

    const char *path_arg = argv[2];
    char name[128] = "";
    char ssid[64] = "";
    char password[64] = "";
    int debug_mode = 0;

    /* parse options starting after the positional <path> */
    optind = 3;
    int opt;
    while ((opt = getopt(argc, argv, "n:s:p:d")) != -1) {
        switch (opt) {
            case 'n': snprintf(name, sizeof(name), "%s", optarg); break;
            case 's': snprintf(ssid, sizeof(ssid), "%s", optarg); break;
            case 'p': snprintf(password, sizeof(password), "%s", optarg); break;
            case 'd': debug_mode = 1; break;
            default:
                log_err("Unknown option. See --help.");
                return 1;
        }
    }

    if (strlen(name) == 0) {
        log_err("Missing required -n <name>");
        return 1;
    }

    char real_path[PATH_MAX];
    if (!realpath(path_arg, real_path)) {
        log_err("Path '%s' does not exist", path_arg);
        return 1;
    }
    struct stat sbuf;
    if (stat(real_path, &sbuf) != 0 || !S_ISDIR(sbuf.st_mode)) {
        log_err("'%s' is not a directory", real_path);
        return 1;
    }

    GlideState existing;
    if (state_read(&existing) == 0 && strlen(existing.share_name) > 0) {
        log_err("A share ('%s') is already active. Run 'unshare' first.", existing.share_name);
        return 1;
    }

    if (strlen(ssid) == 0) {
        snprintf(ssid, sizeof(ssid), "GlideFS-%s", name);
    }
    int generated_password = 0;
    if (strlen(password) == 0) {
        gen_random_password(password, sizeof(password));
        generated_password = 1;
    }
    if (strlen(password) < 8) {
        log_err("Password must be at least 8 characters");
        return 1;
    }

    if (deps_check(DEP_ROLE_HOST, 0) > 0) {
        log_err("Missing host dependencies. Run 'glidefsctl deps' for details");
        log_err("(or 'sudo glidefsctl deps --install' to install them automatically).");
        return 1;
    }

    if (hotspot_start(ssid, password, debug_mode) != 0) {
        return 1;
    }

    char iface[32] = "";
    for (int i = 0; i < 5; i++) {
        if (hotspot_get_iface(iface, sizeof(iface)) == 0) break;
        usleep(300000);
    }
    if (strlen(iface) == 0) {
        log_err("Could not determine hotspot interface after starting it.");
        hotspot_stop();
        return 1;
    }

    if (share_write_conf(name, real_path) != 0) {
        log_err("Failed to write Samba configuration");
        hotspot_stop();
        return 1;
    }

    int smbd_pid = share_start_smbd();
    if (smbd_pid <= 0) {
        log_err("Failed to start smbd. Check %s/logs/smbd.log", GLIDEFS_RUN_DIR);
        hotspot_stop();
        return 1;
    }
    log_ok("Samba share '%s' active (pid %d)", name, smbd_pid);

    int dash_pid = dashboard_start();
    if (dash_pid <= 0) {
        log_err("Warning: dashboard failed to start (port %d busy?)", GLIDEFS_DASH_PORT);
        dash_pid = 0;
    } else {
        log_ok("Dashboard running at http://%s:%d", GLIDEFS_HOST_IP, GLIDEFS_DASH_PORT);
    }

    GlideState st;
    memset(&st, 0, sizeof(st));
    snprintf(st.share_name, sizeof(st.share_name), "%s", name);
    snprintf(st.share_path, sizeof(st.share_path), "%s", real_path);
    snprintf(st.ssid, sizeof(st.ssid), "%s", ssid);
    snprintf(st.password, sizeof(st.password), "%s", password);
    snprintf(st.iface, sizeof(st.iface), "%s", iface);
    st.smbd_pid = smbd_pid;
    st.dashboard_pid = dash_pid;
    st.glidefsd_pid = 0;
    state_write(&st);

    printf("\n");
    log_ok("GlideFS is live.");
    printf("    SSID       : %s\n", ssid);
    if (generated_password) {
        printf("    Password   : %s   (auto-generated, share this with collaborators)\n", password);
    } else {
        printf("    Password   : %s\n", password);
    }
    printf("    Share      : %s  ->  %s\n", name, real_path);
    printf("    Dashboard  : http://%s:%d\n", GLIDEFS_HOST_IP, GLIDEFS_DASH_PORT);
    printf("\nOn another device, run:\n");
    printf("    sudo glidefsctl connect %s -s %s -p %s\n\n", name, ssid, password);

    return 0;
}

int cmd_unshare(int argc, char *argv[]) {
    (void)argc;
    require_root(argv[0]);

    GlideState st;
    if (state_read(&st) != 0 || strlen(st.share_name) == 0) {
        log_err("No active GlideFS share found.");
        return 1;
    }

    if (st.smbd_pid > 0) {
        share_stop_smbd(st.smbd_pid);
        log_info("Stopped Samba share '%s'", st.share_name);
    }
    if (st.dashboard_pid > 0) {
        dashboard_stop(st.dashboard_pid);
        log_info("Stopped dashboard");
    }
    hotspot_stop();

    state_clear();
    run_cmd_silent("rm -f %s", GLIDEFS_SMB_CONF);
    log_ok("GlideFS share fully torn down.");
    return 0;
}

int cmd_status(int argc, char *argv[]) {
    (void)argc; (void)argv;
    GlideState st;
    if (state_read(&st) != 0 || strlen(st.share_name) == 0) {
        printf("No active GlideFS share.\n");
        return 0;
    }

    int smbd_alive = (st.smbd_pid > 0 && kill(st.smbd_pid, 0) == 0);
    int dash_alive = (st.dashboard_pid > 0 && kill(st.dashboard_pid, 0) == 0);

    printf("GlideFS share: %s\n", st.share_name);
    printf("  Path       : %s\n", st.share_path);
    printf("  SSID       : %s\n", st.ssid);
    printf("  Interface  : %s\n", st.iface);
    printf("  Samba      : %s (pid %d)\n", smbd_alive ? "running" : "stopped", st.smbd_pid);
    printf("  Dashboard  : %s -> http://%s:%d\n",
           dash_alive ? "running" : "stopped", GLIDEFS_HOST_IP, GLIDEFS_DASH_PORT);
    return 0;
}

int cmd_connect(int argc, char *argv[]) {
    require_root(argv[0]);

    if (argc < 3) {
        log_err("Usage: %s connect <name> -p <password> [-s <ssid>] [-m <mountpoint>]", argv[0]);
        return 1;
    }
    const char *name = argv[2];
    char ssid[64] = "";
    char password[64] = "";
    char mountpoint[512] = "";

    optind = 3;
    int opt;
    while ((opt = getopt(argc, argv, "s:p:m:")) != -1) {
        switch (opt) {
            case 's': snprintf(ssid, sizeof(ssid), "%s", optarg); break;
            case 'p': snprintf(password, sizeof(password), "%s", optarg); break;
            case 'm': snprintf(mountpoint, sizeof(mountpoint), "%s", optarg); break;
            default:
                log_err("Unknown option. See --help.");
                return 1;
        }
    }
    if (strlen(ssid) == 0) {
        snprintf(ssid, sizeof(ssid), "GlideFS-%s", name);
    }
    if (strlen(password) == 0) {
        log_err("Missing required -p <password>");
        return 1;
    }

    return client_connect(name, ssid, password, mountpoint);
}

int cmd_disconnect(int argc, char *argv[]) {
    require_root(argv[0]);

    if (argc < 3) {
        log_err("Usage: %s disconnect <name> [-m <mountpoint>]", argv[0]);
        return 1;
    }
    const char *name = argv[2];
    char mountpoint[512] = "";

    optind = 3;
    int opt;
    while ((opt = getopt(argc, argv, "m:")) != -1) {
        switch (opt) {
            case 'm': snprintf(mountpoint, sizeof(mountpoint), "%s", optarg); break;
            default:
                log_err("Unknown option. See --help.");
                return 1;
        }
    }

    return client_disconnect(name, mountpoint);
}

int cmd_deps(int argc, char *argv[]) {
    DepRole role = DEP_ROLE_BOTH;
    int do_install = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--install") == 0) do_install = 1;
        else if (strcmp(argv[i], "--host") == 0) role = DEP_ROLE_HOST;
        else if (strcmp(argv[i], "--client") == 0) role = DEP_ROLE_CLIENT;
    }

    if (do_install) {
        require_root(argv[0]);
        return deps_install(role);
    }

    int missing = deps_check(role, 1);
    return missing > 0 ? 1 : 0;
}
