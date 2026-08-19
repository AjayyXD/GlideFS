#include "common.h"
#include "state.h"
#include <sys/stat.h>

int state_write(const GlideState *st) {
    ensure_dir(GLIDEFS_RUN_DIR);
    FILE *f = fopen(GLIDEFS_STATE_FILE, "w");
    if (!f) return 1;
    fprintf(f, "SHARE_NAME=%s\n", st->share_name);
    fprintf(f, "SHARE_PATH=%s\n", st->share_path);
    fprintf(f, "SSID=%s\n", st->ssid);
    fprintf(f, "PASSWORD=%s\n", st->password);
    fprintf(f, "IFACE=%s\n", st->iface);
    fprintf(f, "SMBD_PID=%d\n", st->smbd_pid);
    fprintf(f, "DASHBOARD_PID=%d\n", st->dashboard_pid);
    fprintf(f, "GLIDEFSD_PID=%d\n", st->glidefsd_pid);
    fprintf(f, "TRANSPORT=%d\n", (int)(st->transport));
    fclose(f);
    chmod(GLIDEFS_STATE_FILE, 0644);
    return 0;
}

static void parse_kv(char *line, GlideState *st) {
    char *eq = strchr(line, '=');
    if (!eq) return;
    *eq = '\0';
    char *key = line;
    char *val = eq + 1;
    chomp(val);

    if (strcmp(key, "SHARE_NAME") == 0) snprintf(st->share_name, sizeof(st->share_name), "%s", val);
    else if (strcmp(key, "SHARE_PATH") == 0) snprintf(st->share_path, sizeof(st->share_path), "%s", val);
    else if (strcmp(key, "SSID") == 0) snprintf(st->ssid, sizeof(st->ssid), "%s", val);
    else if (strcmp(key, "PASSWORD") == 0) snprintf(st->password, sizeof(st->password), "%s", val);
    else if (strcmp(key, "IFACE") == 0) snprintf(st->iface, sizeof(st->iface), "%s", val);
    else if (strcmp(key, "SMBD_PID") == 0) st->smbd_pid = atoi(val);
    else if (strcmp(key, "DASHBOARD_PID") == 0) st->dashboard_pid = atoi(val);
    else if (strcmp(key, "GLIDEFSD_PID") == 0) st->glidefsd_pid = atoi(val);
    else if (strcmp(key, "TRANSPORT") == 0) st->transport = (gfs_transport_t)atoi(val);
}

int state_read(GlideState *st) {
    memset(st, 0, sizeof(*st));
    FILE *f = fopen(GLIDEFS_STATE_FILE, "r");
    if (!f) return 1;
    char line[768];
    while (fgets(line, sizeof(line), f)) {
        parse_kv(line, st);
    }
    fclose(f);
    return 0;
}

void state_clear(void) {
    remove(GLIDEFS_STATE_FILE);
}
