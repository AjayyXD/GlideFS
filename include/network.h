#ifndef GLIDEFS_NETWORK_H
#define GLIDEFS_NETWORK_H

#include <stdbool.h>

#define TS_IP_MAXLEN 64

typedef enum {
    GFS_LINK_LAN,
    GFS_LINK_TAILSCALE,
    GFS_LINK_DOWN
} gfs_link_t;

/* ---- Tailscale lifecycle -------------------------------------------- */

/* Starts tailscaled.service (systemd) and authenticates non-interactively
 * with a reusable Tailscale Auth Key (no browser prompt). hostname may be
 * NULL to accept Tailscale's default. Returns 0 on success. */
int gfs_tailscale_up(const char *authkey, const char *hostname);

/* Tears the node down (does not stop tailscaled.service). */
int gfs_tailscale_down(void);

/* Parses `tailscale status --json` and writes this host's 100.x.y.z
 * address into out_ip. Returns 0 on success, -1 if not yet assigned
 * or on parse/exec failure. */
int gfs_tailscale_self_ip(char out_ip[TS_IP_MAXLEN]);

/* ---- Samba interface binding ----------------------------------------- */

/* Rewrites smb_conf_path's [global] section to bind exclusively to
 * tailscale0 and restarts smbd. smb_conf_path is the same file
 * share.c already generates (e.g. /run/glidefs/smb.conf). */
int gfs_samba_bind_tailscale(const char *smb_conf_path);

/* Rebinds smbd back onto a LAN interface (e.g. "wlan0"), used on
 * initial hotspot share and on fail-back. */
int gfs_samba_bind_lan(const char *smb_conf_path, const char *lan_iface);

/* ---- Mid-session failover monitor ------------------------------------ */

typedef void (*gfs_link_cb)(void *ctx);

/* Starts a background thread that pings lan_ip every interval_sec
 * seconds. After fail_threshold consecutive failures it calls
 * on_failover(ctx) once; when the LAN answers again it calls
 * on_recover(ctx) once. Non-blocking; returns 0 if the thread started. */
int gfs_link_monitor_start(const char *lan_ip, int interval_sec,
                            int fail_threshold,
                            gfs_link_cb on_failover,
                            gfs_link_cb on_recover,
                            void *ctx);

/* Signals the monitor thread to stop on its next poll. Does not block. */
void gfs_link_monitor_stop(void);

#endif /* GLIDEFS_NETWORK_H */