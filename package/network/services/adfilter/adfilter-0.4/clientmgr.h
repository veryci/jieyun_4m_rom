#ifndef __CLIENT_MGR_H__
#define __CLIENT_MGR_H__

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
struct ua_info_t;
struct client_info_t;

typedef struct ua_info_t {
	struct client_info_t* client;	
	int				cnt;
	char*			ua;
	int				ua_len;
	bool			(*same_ua)(struct ua_info_t*, char*, int ua_len);
	void			(*release)(struct ua_info_t*);
} ua_info;

typedef struct client_info_t {
	char* 			mac;
	char* 			ip;
	char  			mac_bin[6];
	struct in_addr	ip_addr;	

	int				ua_total;
	int				ua_cnt;	
	ua_info**		ua_list;
	int				(*add_ua)(struct client_info_t*, char* ua, int ua_len, char* host, int host_len, char* url, int url_len);

	int				updating;
	int				active;
	void			(*start_update)(struct client_info_t*);
	void			(*update)(struct client_info_t*);
	void			(*stop_update)(struct client_info_t*);

	void			(*release)(struct client_info_t*);
} client_info;

typedef struct client_mgr_t {
	int				client_total;
	int				client_cnt;
	client_info**	client_list;	

	struct event*	refresh_timer;

	int				(*add_ua)(struct client_mgr_t* mgr, struct sockaddr_in addr, char* ua, int ua_len, char* host, int host_len, char* url, int url_len);
	int 			(*release) (struct client_mgr_t* mgr);
} client_mgr;

client_mgr* get_client_mgr();

#endif // __CLIENT_MGR_H__
