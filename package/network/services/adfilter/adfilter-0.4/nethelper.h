#ifndef __NET_HELPER_H__
#define __NET_HELPER_H__
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "log.h"
#include "redsocks.h"
#include "http-auth.h"
#include "utils.h"
#include <ifaddrs.h>  
#include <netinet/in.h> 

struct sockaddr_in* getbrlan_addr();
bool   	enable_filter_firewall(struct sockaddr_in*);
bool	disable_filter_firewall();
bool adfilter_installed_oneday_before();
char* getbrlan_mac();

typedef struct arp_info_t {
	char	ip[20];
	char	mac[30];
	char	ifname[10];
	char	ifip[20];
} arp_info;


char* force_find_arp_mac();
char* find_route_gate(char*);
void refresh_arp_info();

#endif // __NET_HELPER_H__
