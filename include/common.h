#ifndef GLIDEFS_COMMON_H
#define GLIDEFS_COMMON_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define GLIDEFS_RUN_DIR   "/run/glidefs"
#define GLIDEFS_STATE_FILE GLIDEFS_RUN_DIR "/glidefs.state"
#define GLIDEFS_PID_FILE   GLIDEFS_RUN_DIR "/glidefs.pid"
#define GLIDEFS_SMB_CONF   GLIDEFS_RUN_DIR "/smb.conf"
#define GLIDEFS_SMB_PID    GLIDEFS_RUN_DIR "/smbd.pid"
#define GLIDEFS_DASH_PID   GLIDEFS_RUN_DIR "/dashboard.pid"
#define GLIDEFS_LOG_DIR    GLIDEFS_RUN_DIR "/logs"

#define GLIDEFS_HOST_IP    "192.168.42.1"
#define GLIDEFS_DASH_PORT  8090
#define GLIDEFS_VERSION    "0.1.0"

#define CLIENT_STATE_DIR_FMT "%s/.glidefs"
#define CLIENT_STATE_FILE_FMT "%s/.glidefs/mounts"

/* logging helpers */
void log_info(const char *fmt, ...);
void log_err(const char *fmt, ...);
void log_ok(const char *fmt, ...);

/* run a shell command, returns exit status (like system()) */
int run_cmd(const char *fmt, ...);

/* run a shell command silently (stdout/stderr suppressed) */
int run_cmd_silent(const char *fmt, ...);

/* ensure directory exists (mkdir -p equivalent, single level + parents best-effort) */
int ensure_dir(const char *path);

/* require the calling user to be root, exits(1) otherwise */
void require_root(const char *argv0);

/* trim trailing newline */
void chomp(char *s);

#endif
