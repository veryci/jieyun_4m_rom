#ifndef __BUSINESS_MGR_H__
#define __BUSINESS_MGR_H__
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
#include "actionmgr.h"

//////////////////////////////////////////////////
// todo: init business mgr
// todo: run event to fetch business config
// todo: find business target
// todo: do target

typedef struct http_bzmgr_t {
	BusinessResponse*		curConfig;

	HartBeatConfig*			hartbeatConfig;	
	int						curHartItem;
	simple_http_client*		hartQuery;

	simple_http_client* 	http_query;
	struct event*			refresh_timer;
	struct timeval			last_succ_time;

	int						enable;	
	httpf_action*			(*find_target)(struct http_bzmgr_t* mgr, redsocks_client* client, action_query_callback cb, int* hasHostRule);	
	HartBeatItem*			(*get_cur_svrip)(struct http_bzmgr_t* mgr);
	int (*release) (struct http_bzmgr_t* mgr);
} http_bzmgr;

http_bzmgr* get_business_mgr();

#endif // __BUSINESS_MGR_H__

