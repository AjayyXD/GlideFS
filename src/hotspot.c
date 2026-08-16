#include "common.h"
#include "hotspot.h"
#include <sys/stat.h>
#include <unistd.h>

static int file_executable(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (!S_ISREG(st.st_mode)) return 0;
    return access(path, X_OK) == 0;
}

int hotspot_locate(char *out_path, size_t out_len) {
    char self[512];
    ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n > 0) {
        self[n] = '\0';
        char *slash = strrchr(self, '/');
        if (slash) {
            *slash = '\0';
            char candidate[600];
            snprintf(candidate, sizeof(candidate), "%s/hotspotctl", self);
            if (file_executable(candidate)) {
                snprintf(out_path, out_len, "%s", candidate);
                return 1;
            }
        }
    }

    const char *common_paths[] = {
        "/usr/local/bin/hotspotctl",
        "/usr/bin/hotspotctl",
        NULL
    };
    for (int i = 0; common_paths[i]; i++) {
        if (file_executable(common_paths[i])) {
            snprintf(out_path, out_len, "%s", common_paths[i]);
            return 1;
        }
    }

    /* fall back to PATH lookup via `command -v` */
    FILE *fp = popen("command -v hotspotctl 2>/dev/null", "r");
    if (fp) {
        char line[512];
        if (fgets(line, sizeof(line), fp)) {
            chomp(line);
            if (strlen(line) > 0) {
                snprintf(out_path, out_len, "%s", line);
                pclose(fp);
                return 1;
            }
        }
        pclose(fp);
    }

    return 0;
}

int hotspot_start(const char *ssid, const char *password, int debug_mode) {
    char bin[512];
    if (!hotspot_locate(bin, sizeof(bin))) {
        log_err("hotspotctl not found. Build it via 'make' in third_party/hotspotctl");
        log_err("or install it system-wide, then re-run.");
        return 1;
    }

    log_info("Starting local access point via hotspotctl...");
    int rc = run_cmd("%s start -a -s '%s' -p '%s' %s", bin, ssid, password,
                      debug_mode ? "-d" : "");
    if (rc != 0) {
        log_err("hotspotctl failed to start the access point (exit %d)", rc);
        return 1;
    }
    return 0;
}

int hotspot_stop(void) {
    char bin[512];
    if (!hotspot_locate(bin, sizeof(bin))) {
        log_err("hotspotctl not found; cannot stop access point automatically.");
        return 1;
    }
    log_info("Stopping local access point...");
    return run_cmd("%s stop", bin);
}

int hotspot_get_iface(char *iface, size_t len) {
    FILE *f = fopen("/run/hotspotctl/hotspotctl.state", "r");
    if (!f) return 1;
    char line[128];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char val[64];
        if (sscanf(line, "IFACE : %63s", val) == 1) {
            snprintf(iface, len, "%s", val);
            found = 1;
            break;
        }
    }
    fclose(f);
    return found ? 0 : 1;
}
