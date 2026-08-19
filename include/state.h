#ifndef GLIDEFS_STATE_H
#define GLIDEFS_STATE_H

typedef enum{
    GFS_TRANSPORT_LAN,
    GFS_TRANSPORT_TAILSCALE,
} gfs_transport_t;

typedef struct {
    char share_name[128];
    char share_path[4096];
    char ssid[64];
    char password[64];
    char iface[32];
    int  smbd_pid;
    int  dashboard_pid;
    int  glidefsd_pid;
    gfs_transport_t transport;
} GlideState;

/* write host state to /run/glidefs/glidefs.state */
int state_write(const GlideState *st);

/* read host state; returns 0 on success, 1 if not found/parse error */
int state_read(GlideState *st);

/* remove state file */
void state_clear(void);

#endif
