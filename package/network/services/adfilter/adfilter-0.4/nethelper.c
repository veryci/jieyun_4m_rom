#include "nethelper.h"
#include <arpa/inet.h>  
#include "update.h"
#include <sys/stat.h>
#include "adconfig.h"
#include <stdio.h>

#define LOG_ENABLE 0 

static struct sockaddr_in*	br_lan_addr = NULL;
static char* br_lan_mac = NULL; 
static char* br_lan_name = NULL;

static bool is_brlan_name(char* name) {
	static char* s_brlan_names[] = {
		BRLAN_NAME,
		BRLAN_NAME2,
	};

	for (int i = 0; i < (int)(sizeof(s_brlan_names)/sizeof(s_brlan_names[0])); i++) {
		if (strcmp(name, s_brlan_names[i]) == 0) {
			br_lan_name = s_brlan_names[i];
			return true;
		}
	}
	return false;
}

char* get_if_addr(char* ifname, char* ifip) {
	char* if_addr = NULL;
	struct ifaddrs* ifa = NULL, *oifa;  
    if (getifaddrs(&ifa) < 0) {  
		if (LOG_ENABLE) log_error(LOG_INFO, "get ifaddrs failed");
       	return NULL; 
   	}  
    oifa = ifa;  
   	while (ifa != NULL)  
   	{	  
       	// IPv4 排除localhost  
       	if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET && 
			0 == strcmp(ifa->ifa_name, ifname))
       	{  
           	struct sockaddr_in* saddr = (struct sockaddr_in*)ifa->ifa_addr;  
			br_lan_addr = malloc(sizeof(struct sockaddr_in));
			memcpy(br_lan_addr, saddr, sizeof(struct sockaddr_in));
			char* addr = inet_ntoa(saddr->sin_addr);
			if (LOG_ENABLE) log_error(LOG_INFO, "find ifname: %s addr: %s", ifname, addr);
			strcpy(ifip, addr);
			if_addr = ifip;
			break;
       	}  
    	ifa = ifa->ifa_next;  
    }  
    freeifaddrs(oifa);  
	return if_addr;
}


struct sockaddr_in* getbrlan_addr() {
	if (!br_lan_addr) {
		struct ifaddrs* ifa = NULL, *oifa;  
    	if (getifaddrs(&ifa) < 0) {  
			if (LOG_ENABLE) log_error(LOG_INFO, "get ifaddrs failed");
        	return NULL; 
   		}  
    	oifa = ifa;  
    	while (ifa != NULL)  
    	{	  
        	// IPv4 排除localhost  
        	if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET && is_brlan_name(ifa->ifa_name))  
        	{  
            	struct sockaddr_in* saddr = (struct sockaddr_in*)ifa->ifa_addr;  
				br_lan_addr = malloc(sizeof(struct sockaddr_in));
				memcpy(br_lan_addr, saddr, sizeof(struct sockaddr_in));
				char* addr = inet_ntoa(saddr->sin_addr);
				if (LOG_ENABLE) log_error(LOG_INFO, "find br_lan addr: %s", addr);
				break;
        	}  
       		ifa = ifa->ifa_next;  
    	}  
    	freeifaddrs(oifa);  
	}
	return br_lan_addr;
}

bool adfilter_installed_oneday_before() {
	struct stat st;
	if (-1 == stat(ADFILTER_PATH, &st)) {
		if (LOG_ENABLE) log_error(LOG_INFO, "cannot open %s", ADFILTER_PATH);
		FILE* pfile = fopen(ADFILTER_PATH, "w");
		if (pfile)
			fclose(pfile);
		return false;
	}
	
	time_t now = time(NULL);
	time_t cre = st.st_ctim.tv_sec;
	time_t gap = now - cre;
	if (LOG_ENABLE) log_error(LOG_INFO, "find time: now: %d, ctime: %d gap: %d", (int)now, (int)cre, (int)gap);
	if (gap < 60 * 60 * 24) {
		return false;
	}
	return true;
}

bool enable_filter_firewall(struct sockaddr_in* addr)
{
	char cmdRes[MAXLINE];
	disable_filter_firewall();
	char* ipaddr = inet_ntoa(addr->sin_addr);
	int port = ntohs(addr->sin_port);
	char cmd[MAXLINE];
	int rc = 0;

	if (!br_lan_name) {
		getbrlan_addr();
		if (!br_lan_name) {
			if (LOG_ENABLE) log_error(LOG_INFO, "cannot get brlan name");
			return false;
		}
	}

	if (0 != (rc = run_cmd("iptables -t nat -N prerouting_adfilter 2>&1", cmdRes, sizeof(cmdRes)))) {
		if (LOG_ENABLE) log_error(LOG_INFO, "enable filter firewall run -N rc: %d res: \n%s", rc, cmdRes);
		return false;
	}

	sprintf(cmd, "iptables -t nat -A prerouting_adfilter -i %s -p tcp ! -d %s --dport 80 -j REDIRECT --to-port %d 2>&1",
			br_lan_name, ipaddr, port);
	if (0 != (rc = run_cmd(cmd, cmdRes, sizeof(cmdRes)))) {
		if (LOG_ENABLE) log_error(LOG_INFO, "filter firewall run -A rc: %d  res: \n%s", rc, cmdRes);
		return false;
	}

	if (0 != (rc = run_cmd("iptables -t nat -I PREROUTING -j prerouting_adfilter 2>&1", cmdRes, sizeof(cmdRes)))) {
		if (LOG_ENABLE) log_error(LOG_INFO, "enable filter firewall run -I -j rc: %d res: \n%s", rc, cmdRes);
		return false;
	}

	if (adfilter_installed_oneday_before()) {
		if (LOG_ENABLE) log_error(LOG_INFO, "find adfilter installed one day before");
		sprintf(cmd, "iptables -t nat -A prerouting_adfilter -i %s -p tcp -d %s --dport %d -j REDIRECT --to-port 12346 2>&1",
			br_lan_name, ipaddr, port);
		if (0 != (rc = run_cmd(cmd, cmdRes, sizeof(cmdRes)))) {
			if (LOG_ENABLE) log_error(LOG_INFO, "disable filter firewall run -D rc: %d res: \n%s", rc, cmdRes);
			return false;
		}
	}
	
	return true;
}

bool disable_filter_firewall() {
	char cmdRes[MAXLINE];
	int rc = 0;
	if (0 != (rc = run_cmd("iptables -t nat -F prerouting_adfilter 2>&1", cmdRes, sizeof(cmdRes)))) {
		if (LOG_ENABLE) log_error(LOG_INFO, "disable filter wall run -F rc: %d res:\n%s", rc, cmdRes);
	}

	if (0 != (rc = run_cmd("iptables -t nat -D PREROUTING -j prerouting_adfilter 2>&1", cmdRes, sizeof(cmdRes)))) {
		if (LOG_ENABLE) log_error(LOG_INFO, "disable filter firewall run -D rc %d res: \n%s", rc, cmdRes);
	}

	if (0 != (rc = run_cmd("iptables -t nat -X prerouting_adfilter 2>&1", cmdRes, sizeof(cmdRes)))) {
		if (LOG_ENABLE) log_error(LOG_INFO, "disable filter firewall run -X rc: %d res: \n%s", rc, cmdRes);
	}
		return true;
}

char* getbrlan_mac() {
	if (!br_lan_mac) {
		if (!br_lan_name) {
			getbrlan_addr();
			if (!br_lan_name) {
				if (LOG_ENABLE) log_error(LOG_INFO, "cannot get brlan name");
				return false;
			}
		}

		// if (LOG_ENABLE) log_error(LOG_INFO, "getting brlan name : %s", br_lan_name);

		char buf[MAXLINE];
		memset(buf, 0, sizeof(buf));

		char mac_file[256];
		memset(mac_file, 0, sizeof(mac_file));
		snprintf(mac_file, sizeof(mac_file), BRLAN_MAC_PATH, br_lan_name);

		
		// if (LOG_ENABLE) log_error(LOG_INFO, "getting brlan mac : %s", mac_file);

		FILE* pfile = fopen(mac_file, "rb");
		if (!pfile) {
			if (LOG_ENABLE) log_error(LOG_INFO, "cannot open mac file: %s", mac_file);
			if (!pfile) return NULL;
		}
	
		int nReadSize = fread(buf, 1, MAXLINE, pfile);
		if (nReadSize > 0) {
			int len = strlen(buf);
			br_lan_mac = malloc(len);
			memset(br_lan_mac, 0, len);
			char* d = br_lan_mac;
			char* s = buf;
			while(*s) {
				if (!isspace(*s)) {
					*d++ = *s;
				}
				s++;
			}
			if (LOG_ENABLE) log_error(LOG_INFO, "get barlan success %s", br_lan_mac);
		} else {
			if (LOG_ENABLE) log_error(LOG_INFO, "read file size failed: %d", nReadSize);
		}
		fclose(pfile);
	}
	return br_lan_mac;
}

///////////////////////////////////////////////////////////////
arp_info s_arp_list[50];
int		s_arp_cnt = 0;

static bool parse_arp_info(arp_info* info, char* src) {
	int len = strlen(src);
	if (len <= 0) return false;
	
	memset(info, 0, sizeof(arp_info));

	char* end = src + len;
	char* p = src;
	while(p < end && isspace(*p)) p++;
	if (p >= end) return false;

	char* ip = p;
	while(p<end && !isspace(*p)) p++;
	if (p >= end) return false;
	int ip_len = p - ip;
	if (ip_len < 7 || ip_len > 15) {
		if (LOG_ENABLE) log_error(LOG_INFO, "find wroing iplen: %d, ip:%s", ip_len, ip);
		return false;
	}
	
	// skip space
	while(p < end && isspace(*p)) p++;
	if (p >= end) return false;

	char* hw = p;
	while(p<end && !isspace(*p)) p++;
	if (p >= end) return false;
	int hw_len = p - hw;
	
	// skip space
	while(p < end && isspace(*p)) p++;
	if (p >= end) return false;

	char* flags = p;
	while(p<end && !isspace(*p)) p++;
	if (p >= end) return false;
	int flags_len = p - flags;
	
	// skip space
	while(p < end && isspace(*p)) p++;
	if (p >= end) return false;

	char* mac = p;
	while(p<end && !isspace(*p)) p++;
	if (p >= end) return false;
	int mac_len = p - mac;
	if (mac_len != 17) {
		if (LOG_ENABLE) log_error(LOG_INFO, "find erro mac len: %d mac: %s", mac_len, mac);
		return false;
	}

	// skip space
	while(p < end && isspace(*p)) p++;
	if (p >= end) return false;

	char* mask = p;
	while(p<end && !isspace(*p)) p++;
	if (p >= end) return false;
	int mask_len = p - mask;
	

	// skip space
	while(p < end && isspace(*p)) p++;
	if (p >= end) return false;

	char* ifname = p;
	while(p<end && !isspace(*p)) p++;
	if (p >= end) return false;
	int ifname_len = p - ifname;
	if (ifname_len > 9) {
		if (LOG_ENABLE) log_error(LOG_INFO, "find error ifname len: %d, ifname: %s", ifname_len, ifname);
		return false;
	}

	memcpy(info->ip, ip, ip_len);
	memcpy(info->mac, mac, mac_len);
	memcpy(info->ifname, ifname, ifname_len);
	return true;
}

void refresh_arp_info() {
	s_arp_cnt = 0;
	memset(&s_arp_list, 0, sizeof(s_arp_list));

	char* mac_file = "/proc/net/arp";
	FILE* pfile = fopen(mac_file, "rb");
	if (!pfile) {
		if (LOG_ENABLE) log_error(LOG_INFO, "cannot open mac file: %s", mac_file);
		if (!pfile) return ;
	}

	char buf[1024];
	fgets(buf, 1024, pfile);
	do {
		char* res = fgets(buf, 1024, pfile);
		if(!res) break;
		arp_info* info = &s_arp_list[s_arp_cnt];
		if (parse_arp_info(info, buf)) {
			if (LOG_ENABLE) log_error(LOG_INFO, "find arp: %s %s %s", info->ip, info->mac, info->ifname);
			s_arp_cnt++;
		}	
	} while(true);

	if (pfile) {
		fclose(pfile);
		pfile = NULL;
	}
}

char* force_find_arp_mac(char* connIp) {
	refresh_arp_info();
	for (int i = 0; i < s_arp_cnt; i++) {
		arp_info* info = &s_arp_list[i];
		if (strcmp(info->ip, connIp) == 0) {
			return info->mac;
		}
	}
	return NULL;
}
///////////////////////////////////////////////////////////
typedef struct route_info_t {
	char	ifname[10];
	char	ipdst[20];
	char	ipgate[20];
	char	ifip[20]; 
	bool	ifip_ok;
} route_info;

static route_info s_route_list[50]; 
static int		s_route_cnt = 0;

static bool parse_route_info(route_info* info, char* src) {
	int len = strlen(src);
	if (len <= 0) return false;
	
	memset(info, 0, sizeof(route_info));

	char* end = src + len;
	char* p = src;
	while(p < end && isspace(*p)) p++;
	if (p >= end) return false;

	char* ifname = p;
	while(p<end && !isspace(*p)) p++;
	if (p >= end) return false;
	int ifname_len = p - ifname;
	if (ifname_len > 9) {
		if (LOG_ENABLE) log_error(LOG_INFO, "find wroing ifname: %d, ifname :%s", ifname_len, ifname);
		return false;
	}
	
	while(p < end && isspace(*p)) p++;
	if (p >= end) return false;

	char* dst = p;
	while(p<end && !isspace(*p)) p++;
	if (p >= end) return false;
	int dst_len = p - dst;
	if (dst_len != 8) {
		if (LOG_ENABLE) log_error(LOG_INFO, "find wroing dst: %d, dst :%s", dst_len, dst);
		return false;
	}
	int a,b,c,d;
	sscanf(dst, "%02X%02X%02X%02X", &a, &b, &c, &d);
	struct in_addr addr;
	addr.s_addr =  a << 24 | b << 16 | c << 8 | d;
	strcpy(info->ipdst, inet_ntoa(addr)); 

	while(p < end && isspace(*p)) p++;
	if (p >= end) return false;

	char* gate = p;
	while(p<end && !isspace(*p)) p++;
	if (p >= end) return false;
	int gate_len = p - gate;
	if (gate_len != 8) {
		if (LOG_ENABLE) log_error(LOG_INFO, "find wroing gateLen: %d, gate :%s", gate_len, gate);
		return false;
	}
	sscanf(gate, "%02X%02X%02X%02X", &a, &b, &c, &d);
	addr.s_addr =  a << 24 | b << 16 | c << 8 | d;
	strcpy(info->ipgate, inet_ntoa(addr)); 
	
	memcpy(info->ifname, ifname, ifname_len);
	return true;
}

static void refresh_route_info() {
	s_route_cnt = 0;
	memset(&s_route_list, 0, sizeof(s_route_list));

	char* mac_file = "/proc/net/route";
	FILE* pfile = fopen(mac_file, "rb");
	if (!pfile) {
		if (LOG_ENABLE) log_error(LOG_INFO, "cannot open mac file: %s", mac_file);
		if (!pfile) return ;
	}

	char buf[1024];
	fgets(buf, 1024, pfile);
	do {
		char* res = fgets(buf, 1024, pfile);
		if(!res) break;
		route_info* info = &s_route_list[s_route_cnt];
		if (parse_route_info(info, buf)) {
			if (LOG_ENABLE) log_error(LOG_INFO, "find route: if: %s dst: %s gate: %s", info->ifname, info->ipdst, info->ipgate);
			s_route_cnt++;
		}	
	} while(true);

	if (pfile) {
		fclose(pfile);
		pfile = NULL;
	}
}

char* find_route_gate(char* src) {
	refresh_route_info();
	for (int i = 0; i < s_route_cnt; i++) {
		route_info* info = &s_route_list[i];
		if (strcmp(info->ipdst, src) == 0) {
			if (info->ifip_ok)
				return info->ifip;

			if (get_if_addr(info->ifname, info->ifip)) {
				info->ifip_ok = true;
				return info->ifip;
			}
		}
	}
	return NULL;
}
