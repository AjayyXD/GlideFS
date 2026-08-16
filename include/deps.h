#ifndef GLIDEFS_DEPS_H
#define GLIDEFS_DEPS_H

typedef enum {
    DEP_ROLE_HOST,
    DEP_ROLE_CLIENT,
    DEP_ROLE_BOTH
} DepRole;

/* Prints a report of missing runtime dependencies for the given role.
 * Returns the number of missing binaries (0 = everything present). */
int deps_check(DepRole role, int verbose);

/* Detects the system's package manager and installs whatever is
 * missing for the given role. Requires root. Returns 0 on success. */
int deps_install(DepRole role);

#endif
