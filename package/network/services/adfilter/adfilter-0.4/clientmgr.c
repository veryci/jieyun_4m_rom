#include "clientmgr.h"
#include "nethelper.h"
#include <sys/ioctl.h> 
#include <sys/socket.h> 
#include <net/if.h> 
#include <netinet/if_ether.h> 
#include <string.h>
#include <arpa/inet.h>
#include <string.h>
#include "report.h"

#define DEBUG_LOG 0 

extern struct event_base * s_event_base;
extern struct evdns_base * s_dns_base;

#define CLIENT_UPDATE_GAP		(1000 * 60 * 2)

/////////////////////////////////////////////////////////////////////////
static void release_ua_info(ua_info* ua) {
	if (ua) {
		if (ua->ua) {
			free(ua->ua);
			ua->ua = NULL;
		}
		ua->cnt = 0;
		free(ua);
		ua = NULL;
	}
}

static bool	same_ua(struct ua_info_t* info, char* ua, int ua_len) {
	if (ua_len == info->ua_len) {
		return strncasecmp(ua, info->ua, ua_len) == 0;
	}
	return false;
}

static void ua_report_create(client_info* client, ua_info* ua, char* host, int host_len, char* url, int url_len) {
	report_ua_find(client->mac, client->ip, ua->ua, host, host_len, url, url_len);	
}

static ua_info* create_ua_info(struct client_info_t* client, char* ua, int ua_len, char* host, int host_len, char* url, int url_len) {
	ua_info* info = malloc(sizeof(ua_info));
	memset(info, 0, sizeof(ua_info));

	char* ua_cpy = malloc(ua_len+1);
	memcpy(ua_cpy, ua, ua_len);
	ua_cpy[ua_len] = 0;
	info->ua = ua_cpy;
	info->ua_len = ua_len;

	info->cnt = 1;
	info->client = client;
	info->release = release_ua_info;
	info->same_ua = same_ua;
	ua_report_create(client, info, host, host_len, url, url_len);
	return info;	
}

//////////////////////////////////////////////////////////////////////////
static void mac_to_bin(char* mac ,char mac_bin[6]) {
	int m1,m2,m3,m4,m5,m6;	
	sscanf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", &m1, &m2, &m3, &m4, &m5, &m6);
	mac_bin[0] = (char)m1;
	mac_bin[1] = (char)m2;
	mac_bin[2] = (char)m3;
	mac_bin[3] = (char)m4;
	mac_bin[4] = (char)m5;
	mac_bin[5] = (char)m6;
}

static void str_tolower(char* src) {
	while (src && (*src)) {
		*src = tolower(*src);
		src++;
	}
}

static void release_client_info(client_info* info) {
	if (info) {
		if (info->mac) {
			free(info->mac);
			info->mac = NULL;
		}
		if (info->ip) {
			free(info->ip);
			info->ip = NULL;
		}
		for (int i = 0; i < info->ua_cnt; i++) {
			info->ua_list[i]->release(info->ua_list[i]);
			info->ua_list[i] = NULL;
		}
		if (info->ua_list) {
			free(info->ua_list);
			info->ua_list = NULL;
		}
		info->ua_cnt = info->ua_total = 0;
		free(info);
		info = NULL;
	}
}

static ua_info* find_ua_info(client_info* client, char* ua, int ua_len) {
	for (int i = 0; i < client->ua_cnt; i++) {
		ua_info* info = client->ua_list[i];
		if (info->same_ua(info, ua, ua_len))
			return info;
	}
	return NULL;
}

static int client_add_ua(struct client_info_t* client, char* ua, int ua_len, char* host, int host_len, char* url, int url_len) {
	ua_info* info = find_ua_info(client, ua, ua_len);
	if (info != NULL) {
		info->cnt++;
		return 0;
	}
	
	info = create_ua_info(client, ua, ua_len, host, host_len, url, url_len);
	if (DEBUG_LOG) log_error(LOG_INFO, "========= find new ua: mac: %s ip:%s ua: %s", client->mac, client->ip, info->ua);	
	if (client->ua_cnt + 1 > client->ua_total) {
		if (client->ua_total > 0)
			client->ua_total *= 2;
		else
			client->ua_total = 50;
		
		if (DEBUG_LOG) log_error(LOG_INFO, "externding ua list size to: %d", client->ua_total);
		int size = sizeof(ua_info*) * client->ua_total;
		ua_info** list = malloc(size);
		memset(list, 0, size);
		if (client->ua_cnt && client->ua_list) {
			memcpy(list, client->ua_list, sizeof(ua_info*) * client->ua_cnt);
		}
		if (client->ua_list) {
			free(client->ua_list);
			client->ua_list = NULL;
		}
		client->ua_list = list;
	}
	client->ua_list[client->ua_cnt++] = info;		

	return 0;
}

static void client_start_update(struct client_info_t* info) {
	info->updating = 1;
}

static void client_report_active(struct client_info_t* info) {
	if (DEBUG_LOG) log_error(LOG_INFO, "client: mac:%s ip:%s active: %d", info->mac, info->ip, info->active);
	report_client_find(info->mac, info->ip, info->active);
}

static void	client_update(struct client_info_t* info){
	info->updating = 0;
	if (info->active == 0) {
		info->active = 1;
		client_report_active(info);
	}
}

static void	client_stop_update(struct client_info_t* info) {
	if (info->updating == 1) {
		info->active = 0;
		client_report_active(info);
	}
	info->updating = 0;
}

static client_info* create_client_info(char* mac, char* ip) {
	client_info* info = malloc(sizeof(client_info));
	memset(info, 0, sizeof(client_info));
	info->mac = strdup(mac);
	info->ip = strdup(ip);

	// str_tolower(info->mac);
	// mac_to_bin(info->mac, info->mac_bin);
	
	struct in_addr ia;
	if (inet_aton(ip, &ia)) {
		info->ip_addr = ia;	
	}
	info->release = release_client_info;
	info->add_ua = client_add_ua;

	info->start_update = client_start_update;
	info->stop_update = client_stop_update;
	info->update  = client_update;
	
	info->active = 1;
	client_report_active(info);
	return info;	
}

////////////////////////////////////////////////////////////////////////////
static int	update_client_from_arp (struct client_mgr_t* mgr);

static client_mgr* s_client_mgr = NULL;
static int release_client_mgr(client_mgr* mgr ) {
	if (mgr) {
		for (int i = 0; i < mgr->client_cnt; i++) {
			client_info* info = mgr->client_list[i];
			if (!info) continue;

			info->release(info);
			mgr->client_list[i] = NULL;
		}	
		if (mgr->client_list) {
			free(mgr->client_list);
			mgr->client_list = NULL;
		}
		mgr->client_cnt = mgr->client_total = 0;
		free(mgr);
		mgr = NULL;
		s_client_mgr = NULL;
	}
	return 0;
}

static client_info*	find_client_by_arp(client_mgr *mgr, char* mac, char* ip) {
	for (int i = 0; i < mgr->client_cnt; i++) {
		client_info* info = mgr->client_list[i];
		if (!info) continue;

		if (strcmp(mac, info->mac) == 0 && strcmp(ip, info->ip) == 0) {
			return info;
		}
	}
	if (DEBUG_LOG) log_error(LOG_INFO, "cannot find client of mac");
	return NULL;
}

static client_info* find_client_by_addr(client_mgr* mgr, struct in_addr addr) {
	for (int i = 0; i < mgr->client_cnt; i++) {
		client_info* info = mgr->client_list[i];
		if (info->active && addr.s_addr == info->ip_addr.s_addr) {
			return info;
		}
	}
	return NULL;
}

static int add_ua(struct client_mgr_t* mgr, struct sockaddr_in addr, char* ua, int ua_len, char* host, int host_len, char* url, int url_len) {
	client_info* info = find_client_by_addr(mgr, addr.sin_addr);
	if (info == NULL) {
		if (DEBUG_LOG) log_error(LOG_INFO, "cannot find ua by addr updating arp");
		update_client_from_arp(mgr);
	}

	info = find_client_by_addr(mgr, addr.sin_addr);
	if (info == NULL) {
		if (DEBUG_LOG) log_error(LOG_INFO, "cannot find ua after updating arp");
		return -1;
	}

	info->add_ua(info, ua, ua_len, host, host_len, url, url_len);
	return 0;
}

static void add_client_info(client_mgr* mgr, client_info* info) {
	if (mgr->client_cnt + 1 >  mgr->client_total) {
		if (mgr->client_total > 0)
			mgr->client_total *= 2;
		else
			mgr->client_total = 50;
		
		if (DEBUG_LOG) log_error(LOG_INFO, "externding client list size to: %d", mgr->client_total);

		int size = sizeof(client_info*) * mgr->client_total;
		client_info** list = malloc(size);
		memset(list, 0, size);
		if (mgr->client_cnt && mgr->client_list) {
			memcpy(list, mgr->client_list, sizeof(client_info*) * mgr->client_cnt);
		}
		if (mgr->client_list) {
			free(mgr->client_list);
			mgr->client_list = NULL;
		}
		mgr->client_list = list;
	}
	mgr->client_list[mgr->client_cnt++] = info;		
}

extern arp_info s_arp_list[50];
extern int		s_arp_cnt;
static int	update_client_from_arp (struct client_mgr_t* mgr) {
	refresh_arp_info();

	for (int i = 0; i < mgr->client_cnt; i++) {
		client_info* info = mgr->client_list[i];
		if (!info) continue;
		info->start_update(info);
	}

	if (DEBUG_LOG) log_error(LOG_INFO, "find %d arp entry", s_arp_cnt);
	for (int i = 0; i < s_arp_cnt; i++) {
		arp_info* pInfo = &(s_arp_list[i]);
	
		if (DEBUG_LOG) log_error(LOG_INFO, "processsing %d arp, mac: %s ip: %s", i, pInfo->mac, pInfo->ip);

		client_info* clt = find_client_by_arp(mgr, pInfo->mac, pInfo->ip);
		if (clt == NULL) {
			clt = create_client_info(pInfo->mac, pInfo->ip);
			add_client_info(mgr, clt);
			if (DEBUG_LOG) log_error(LOG_INFO, "add new client ok");
		} else {
			clt->update(clt);
		}
	}
	
	// clear disactive entry
	int j = 0;
	for (int i = 0; i < mgr->client_cnt; i++) {
		client_info* info = mgr->client_list[i];
		if (!info) continue;
		info->stop_update(info);
		if (info->active) {
			mgr->client_list[i] = NULL;
			mgr->client_list[j] = info;
			j++;
		} else {
			if (DEBUG_LOG) log_error(LOG_INFO, "find client offline: mac %s ip %s releaseing it", info->mac, info->ip);
			info->release(info);
			mgr->client_list[i] = NULL;
		}
	}
	mgr->client_cnt = j;
	return 0;
}

static bool set_refresh_timer(client_mgr* mgr, int timeout_ms) {
	struct timeval tv;
	tv.tv_sec = timeout_ms/1000;
	tv.tv_usec = (timeout_ms % 1000) * 1000;
	evtimer_add(mgr->refresh_timer, &tv);
	return true;
}

static void client_refresh(evutil_socket_t fd, short what, void *user_data) {
	client_mgr* mgr = (client_mgr*)user_data;
	set_refresh_timer(mgr, CLIENT_UPDATE_GAP);
	update_client_from_arp(mgr);
}

client_mgr* get_client_mgr() {
	if (s_client_mgr == NULL) {
		if (DEBUG_LOG) log_error(LOG_INFO, "initting client mgr");
		client_mgr* mgr = malloc(sizeof(client_mgr));
		memset(mgr, 0, sizeof(client_mgr));
		mgr->release = release_client_mgr;
		mgr->add_ua = add_ua;
		update_client_from_arp(mgr);

		mgr->refresh_timer = evtimer_new(s_event_base, client_refresh, mgr);
		set_refresh_timer(mgr, CLIENT_UPDATE_GAP);
		s_client_mgr = mgr;
	}
	return s_client_mgr;
}
