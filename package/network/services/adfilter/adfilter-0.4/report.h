#ifndef __REPORT_H__
#define __REPORT_H__
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
#include "business.pb-c.h"
#include "simple-http.h"

typedef struct report_mgr_t {
	int32_t				nextid;
	char				curRptFile[256];
	simple_http_client* report_client;	
	struct event*		refresh_timer;
	struct timeval		last_succ_time;

	int (*release) (struct report_mgr_t* mgr);
} report_mgr;

bool clear_rpt_cache();

report_mgr* init_report_mgr();
bool report_start_up();
bool report_target_hit(char* url, char* host, char* ua, char* refer, int ruleid, int targetid, char* connIp, char* connMac);
bool report_crash();
bool report_client_find(char* mac, char* ip, int active);
bool report_ua_find(char* mac, char* ip, char* ua, char* host, int host_len, char* url, int url_len);
bool report_ios_work(char* guid, char* location, char* url, int status, char* endPointMac);
bool report_rpt_hit(char* url, char* headers, int rptid, char* connIp, char* connMac);

#endif // __REPORT_H__
