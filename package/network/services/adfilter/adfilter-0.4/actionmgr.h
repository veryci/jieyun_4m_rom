#ifndef __ACTION_MGR_H__
#define __ACTION_MGR_H__

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
#include "libh3/h3.h"
#include "action.pb-c.h"
#include "simple-http.h"

//////////////////////////////////////////////////
/*
typedef struct httpf_action_redirect_t {
	char*	host;
	short	port;
	char*	urlPath;
	char*	method;
	char*	httpver;
	char*	referer;
} httpf_action_redirect;


typedef struct httpf_action_data_t {
	char*	data;
	int		size;
} httpf_action_data;


typedef enum httpf_action_type_t {
	httpf_at_none,
	httpf_at_redirect,
	httpf_at_data,
} httpf_action_type;/

*/

typedef struct httpf_action_t httpf_action;

typedef int (*action_query_callback)(redsocks_client *client, httpf_action* action, int ok, httpf_action* new_action);

struct httpf_action_t {
	HttpAction*				action;
	int						unpack;
	redsocks_client*		client;
	action_query_callback 	querycb;

	simple_http_client* 	http_query;

	int		(*release)(struct httpf_action_t *action);
};

int httpf_release_action(httpf_action* action);

///////////////////////////////////////////////

typedef struct httpf_action_mgr_t {
	bool			initd;	
	httpf_action* 	(*query_cache_action)(struct httpf_action_mgr_t* mgr, redsocks_client *client, action_query_callback cb);	

} httpf_action_mgr;

httpf_action_mgr*  get_action_mgr();

#endif // __ACTION_MGR_H___
