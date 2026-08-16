#include "common.h"
#include "client.h"
#include <pwd.h>
#include <sys/stat.h>

void client_home_dir(char *out, size_t len) {
    const char *sudo_user = getenv("SUDO_USER");
    struct passwd *pw = NULL;
    if (sudo_user && strlen(sudo_user) > 0) {
        pw = getpwnam(sudo_user);
    }
    if (pw) {
        snprintf(out, len, "%s", pw->pw_dir);
        return;
    }
    const char *home = getenv("HOME");
    snprintf(out, len, "%s", home ? home : "/root");
}

static void invoking_ids(uid_t *uid, gid_t *gid) {
    const char *sudo_user = getenv("SUDO_USER");
    struct passwd *pw = NULL;
    if (sudo_user && strlen(sudo_user) > 0) {
        pw = getpwnam(sudo_user);
    }
    if (pw) {
        *uid = pw->pw_uid;
        *gid = pw->pw_gid;
    } else {
        *uid = getuid();
        *gid = getgid();
    }
}

static int have_cmd(const char *cmd) {
    char probe[128];
    snprintf(probe, sizeof(probe), "command -v %s > /dev/null 2>&1", cmd);
    return system(probe) == 0;
}

static void record_mount(const char *name, const char *mountpoint) {
    char home[256], statedir[300], statefile[340];
    client_home_dir(home, sizeof(home));
    snprintf(statedir, sizeof(statedir), CLIENT_STATE_DIR_FMT, home);
    ensure_dir(statedir);
    snprintf(statefile, sizeof(statefile), CLIENT_STATE_FILE_FMT, home);

    /* rewrite without any prior entry for this name, then append */
    char tmpfile[360];
    snprintf(tmpfile, sizeof(tmpfile), "%s.tmp", statefile);
    FILE *out = fopen(tmpfile, "w");
    FILE *in = fopen(statefile, "r");
    if (in) {
        char line[600];
        while (fgets(line, sizeof(line), in)) {
            char ename[128];
            if (sscanf(line, "%127[^|]|", ename) == 1 && strcmp(ename, name) == 0) {
                continue; /* drop stale entry */
            }
            if (out) fputs(line, out);
        }
        fclose(in);
    }
    if (out) {
        fprintf(out, "%s|%s\n", name, mountpoint);
        fclose(out);
        rename(tmpfile, statefile);
    }
}

static int lookup_mount(const char *name, char *mountpoint, size_t len) {
    char home[256], statefile[340];
    client_home_dir(home, sizeof(home));
    snprintf(statefile, sizeof(statefile), CLIENT_STATE_FILE_FMT, home);

    FILE *f = fopen(statefile, "r");
    if (!f) return 1;
    char line[600];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char ename[128], epath[512];
        if (sscanf(line, "%127[^|]|%511[^\n]", ename, epath) == 2) {
            if (strcmp(ename, name) == 0) {
                snprintf(mountpoint, len, "%s", epath);
                found = 1;
                break;
            }
        }
    }
    fclose(f);
    return found ? 0 : 1;
}

static void forget_mount(const char *name) {
    char home[256], statefile[340], tmpfile[360];
    client_home_dir(home, sizeof(home));
    snprintf(statefile, sizeof(statefile), CLIENT_STATE_FILE_FMT, home);
    snprintf(tmpfile, sizeof(tmpfile), "%s.tmp", statefile);

    FILE *in = fopen(statefile, "r");
    FILE *out = fopen(tmpfile, "w");
    if (in && out) {
        char line[600];
        while (fgets(line, sizeof(line), in)) {
            char ename[128];
            if (sscanf(line, "%127[^|]|", ename) == 1 && strcmp(ename, name) == 0) continue;
            fputs(line, out);
        }
    }
    if (in) fclose(in);
    if (out) { fclose(out); rename(tmpfile, statefile); }
}

int client_connect(const char *name, const char *ssid, const char *password,
                    const char *mountpoint) {
    if (!have_cmd("nmcli")) {
        log_err("nmcli (NetworkManager) not found. GlideFS clients currently");
        log_err("require NetworkManager to join the hotspot automatically.");
        log_err("Connect to SSID '%s' manually, then retry with --skip-wifi.", ssid);
        return 1;
    }

    log_info("Joining hotspot '%s'...", ssid);
    int rc = run_cmd("nmcli device wifi connect '%s' password '%s' 2>&1", ssid, password);
    if (rc != 0) {
        log_err("Failed to join WiFi network '%s'. Check the SSID/password.", ssid);
        return 1;
    }

    log_info("Waiting for host %s to respond...", GLIDEFS_HOST_IP);
    int reachable = 0;
    for (int i = 0; i < 10; i++) {
        if (run_cmd_silent("ping -c1 -W1 %s", GLIDEFS_HOST_IP) == 0) {
            reachable = 1;
            break;
        }
        sleep(1);
    }
    if (!reachable) {
        log_err("Could not reach GlideFS host at %s", GLIDEFS_HOST_IP);
        return 1;
    }

    char home[256];
    char mp[512];
    client_home_dir(home, sizeof(home));
    if (mountpoint && strlen(mountpoint) > 0) {
        snprintf(mp, sizeof(mp), "%s", mountpoint);
    } else {
        snprintf(mp, sizeof(mp), "%s/GlideFS/%s", home, name);
    }
    ensure_dir(mp);

    uid_t uid; gid_t gid;
    invoking_ids(&uid, &gid);

    log_info("Mounting //%s/%s at %s...", GLIDEFS_HOST_IP, name, mp);
    rc = run_cmd("mount -t cifs //%s/%s '%s' -o guest,vers=3.0,uid=%d,gid=%d,iocharset=utf8,file_mode=0664,dir_mode=0775",
                 GLIDEFS_HOST_IP, name, mp, uid, gid);
    if (rc != 0) {
        log_err("Mount failed. Is cifs-utils installed (mount.cifs)?");
        return 1;
    }

    record_mount(name, mp);
    log_ok("Connected. '%s' is now available at %s", name, mp);
    return 0;
}

int client_disconnect(const char *name, const char *mountpoint) {
    char mp[512];
    if (mountpoint && strlen(mountpoint) > 0) {
        snprintf(mp, sizeof(mp), "%s", mountpoint);
    } else if (lookup_mount(name, mp, sizeof(mp)) != 0) {
        log_err("No recorded mount for '%s'. Pass -m <mountpoint> explicitly.", name);
        return 1;
    }

    log_info("Unmounting %s...", mp);
    int rc = run_cmd("umount '%s'", mp);
    if (rc != 0) {
        log_err("Unmount failed (is it busy?). Try: umount -l '%s'", mp);
        return 1;
    }
    forget_mount(name);
    log_ok("Disconnected '%s'", name);
    return 0;
}
