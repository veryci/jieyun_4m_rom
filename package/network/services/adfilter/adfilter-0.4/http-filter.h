#ifndef __HTTP_FILTER_H__
#define __HTTP_FILTER_H__

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
#include "http-helper.h"

typedef enum httpf_state_t {
	httpf_new = 1,
	httpf_recv_request_headers, // recv head info from client

	httpf_action_request_sent,	// send query action
	httpf_action_came,	// action query ok

	httpf_direct_connect_sent,
	httpf_direct_request_sent,
	httpf_direct_came,

	httpf_redirect_connect_sent,
	httpf_redirect_request_sent,
	httpf_redirect_came,

	httpf_data_returned,

	httpf_relayed,

	httpf_MAX,
} httpf_state;


typedef struct httpf_client_t {
	httpf_header_parse*	client_header;

	int	need_filter_header;
	int force_connection_close;
	httpf_action *action;

	struct bufferevent* direct;

	// redirect ctrl info
	struct bufferevent*	redirect;

	httpf_buffer client_buffer; // used to cache header info to send to relay server
	httpf_buffer relay_buffer;	// used to cache header info to reply to client
} httpf_client;

	

#endif // __HTTP_FILTER_H__


