#ifndef GLIDEFS_HOTSPOT_H
#define GLIDEFS_HOTSPOT_H

/* Locate the hotspotctl binary. Checks PATH, then common install
 * locations, then a build-relative fallback. Returns 1 and fills
 * out_path if found, 0 otherwise. */
int hotspot_locate(char *out_path, size_t out_len);

/* Bring up the local access point with the given ssid/password.
 * Uses auto interface/uplink/band/channel/country detection.
 * Returns 0 on success. */
int hotspot_start(const char *ssid, const char *password, int debug_mode);

/* Tear down the access point. Returns 0 on success. */
int hotspot_stop(void);

/* Fill iface with the wifi interface hotspotctl is using, by reading
 * hotspotctl's own state file. Returns 0 on success. */
int hotspot_get_iface(char *iface, size_t len);

#endif
