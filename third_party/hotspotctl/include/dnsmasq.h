#ifndef dnsmasq_h
#define dnsmasq_h
#include "hostapd.h"

typedef struct {
    char ip_addr[128];
    char mac_addr[128];
    char hostname[128];
    char lease[128];
    char state[128];
} device;


int create_dnsmasq_conf(HotspotConfig *cfg);
int get_connected_devices(char* iface);

#endif