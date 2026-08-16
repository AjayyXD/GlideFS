#include <stdio.h>

int help()
{
    fprintf(stdout, "\nUsage: sudo hotspotctl [option] [flags]\n\n");

    fprintf(stdout, "Hotspotctl is a lightweight hotspot manager which allows your computer to act as a router / access point.\n\n");
    fprintf(stdout, "Options:\n");
    fprintf(stdout, "  --help              Show this help message and exit.\n");
    fprintf(stdout, "  --version           Show the program's version number and exit.\n");
    fprintf(stdout, "  -a                  Auto mode. Uses defaults, but allows flag overrides (e.g., -a -s MyWifi).\n");
    fprintf(stdout, "  -m                  Manual mode. Requires all mandatory flags to be specified.\n\n");
    fprintf(stdout, "Flags:\n");
    fprintf(stdout, "  -s <ssid>           Name of the access point.                                         (e.g. MyWifi)\n");
    fprintf(stdout, "  -p <password>       Password for the connection, between 8 to 63 characters.          (e.g. strongpass)\n");
    fprintf(stdout, "  -i <interface>      Interface to be used for hotspot.                                 (e.g. wlp8s0)\n");
    fprintf(stdout, "  -u <uplink>         Interface from which internet should be shared.                   (e.g. enp7s0)\n");
    fprintf(stdout, "  -b <band>           Band: g = 2.4GHz, a = 5GHz                                        (e.g. -b a)\n");
    fprintf(stdout, "  -c <channel>        Channel to be used for the hotspot                                (e.g. 6, 36, 48)\n");
    fprintf(stdout, "  -r <region>         Region / Country code                                             (e.g. IN, US)\n");
    fprintf(stdout, "  -d                  Debug mode (optional, requires no arguments).\n\n");
    return 0;
}