#ifndef __SIMPLE_HTTP_H__
#define __SIMPLE_HTTP_H__

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
#include "utils.h"
#include "http-helper.h"


typedef struct simple_http_client_t simple_http_client;

typedef int (*http_res_cb)(simple_http_client* http, void* user_data);

typedef enum _simple_http_state {
	simple_http_new = 0,
	simple_http_conn_sent,
	simple_http_request_sent,
	simple_http_data_recving,
	simple_http_data_ok,
	simple_http_conn_failed,
}simple_http_state;

struct simple_http_client_t {
	simple_http_state		state;
	int						out_header_len;
	httpf_buffer			out;
	struct event*			timeout;
	
	httpf_response_parse*	resp_header;
	int						content_len;
	int						resp_status;
	httpf_buffer			body;

	http_res_cb				rescb;
	void*					rescb_userdata;

	struct bufferevent* 	conn;
	char*					host;
	int						port;
	char*					url;
	char*					method;

	bool					encrypt;
	httpf_buffer			url_key;
	httpf_buffer			data_key;
	httpf_buffer			encypt_url;
	
	int (*add_header)(simple_http_client* http, char* key, char* val);
	int	(*do_request)(simple_http_client* http, char* data, int size, int timeout_ms);
	void (*release)(simple_http_client* http);
};

simple_http_client* create_simple_http(char* host, int port, char* url, char* method, http_res_cb cb, void* user_data, bool encrypt);
#endif //  __SIMPLE_HTTP_H__
