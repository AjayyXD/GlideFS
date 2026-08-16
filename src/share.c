#include "common.h"
#include "share.h"
#include <sys/stat.h>
#include <pwd.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

static void get_invoking_user(char *user, size_t ulen, char *group, size_t glen) {
    const char *sudo_user = getenv("SUDO_USER");
    struct passwd *pw = NULL;

    if (sudo_user && strlen(sudo_user) > 0) {
        pw = getpwnam(sudo_user);
    }
    if (!pw) {
        pw = getpwuid(getuid());
    }
    if (pw) {
        snprintf(user, ulen, "%s", pw->pw_name);
        struct passwd *pw2 = getpwuid(pw->pw_gid);
        if (pw2) {
            snprintf(group, glen, "%s", pw2->pw_name);
        } else {
            snprintf(group, glen, "%s", pw->pw_name);
        }
    } else {
        snprintf(user, ulen, "nobody");
        snprintf(group, glen, "nogroup");
    }
}

int share_write_conf(const char *name, const char *path) {
    ensure_dir(GLIDEFS_RUN_DIR);
    ensure_dir(GLIDEFS_LOG_DIR);
    ensure_dir(GLIDEFS_RUN_DIR "/locks");
    ensure_dir(GLIDEFS_RUN_DIR "/private");
    ensure_dir(GLIDEFS_RUN_DIR "/cache");

    char user[64], group[64];
    get_invoking_user(user, sizeof(user), group, sizeof(group));

    FILE *f = fopen(GLIDEFS_SMB_CONF, "w");
    if (!f) return 1;

    fprintf(f,
        "[global]\n"
        "    workgroup = WORKGROUP\n"
        "    server string = GlideFS Host\n"
        "    netbios name = GLIDEFS\n"
        "    security = user\n"
        "    map to guest = Bad User\n"
        "    guest account = nobody\n"
        "    server role = standalone server\n"
        "    log file = %s/logs/smbd.log\n"
        "    log level = 1\n"
        "    pid directory = %s\n"
        "    lock directory = %s/locks\n"
        "    private dir = %s/private\n"
        "    cache directory = %s/cache\n"
        "    state directory = %s\n"
        "    smb ports = 445\n"
        "    min protocol = SMB2\n"
        "    oplocks = yes\n"
        "    kernel oplocks = yes\n"
        "    level2 oplocks = yes\n"
        "    disable spoolss = yes\n"
        "    load printers = no\n"
        "    printcap name = /dev/null\n"
        "    bind interfaces only = no\n"
        "\n"
        "[%s]\n"
        "    path = %s\n"
        "    comment = Shared via GlideFS\n"
        "    guest ok = yes\n"
        "    read only = no\n"
        "    browsable = yes\n"
        "    writable = yes\n"
        "    oplocks = yes\n"
        "    locking = yes\n"
        "    force user = %s\n"
        "    force group = %s\n"
        "    create mask = 0664\n"
        "    directory mask = 0775\n",
        GLIDEFS_RUN_DIR, GLIDEFS_RUN_DIR, GLIDEFS_RUN_DIR, GLIDEFS_RUN_DIR,
        GLIDEFS_RUN_DIR, GLIDEFS_RUN_DIR,
        name, path, user, group);

    fclose(f);
    return 0;
}

int share_start_smbd(void) {
    pid_t pid = fork();
    if (pid < 0) {
        log_err("fork() failed while starting smbd");
        return -1;
    }
    if (pid == 0) {
        /* child: exec smbd in foreground, don't let it daemonize itself */
        execlp("smbd", "smbd", "-F", "--no-process-group",
               "-s", GLIDEFS_SMB_CONF, (char *)NULL);
        /* if we get here, exec failed */
        fprintf(stderr, "[-] Failed to exec smbd: is samba installed?\n");
        _exit(127);
    }
    /* give smbd a moment to bind/start */
    usleep(400000);

    int status;
    pid_t r = waitpid(pid, &status, WNOHANG);
    if (r == pid) {
        /* smbd already exited -> failure */
        return -1;
    }
    return pid;
}

int share_stop_smbd(int pid) {
    if (pid <= 0) return 1;
    if (kill(pid, SIGTERM) != 0) {
        return 1;
    }
    return 0;
}
