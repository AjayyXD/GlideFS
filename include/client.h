#ifndef GLIDEFS_CLIENT_H
#define GLIDEFS_CLIENT_H

/* Connect to a GlideFS host: joins the WiFi hotspot (ssid/password)
 * then mounts the named CIFS share at mountpoint (or the default
 * ~/GlideFS/<name> if mountpoint is NULL). Returns 0 on success. */
int client_connect(const char *name, const char *ssid, const char *password,
                    const char *mountpoint);

/* Disconnect: unmounts the named share. Returns 0 on success. */
int client_disconnect(const char *name, const char *mountpoint);

/* Resolve the invoking (non-root, if run via sudo) user's home dir. */
void client_home_dir(char *out, size_t len);

#endif
