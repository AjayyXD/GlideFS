#ifndef GLIDEFS_CLI_H
#define GLIDEFS_CLI_H

void print_help(const char *argv0);
void print_version(void);

int cmd_init(int argc, char *argv[]);
int cmd_share(int argc, char *argv[]);
int cmd_unshare(int argc, char *argv[]);
int cmd_status(int argc, char *argv[]);
int cmd_connect(int argc, char *argv[]);
int cmd_disconnect(int argc, char *argv[]);
int cmd_deps(int argc, char *argv[]);

#endif
