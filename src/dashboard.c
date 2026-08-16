#include "common.h"
#include "dashboard.h"
#include "state.h"
#include "dashboard_html.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#define REQ_BUF 4096
#define RESP_BUF 65536

/* --- helpers -------------------------------------------------------- */

static void json_escape(const char *in, char *out, size_t outlen) {
    size_t o = 0;
    for (size_t i = 0; in[i] != '\0' && o + 2 < outlen; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '"' || c == '\\') {
            if (o + 2 >= outlen) break;
            out[o++] = '\\';
            out[o++] = c;
        } else if (c == '\n') {
            if (o + 2 >= outlen) break;
            out[o++] = '\\';
            out[o++] = 'n';
        } else if (c == '\r') {
            continue;
        } else if (c == '\t') {
            if (o + 2 >= outlen) break;
            out[o++] = '\\';
            out[o++] = 't';
        } else if (c < 0x20) {
            continue;
        } else {
            out[o++] = (char)c;
        }
    }
    out[o] = '\0';
}

static void run_capture(const char *cmd, char *out, size_t outlen) {
    out[0] = '\0';
    FILE *fp = popen(cmd, "r");
    if (!fp) return;
    size_t used = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp) && used < outlen - 1) {
        size_t l = strlen(line);
        if (used + l >= outlen - 1) l = outlen - 1 - used;
        memcpy(out + used, line, l);
        used += l;
    }
    out[used] = '\0';
    pclose(fp);
}

/* Build a JSON array of connected devices from `ip neighbour`. */
static void build_devices_json(const char *iface, char *out, size_t outlen, int *count_out) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ip neighbour show dev %s 2>/dev/null", iface);
    FILE *fp = popen(cmd, "r");
    size_t used = 0;
    int count = 0;
    used += snprintf(out + used, outlen - used, "[");
    if (fp) {
        char line[256];
        int first = 1;
        while (fgets(line, sizeof(line), fp)) {
            char ip[64] = "", mac[64] = "", state[32] = "";
            char tok[8][64];
            int n = sscanf(line, "%63s %63s %63s %63s %63s %63s",
                            tok[0], tok[1], tok[2], tok[3], tok[4], tok[5]);
            if (n < 1) continue;
            snprintf(ip, sizeof(ip), "%s", tok[0]);
            for (int i = 1; i < n - 1; i++) {
                if (strcmp(tok[i], "lladdr") == 0) {
                    snprintf(mac, sizeof(mac), "%s", tok[i + 1]);
                }
            }
            snprintf(state, sizeof(state), "%s", n > 0 ? tok[n - 1] : "UNKNOWN");
            if (strcmp(state, "FAILED") == 0) continue;
            if (strlen(mac) == 0) snprintf(mac, sizeof(mac), "-");

            if (!first) used += snprintf(out + used, outlen - used, ",");
            used += snprintf(out + used, outlen - used,
                              "{\"ip\":\"%s\",\"mac\":\"%s\",\"state\":\"%s\"}",
                              ip, mac, state);
            first = 0;
            count++;
            if (used >= outlen - 200) break;
        }
        pclose(fp);
    }
    used += snprintf(out + used, outlen - used, "]");
    *count_out = count;
}

static double read_iface_counter(const char *iface, const char *which) {
    char path[160];
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/%s", iface, which);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    double v = -1;
    if (fscanf(f, "%lf", &v) != 1) v = -1;
    fclose(f);
    return v;
}

static void get_throughput(const char *iface, double *rx_bps, double *tx_bps) {
    static double last_rx = -1, last_tx = -1;
    static struct timespec last_ts = {0, 0};

    double rx = read_iface_counter(iface, "rx_bytes");
    double tx = read_iface_counter(iface, "tx_bytes");
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    *rx_bps = 0;
    *tx_bps = 0;

    if (rx >= 0 && tx >= 0 && last_rx >= 0 && last_ts.tv_sec != 0) {
        double dt = (now.tv_sec - last_ts.tv_sec) + (now.tv_nsec - last_ts.tv_nsec) / 1e9;
        if (dt > 0.1) {
            double drx = rx - last_rx;
            double dtx = tx - last_tx;
            if (drx >= 0) *rx_bps = drx / dt;
            if (dtx >= 0) *tx_bps = dtx / dt;
        }
    }
    if (rx >= 0) last_rx = rx;
    if (tx >= 0) last_tx = tx;
    last_ts = now;
}

static void build_status_json(char *out, size_t outlen) {
    GlideState st;
    state_read(&st);

    char iface[32];
    snprintf(iface, sizeof(iface), "%s", strlen(st.iface) ? st.iface : "wlan0");

    char devices[8192];
    int devcount = 0;
    build_devices_json(iface, devices, sizeof(devices), &devcount);

    double rx_bps = 0, tx_bps = 0;
    get_throughput(iface, &rx_bps, &tx_bps);

    char sessions_raw[4096], locks_raw[4096];
    run_capture("smbstatus -p 2>/dev/null", sessions_raw, sizeof(sessions_raw));
    run_capture("smbstatus -L 2>/dev/null", locks_raw, sizeof(locks_raw));

    char sessions_esc[8192], locks_esc[8192];
    json_escape(sessions_raw, sessions_esc, sizeof(sessions_esc));
    json_escape(locks_raw, locks_esc, sizeof(locks_esc));

    char ssid_esc[128], name_esc[256], path_esc[600];
    json_escape(st.ssid, ssid_esc, sizeof(ssid_esc));
    json_escape(st.share_name, name_esc, sizeof(name_esc));
    json_escape(st.share_path, path_esc, sizeof(path_esc));

    snprintf(out, outlen,
        "{"
        "\"ssid\":\"%s\","
        "\"share_name\":\"%s\","
        "\"share_path\":\"%s\","
        "\"iface\":\"%s\","
        "\"host_ip\":\"%s\","
        "\"port\":445,"
        "\"throughput\":{\"rx_bps\":%.1f,\"tx_bps\":%.1f},"
        "\"devices\":%s,"
        "\"device_count\":%d,"
        "\"smb_sessions_raw\":\"%s\","
        "\"smb_locks_raw\":\"%s\""
        "}",
        ssid_esc, name_esc, path_esc, iface, GLIDEFS_HOST_IP,
        rx_bps, tx_bps, devices, devcount, sessions_esc, locks_esc);
}

/* --- tiny HTTP server ------------------------------------------------ */

static void send_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, buf + sent, len - sent);
        if (n <= 0) return;
        sent += (size_t)n;
    }
}

static void handle_client(int fd) {
    char req[REQ_BUF];
    ssize_t n = read(fd, req, sizeof(req) - 1);
    if (n <= 0) { close(fd); return; }
    req[n] = '\0';

    char method[8] = "", path[256] = "";
    sscanf(req, "%7s %255s", method, path);

    char resp[RESP_BUF];

    if (strcmp(path, "/api/status") == 0) {
        char json[49152];
        build_status_json(json, sizeof(json));
        int hlen = snprintf(resp, sizeof(resp),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Cache-Control: no-store\r\n"
            "Connection: close\r\n"
            "Content-Length: %zu\r\n\r\n", strlen(json));
        send_all(fd, resp, (size_t)hlen);
        send_all(fd, json, strlen(json));
    } else {
        int hlen = snprintf(resp, sizeof(resp),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Connection: close\r\n"
            "Content-Length: %u\r\n\r\n", DASHBOARD_HTML_LEN);
        send_all(fd, resp, (size_t)hlen);
        send_all(fd, DASHBOARD_HTML, DASHBOARD_HTML_LEN);
    }
    close(fd);
}

static void dashboard_loop(void) {
    signal(SIGCHLD, SIG_IGN);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { _exit(1); }
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(GLIDEFS_DASH_PORT);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        _exit(1);
    }
    if (listen(srv, 16) != 0) {
        _exit(1);
    }

    while (1) {
        struct sockaddr_in cli;
        socklen_t clilen = sizeof(cli);
        int fd = accept(srv, (struct sockaddr *)&cli, &clilen);
        if (fd < 0) {
            if (errno == EINTR) continue;
            continue;
        }
        handle_client(fd);
    }
}

int dashboard_start(void) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        /* detach from parent's stdio so it doesn't hold the terminal open */
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        dashboard_loop();
        _exit(0);
    }
    usleep(200000);
    return pid;
}

int dashboard_stop(int pid) {
    if (pid <= 0) return 1;
    return kill(pid, SIGTERM);
}
