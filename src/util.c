#include "common.h"
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

void log_info(const char *fmt, ...) {
    va_list ap;
    fprintf(stdout, "[*] ");
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fprintf(stdout, "\n");
    fflush(stdout);
}

void log_ok(const char *fmt, ...) {
    va_list ap;
    fprintf(stdout, "[+] ");
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fprintf(stdout, "\n");
    fflush(stdout);
}

void log_err(const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "[-] ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    fflush(stderr);
}

int run_cmd(const char *fmt, ...) {
    char cmd[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);
    return system(cmd);
}

int run_cmd_silent(const char *fmt, ...) {
    char cmd[1024];
    char full[1100];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);
    snprintf(full, sizeof(full), "%s > /dev/null 2>&1", cmd);
    return system(full);
}

int ensure_dir(const char *path) {
    char tmp[512];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') tmp[len - 1] = '\0';

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return 1;
    }
    return 0;
}

void require_root(const char *argv0) {
    if (geteuid() != 0) {
        log_err("GlideFS requires root privileges for this operation.");
        log_err("Please run again using: sudo %s", argv0);
        exit(1);
    }
}

void chomp(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[--n] = '\0';
    }
}
