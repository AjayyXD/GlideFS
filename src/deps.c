#include "common.h"
#include "deps.h"

typedef struct {
    const char *bin;      /* binary we actually need on $PATH */
    const char *apt;
    const char *pacman;
    const char *dnf;
    const char *zypper;
    const char *apk;
    DepRole role;
    const char *used_for;
} Dep;

static const Dep DEPS[] = {
    { "hostapd", "hostapd",           "hostapd",         "hostapd",         "hostapd",         "hostapd",         DEP_ROLE_HOST,   "broadcasting the access point" },
    { "dnsmasq", "dnsmasq",           "dnsmasq",         "dnsmasq",         "dnsmasq",         "dnsmasq",         DEP_ROLE_HOST,   "DHCP for connected devices" },
    { "nft",     "nftables",         "nftables",        "nftables",        "nftables",        "nftables",        DEP_ROLE_HOST,   "NAT / firewall rules" },
    { "ip",      "iproute2",         "iproute2",        "iproute",         "iproute2",        "iproute2",        DEP_ROLE_HOST,   "interface & routing setup" },
    { "iw",      "iw",               "iw",              "iw",              "iw",              "iw",              DEP_ROLE_HOST,   "WiFi band/region detection" },
    { "rfkill",  "rfkill",           "rfkill",          "rfkill",          "rfkill",          "rfkill",          DEP_ROLE_HOST,   "unblocking the WiFi radio" },
    { "smbd",    "samba",            "samba",           "samba",           "samba",           "samba",           DEP_ROLE_HOST,   "serving the shared folder" },
    { "mount.cifs", "cifs-utils",   "cifs-utils",      "cifs-utils",      "cifs-utils",      "cifs-utils",      DEP_ROLE_CLIENT,"mounting the remote share" },
    { "nmcli",   "network-manager",  "networkmanager",  "NetworkManager",  "NetworkManager",  "networkmanager",  DEP_ROLE_CLIENT,"joining the hotspot automatically" },
    { "ping",    "iputils-ping",     "iputils",         "iputils",         "iputils",         "iputils",         DEP_ROLE_CLIENT,"checking the host is reachable" },
};
static const int DEPS_COUNT = sizeof(DEPS) / sizeof(DEPS[0]);

typedef enum { PM_NONE, PM_APT, PM_PACMAN, PM_DNF, PM_ZYPPER, PM_APK } PkgMgr;

static PkgMgr detect_pkg_mgr(void) {
    if (system("command -v apt-get > /dev/null 2>&1") == 0) return PM_APT;
    if (system("command -v pacman > /dev/null 2>&1") == 0) return PM_PACMAN;
    if (system("command -v dnf > /dev/null 2>&1") == 0) return PM_DNF;
    if (system("command -v zypper > /dev/null 2>&1") == 0) return PM_ZYPPER;
    if (system("command -v apk > /dev/null 2>&1") == 0) return PM_APK;
    return PM_NONE;
}

static const char *pkg_mgr_name(PkgMgr pm) {
    switch (pm) {
        case PM_APT: return "apt";
        case PM_PACMAN: return "pacman";
        case PM_DNF: return "dnf";
        case PM_ZYPPER: return "zypper";
        case PM_APK: return "apk";
        default: return "unknown";
    }
}

static const char *pkg_name_for(const Dep *d, PkgMgr pm) {
    switch (pm) {
        case PM_APT: return d->apt;
        case PM_PACMAN: return d->pacman;
        case PM_DNF: return d->dnf;
        case PM_ZYPPER: return d->zypper;
        case PM_APK: return d->apk;
        default: return d->apt; /* best guess */
    }
}

static int role_matches(DepRole want, DepRole have) {
    if (want == DEP_ROLE_BOTH) return 1;
    return want == have;
}

static int have_binary(const char *bin) {
    char probe[160];
    snprintf(probe, sizeof(probe), "command -v %s > /dev/null 2>&1", bin);
    return system(probe) == 0;
}

int deps_check(DepRole role, int verbose) {
    int missing = 0;
    PkgMgr pm = detect_pkg_mgr();
    const char *missing_pkgs[32];
    int mp_count = 0;

    if (verbose) {
        printf("Checking GlideFS runtime dependencies (%s)...\n",
               role == DEP_ROLE_HOST ? "host" : role == DEP_ROLE_CLIENT ? "client" : "host + client");
    }

    for (int i = 0; i < DEPS_COUNT; i++) {
        const Dep *d = &DEPS[i];
        if (!role_matches(role, d->role)) continue;
        int present = have_binary(d->bin);
        if (verbose) {
            printf("  [%s] %-12s %s\n", present ? "ok" : "!!", d->bin, d->used_for);
        }
        if (!present) {
            missing++;
            if (mp_count < 32) missing_pkgs[mp_count++] = pkg_name_for(d, pm);
        }
    }

    if (missing > 0 && verbose) {
        printf("\n%d dependenc%s missing.\n", missing, missing == 1 ? "y" : "ies");
        if (pm == PM_NONE) {
            printf("Could not detect your package manager. Install these manually:\n  ");
            for (int i = 0; i < mp_count; i++) printf("%s ", missing_pkgs[i]);
            printf("\n");
        } else {
            char cmd[512] = "";
            switch (pm) {
                case PM_APT: snprintf(cmd, sizeof(cmd), "sudo apt-get install -y"); break;
                case PM_PACMAN: snprintf(cmd, sizeof(cmd), "sudo pacman -S --noconfirm"); break;
                case PM_DNF: snprintf(cmd, sizeof(cmd), "sudo dnf install -y"); break;
                case PM_ZYPPER: snprintf(cmd, sizeof(cmd), "sudo zypper install -y"); break;
                case PM_APK: snprintf(cmd, sizeof(cmd), "sudo apk add"); break;
                default: break;
            }
            printf("Detected package manager: %s\nRun:\n  %s", pkg_mgr_name(pm), cmd);
            for (int i = 0; i < mp_count; i++) printf(" %s", missing_pkgs[i]);
            printf("\nOr let GlideFS do it: sudo glidefsctl deps --install\n");
        }
    } else if (verbose) {
        printf("\nAll dependencies present.\n");
    }

    return missing;
}

int deps_install(DepRole role) {
    if (geteuid() != 0) {
        log_err("Installing dependencies requires root. Re-run with sudo.");
        return 1;
    }

    PkgMgr pm = detect_pkg_mgr();
    if (pm == PM_NONE) {
        log_err("Could not detect a supported package manager (apt/pacman/dnf/zypper/apk).");
        log_err("Please install the missing packages manually (see 'glidefsctl deps').");
        return 1;
    }

    const char *missing_pkgs[32];
    int mp_count = 0;
    int missing = 0;

    for (int i = 0; i < DEPS_COUNT; i++) {
        const Dep *d = &DEPS[i];
        if (!role_matches(role, d->role)) continue;
        if (!have_binary(d->bin)) {
            missing++;
            const char *pkg = pkg_name_for(d, pm);
            /* de-dup (samba covers smbd/smbstatus, etc.) */
            int dup = 0;
            for (int j = 0; j < mp_count; j++) {
                if (strcmp(missing_pkgs[j], pkg) == 0) { dup = 1; break; }
            }
            if (!dup && mp_count < 32) missing_pkgs[mp_count++] = pkg;
        }
    }

    if (missing == 0) {
        log_ok("All dependencies already present.");
        return 0;
    }

    char pkglist[512] = "";
    for (int i = 0; i < mp_count; i++) {
        strncat(pkglist, missing_pkgs[i], sizeof(pkglist) - strlen(pkglist) - 2);
        strncat(pkglist, " ", sizeof(pkglist) - strlen(pkglist) - 1);
    }

    char cmd[700];
    switch (pm) {
        case PM_APT:
            snprintf(cmd, sizeof(cmd), "apt-get update && apt-get install -y %s", pkglist);
            break;
        case PM_PACMAN:
            snprintf(cmd, sizeof(cmd), "pacman -Sy --noconfirm %s", pkglist);
            break;
        case PM_DNF:
            snprintf(cmd, sizeof(cmd), "dnf install -y %s", pkglist);
            break;
        case PM_ZYPPER:
            snprintf(cmd, sizeof(cmd), "zypper --non-interactive install %s", pkglist);
            break;
        case PM_APK:
            snprintf(cmd, sizeof(cmd), "apk add %s", pkglist);
            break;
        default:
            return 1;
    }

    log_info("Installing via %s: %s", pkg_mgr_name(pm), pkglist);
    int rc = system(cmd);
    if (rc != 0) {
        log_err("Package installation failed (exit %d).", rc);
        return 1;
    }
    log_ok("Dependencies installed.");
    return 0;
}
