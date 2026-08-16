#ifndef GLIDEFS_SHARE_H
#define GLIDEFS_SHARE_H

/* Generate /run/glidefs/smb.conf exposing `path` as `name`.
 * File operations over the share are mapped to the invoking
 * (sudo) user so guests write files the host user actually owns.
 * Returns 0 on success. */
int share_write_conf(const char *name, const char *path);

/* Launch smbd bound to the generated config, in the foreground of a
 * forked child (so we can track its pid). Returns the child pid on
 * success, -1 on failure. */
int share_start_smbd(void);

/* Stop the smbd instance we started (by pid). */
int share_stop_smbd(int pid);

#endif
