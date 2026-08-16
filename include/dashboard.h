#ifndef GLIDEFS_DASHBOARD_H
#define GLIDEFS_DASHBOARD_H

/* Fork off the dashboard HTTP server (blocking accept loop in the
 * child) bound to GLIDEFS_DASH_PORT. Returns the child pid, or -1
 * on failure. */
int dashboard_start(void);

/* Stop a previously started dashboard by pid. */
int dashboard_stop(int pid);

#endif
