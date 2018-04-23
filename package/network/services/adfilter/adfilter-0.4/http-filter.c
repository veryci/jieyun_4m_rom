/* redsocks - transparent TCP-to-proxy redirector
 * Copyright (C) 2007-2011 Leonid Evdokimov <leon@darkk.net.ru>
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 *
 * http-relay upstream module for redsocks
 */

#include "actionmgr.h"
#include "http-filter.h"
#include "libh3/scanner.h"
#include "businessmgr.h"
#include "adconfig.h"
#include "report.h"
#include "clientmgr.h"

extern const char *auth_request_header;
extern const char *auth_response_header;
extern struct event_base * s_event_base ;
extern struct evdns_base * s_dns_base ;

#define LOG_ENABLE 0	

static void httpf_connect_relay(redsocks_client *client);

static void httpf_client_init(redsocks_client *client)
{
	httpf_client *httpf = (void*)(client + 1);

	client->state = httpf_new;
	memset(httpf, 0, sizeof(*httpf));
	httpf_buffer_init(&httpf->client_buffer);
	httpf_buffer_init(&httpf->relay_buffer);
}

static void httpf_client_fini(redsocks_client *client){
	httpf_client *httpf = (void*)(client + 1);

	httpf_buffer_fini(&httpf->client_buffer);
	httpf_buffer_fini(&httpf->relay_buffer);

	if (httpf->direct) {
		bufferevent_free(httpf->direct);
		httpf->direct = NULL;
	}
	if (httpf->redirect) {
		bufferevent_free(httpf->redirect);
		httpf->redirect = NULL;
	}

	if (httpf->action) {
		if (httpf->action->release) {
			httpf->action->release(httpf->action);
		}
		httpf->action = NULL;
	}

	if (httpf->client_header) {
		httpf->client_header->release(httpf->client_header);
		httpf->client_header = NULL;
	}
}

static void httpf_instance_fini(redsocks_instance *instance)
{
	http_auth *auth = (void*)(instance + 1);
	free(auth->last_auth_query);
	auth->last_auth_query = NULL;
}

// drops client on failure
static int httpf_append_header(redsocks_client *client, char *line)
{
	httpf_client *httpf = (void*)(client + 1);

	if (httpf_buffer_append(&httpf->client_buffer, line, strlen(line)) != 0)
		return -1;
	if (httpf_buffer_append(&httpf->client_buffer, "\x0d\x0a", 2) != 0)
		return -1;
	return 0;
}

// This function is not reenterable
static char *fmt_http_host(struct sockaddr_in addr)
{
	static char host[] = "123.123.123.123:12345";
	if (ntohs(addr.sin_port) == 80)
		return inet_ntoa(addr.sin_addr);
	else {
		snprintf(host, sizeof(host),
				"%s:%u",
				inet_ntoa(addr.sin_addr),
				ntohs(addr.sin_port)
				);
		return host;
	}
}
/*
static int httpf_build_relay_http_firstline(redsocks_client *client)
{
	httpf_client *httpf = (void*)(client + 1);
	char *uri = NULL;
	char *host = httpf->has_host ? httpf->host : fmt_http_host(client->destaddr);

	assert(httpf->firstline);

	uri = strchr(httpf->firstline, ' ');
	if (uri)
		uri += 1; // one char further
	else {
		redsocks_log_error(client, LOG_NOTICE, "malformed request came");
		goto fail;
	}

	httpf_buffer nbuff;
	if (httpf_buffer_init(&nbuff) != 0) {
		redsocks_log_error(client, LOG_ERR, "httpf_buffer_init");
		goto fail;
	}

	// http scheme
	if (httpf_buffer_append(&nbuff, httpf->firstline, uri - httpf->firstline) != 0)
		goto addition_fail;
	// http org url
	if (httpf_buffer_append(&nbuff, "http://", 7) != 0)
		goto addition_fail;
	if (httpf_buffer_append(&nbuff, host, strlen(host)) != 0)
		goto addition_fail;
	if (httpf_buffer_append(&nbuff, uri, strlen(uri)) != 0)
		goto addition_fail;
	// \r\n
	if (httpf_buffer_append(&nbuff, "\x0d\x0a", 2) != 0)
		goto addition_fail;

	free(httpf->firstline);

	httpf->firstline = calloc(nbuff.len + 1, 1);
	strcpy(httpf->firstline, nbuff.buff);
	httpf_buffer_fini(&nbuff);
	return 0;

addition_fail:
	httpf_buffer_fini(&nbuff);
fail:
	redsocks_log_error(client, LOG_ERR, "httpf_toss_http_firstline");
	return -1;
}
*/

/**
 * This function returns a char pointer, which is the end of the request line.
 *    if ( iscrlf(p)) {

 * Return NULL if parse failed.
 */

static bool is_post(httpf_client* httpf) {
	return 0 == strncmp(httpf->client_header->method, "POST", 4);
}

static int httpf_send_replace_header(struct bufferevent *bev, redsocks_client* client, httpf_client* httpf) {
	int res = -1;
	HttpAction* action = httpf->action->action;
	if (!action->data) return -1;
	HttpDataAction* data = action->data;
	if (0 != bufferevent_write(bev, data->data.data, data->data.len)) {
		redsocks_log_error(client, LOG_ERR, "write direct header failed");
		goto exit0;
	}

	if (LOG_ENABLE) redsocks_log_error(client, LOG_INFO, "replace request: \n%s", data->data.data);
	res = 0;
exit0:
	return res;
}


static int httpf_send_direct_header(struct bufferevent *bev, redsocks_client* client, httpf_client* httpf) {
	int res = -1;
	httpf_buffer header;
	httpf_header_parse* client_header = httpf->client_header;

	httpf_buffer_init(&header);

	// build http request line
	append_buffer(&header, client_header->method, client_header->method_len);
	append_buffer(&header, " ", 1);
	append_buffer(&header, client_header->url, client_header->url_len);
	append_buffer(&header, " ", 1);
	append_buffer(&header, client_header->ver, client_header->ver_len ? client_header->ver_len: strlen(client_header->ver));
	append_buffer(&header, "\r\n", 2);


	// copy other header info
	for (int i = 0; i < client_header->header_cnt; i++) {
		char* name = client_header->header_names[i];
		int name_len = client_header->header_name_lens[i];
		char* val = client_header->header_vals[i];
		int val_len = client_header->header_val_lens[i];

		/*
		* todo: dosomething with the header val	
		if (strncmp(name, "Referer", 7) == 0) {
		}*/

		if (httpf->force_connection_close && (strncmp(name, "Connection", 10) == 0))
			continue;

		// copy header info
		append_buffer(&header, name, name_len);
		append_buffer(&header, ": ", 2);
		append_buffer(&header, val, val_len);
		append_buffer(&header, "\r\n", 2);
	}

	if (httpf->force_connection_close) {
		append_buffer(&header, "Connection: close\r\n", 19);
	}

	append_buffer(&header, "\r\n", 2);

	if (0 != bufferevent_write(bev, header.buff, header.len)) {
		redsocks_log_error(client, LOG_ERR, "write direct header failed");
		goto exit0;	
	}

	if (LOG_ENABLE)redsocks_log_error(client, LOG_INFO, "direct request: \n%s", header.buff);
	res = 0;

exit0:
	httpf_buffer_fini(&header);
	return res;
}

static int httpf_send_redirect_header(struct bufferevent *bev, redsocks_client* client, httpf_client* httpf) {
	int res = -1;
	httpf_buffer header;
	HttpRedirectAction* red = httpf->action->action->redirect;

	httpf_header_parse* client_header = httpf->client_header;

	httpf_buffer_init(&header);

	// build http request line
	append_buffer(&header, red->method, strlen(red->method));
	append_buffer(&header, " ", 1);
	append_buffer(&header, red->url, strlen(red->url));
	append_buffer(&header, " ", 1);
	append_buffer(&header, red->httpver, strlen(red->httpver));
	append_buffer(&header, "\r\n", 2);

	// host
	append_buffer(&header, "Host: ", 6);
	append_buffer(&header, red->host, strlen(red->host));
	append_buffer(&header, "\r\n", 2);

	// copy other header info
	for (int i = 0; i < client_header->header_cnt; i++) {
		char* name = client_header->header_names[i];
		int name_len = client_header->header_name_lens[i];
		char* val = client_header->header_vals[i];
		int val_len = client_header->header_val_lens[i];

		if (strncmp(name, "Host", 4) == 0) {
			continue;
		} else if (strncmp(name, "Referer", 7) == 0) {
			if (red->referer) {
				append_buffer(&header, "Referer: ", 9);
				append_buffer(&header, red->referer, strlen(red->referer));
				append_buffer(&header, "\r\n", 2);
				continue;
			}
		}
		
		// copy header info
		append_buffer(&header, name, name_len);
		append_buffer(&header, ": ", 2);
		append_buffer(&header, val, val_len);
		append_buffer(&header, "\r\n", 2);
	}

	append_buffer(&header, "\r\n", 2);

	if (0 != bufferevent_write(bev, header.buff, header.len)) {
		redsocks_log_error(client, LOG_ERR, "write redirect header failed");
		goto exit0;	
	}

	if (LOG_ENABLE)redsocks_log_error(client, LOG_INFO, "redirect request: \n%s", header.buff);
	res = 0;

exit0:
	httpf_buffer_fini(&header);
	return res;
}

static int httpf_send_response(redsocks_client* client, httpf_response_parse* resp) {
	httpf_client *httpf = (void*)(client + 1);
	int res = -1;
	httpf_buffer header;
	httpf_buffer_init(&header);

	// build http request line
	append_buffer(&header, resp->ver, resp->ver_len);
	append_buffer(&header, " ", 1);
	append_buffer(&header, resp->status, resp->status_len);
	append_buffer(&header, " ", 1);
	append_buffer(&header, resp->reason, resp->reason_len);
	append_buffer(&header, "\r\n", 2);

	// copy other header info
	for (int i = 0; i < resp->header_cnt; i++) {
		char* name = resp->header_names[i];
		int name_len = resp->header_name_lens[i];
		char* val = resp->header_vals[i];
		int val_len = resp->header_val_lens[i];

		/*
		* todo: dosomething with the header val	
		if (strncmp(name, "Referer", 7) == 0) {
		}*/
		
		if (httpf->force_connection_close && (strncmp(name, "Connection", 10) == 0))
			continue;

		// copy header info
		append_buffer(&header, name, name_len);
		append_buffer(&header, ": ", 2);
		append_buffer(&header, val, val_len);
		append_buffer(&header, "\r\n", 2);
	}

	if (httpf->force_connection_close) {
		append_buffer(&header, "Connection: close\r\n", 19);
	}

	append_buffer(&header, "\r\n", 2);

	if (0 != bufferevent_write(client->client, header.buff, header.len)) {
		redsocks_log_error(client, LOG_ERR, "write direct header failed");
		goto exit0;	
	}

	if (LOG_ENABLE)redsocks_log_error(client, LOG_INFO, "response to client: \n%s", header.buff);
	res = 0;

exit0:
	httpf_buffer_fini(&header);
	return res;
}

static int httpf_filter_response(struct bufferevent* bev, redsocks_client* client) {
	int res = 0;
	// filter the return header
	httpf_response_parse* parse = malloc(sizeof(httpf_response_parse));
	if (!parse) return res;
	memset(parse, 0, sizeof(httpf_response_parse));
	int nRes = httpf_read_response_header(bev, parse);
	if (nRes > 0) {
		// read header success, write to 
		if (0 != httpf_send_response(client, parse)) {
			res = -1;
			goto exit0;
		}	
		evbuffer_drain(bev->input, parse->header_len);
	} else {
		redsocks_log_error(client, LOG_ERR, "parse response failed: %d", nRes);
	}

exit0:
	free(parse);
	return res;
}

static void htttpf_connect_read_cb(struct bufferevent* bev, void* user_data) {
	// just process the returned header
	UNUSED(bev);
	redsocks_client *client = user_data;
	httpf_client *httpf = (void*)(client + 1);
	if (LOG_ENABLE) redsocks_log_error(client, LOG_INFO, "connect read cb");
	if (httpf->direct) {
		// should not be here unless we want to filter the returnd header by server	
		if (client->state == httpf_direct_request_sent) {
			if (httpf->need_filter_header) {
				// write the heade to client's output buffer
				if (-1 == httpf_filter_response(bev, client)) {
					redsocks_log_error(client, LOG_ERR, "filter response");
					redsocks_drop_client(client);
					return;
				}	
			}
			// start relay
			client->state = httpf_relayed;
			client->relay = httpf->direct;
			httpf->direct = NULL;
			if (LOG_ENABLE) redsocks_log_error(client, LOG_INFO, "relaying client from direct data arrive");
			if (0 == redsocks_start_relay_ex(client)) {
				if (client->relay) 
					client->relay->writecb(bev, user_data);
				if (client->client)
					client->client->readcb(client->client, client);
			}
			return;
		}
	} else if (httpf->redirect) {
		// write the header	
		if (client->state == httpf_redirect_request_sent) {
			if (httpf->need_filter_header) {
				// write the heade to client's output buffer
				if (-1 == httpf_filter_response(bev, client)) {
					redsocks_log_error(client, LOG_ERR, "filter response");
					redsocks_drop_client(client);
					return;
				}	
			}
			// start relay
			client->state = httpf_relayed;
			client->relay = httpf->redirect;
			httpf->redirect = NULL;
			if (LOG_ENABLE) redsocks_log_error(client, LOG_INFO, "relaying client from redirect data arrive");
			if (0 == redsocks_start_relay_ex(client)) {
				if (client->relay) 
					client->relay->writecb(bev, user_data);
				if (client->client)
					client->client->readcb(client->client, client);
			}
			return;
		}
	}
	redsocks_log_error(client, LOG_ERR, "direct process error");
	redsocks_drop_client(client);
}

static void htttpf_connect_write_cb(struct bufferevent* bev, void* user_data) {
	// just process at the first call, this indicate the connection ok
	redsocks_client *client = user_data;
	httpf_client *httpf = (void*)(client + 1);

	if (LOG_ENABLE) redsocks_log_error(client, LOG_INFO, "htttpf_connect_write_cb direct");

	if (httpf->direct) {
		// connect ok
		if (client->state == httpf_direct_connect_sent) {
			if (!httpf->need_filter_header) {
				client->state = httpf_relayed;
				client->relay = httpf->direct;
				httpf->direct = NULL;
				// note: here not need to drain the client's input buffer
				if (0 == redsocks_start_relay_ex(client)) {
					if (client->relay) 
						client->relay->writecb(bev, user_data);
					if (client->client)
						client->client->readcb(client->client, client);
				}
				return;
			} else {
				// we want to filter the returned data
				// build the request header and send, than change the state	
				httpf_action* action = httpf->action;
				if (action && action->action && action->action->action == ACTION_FLAG__ActionReplaceRequest) {
					if (httpf_send_replace_header(bev, client, httpf) != 0){ 
						redsocks_log_error(client, LOG_ERR, "send replace header failed");
						redsocks_drop_client(client);
						return;
					}	
				}
				else if (httpf_send_direct_header(bev, client, httpf) != 0) {
					redsocks_log_error(client, LOG_ERR, "send direct header failed");
					redsocks_drop_client(client);
					return;
				}	

				// and enalbe the connection to read
				httpf->direct->readcb = htttpf_connect_read_cb;
				bufferevent_enable(httpf->direct, EV_READ | EV_WRITE);

				// drain the client's input buffer for header size;
				if (-1 == evbuffer_drain(client->client->input, httpf->client_header->header_len)) {
					redsocks_log_error(client, LOG_ERR, "drain client buffer failed");
					redsocks_drop_client(client);
					return;
				}
			
				//change state, and wait for data arrive
				client->state = httpf_direct_request_sent;
				if (LOG_ENABLE) redsocks_log_error(client, LOG_INFO, "send to redirect ok");
	
				// if post we need relay the post data
				if (is_post(httpf)) {
					bufferevent_enable(client->client, EV_READ);
					if (0 != bufferevent_write_buffer(httpf->direct, bufferevent_get_input(client->client))) {
						redsocks_log_error(client, LOG_ERR, "write client input to server for post data failed");
						redsocks_drop_client(client);
						return;
					}
				}	
			}
		}
	} else if (httpf->redirect) {
		if (client->state == httpf_redirect_connect_sent) {		
			// build the request header and send, than change the state	
			if (httpf_send_redirect_header(bev, client, httpf) != 0) {
				redsocks_log_error(client, LOG_ERR, "send redirect header failed");
				redsocks_drop_client(client);
				return;
			}	
			// and enalbe the connection to read
			httpf->redirect->readcb = htttpf_connect_read_cb;
			bufferevent_enable(httpf->redirect, EV_READ | EV_WRITE);

			// drain the client's input buffer for header size;
			if (-1 == evbuffer_drain(client->client->input, httpf->client_header->header_len)) {
				redsocks_log_error(client, LOG_ERR, "drain client buffer failed");
				redsocks_drop_client(client);
				return;
			}
			
			// change state, and wait for data arrive
			client->state = httpf_redirect_request_sent;
			if (LOG_ENABLE) redsocks_log_error(client, LOG_INFO, "send to redirect ok");

			if (is_post(httpf)) {
				// must enable read for client
				bufferevent_enable(client->client, EV_READ);
				if (0 != bufferevent_write_buffer(httpf->redirect, bufferevent_get_input(client->client))) {
					redsocks_log_error(client, LOG_ERR, "write client input to server for post data failed");
					redsocks_drop_client(client);
					return;
				}
			}
		}
	}
}

static bool is_request_send(redsocks_client* client) {
	if (client->state == httpf_direct_request_sent) {
		return true;
	}
	if (client->state == httpf_redirect_request_sent) {
		return true;
	}
	return false;
}

static void htttpf_connect_event_cb(struct bufferevent* bev, short evt, void* user_data) {
	UNUSED(bev);
	redsocks_client *client = user_data;
	httpf_client* httpf = (void*)(client + 1);
	if (evt & BEV_EVENT_CONNECTED) {
		// todo: continue to read and write
		if (LOG_ENABLE) redsocks_log_error(client, LOG_INFO, "httpf_connected success");
		bufferevent_enable(bev,  EV_WRITE);
	} else {
		// todo: other error
		// the server drop the connection
		// ignore the post after send request to the server
		if (!(is_post(httpf) && is_request_send(client))) { 
			redsocks_log_error(client, LOG_ERR, "httpf connect failed, evt: %d state: %d client request: \n%s", evt, client->state, httpf->client_header->data);
			redsocks_log_error(client, LOG_ERR, "is post: %d is request_send: %d", (int)is_post(httpf), (int)is_request_send(client));
		}
		redsocks_drop_client(client);
	}
}

static int httpf_direct_connect(redsocks_client *client) {
	// direct connect to destaddr and copy data between client and server
	httpf_client* httpf = (void*)(client+1);
	struct bufferevent* direct = bufferevent_socket_new(s_event_base, -1, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);	
	if (!direct) {
		redsocks_drop_client(client);
		return -1;
	}
	bufferevent_setcb(direct, NULL, &htttpf_connect_write_cb, &htttpf_connect_event_cb, client);
	bufferevent_enable(direct, EV_WRITE);

	if (0 != bufferevent_socket_connect(direct, (struct sockaddr*)(&(client->destaddr)), sizeof(struct sockaddr_in))) {
		redsocks_log_error(client, LOG_ERR, "connect direct to destaddr failed");
		bufferevent_free(direct);
		redsocks_drop_client(client);
		return -1;			
	}
	httpf->direct = direct;
	client->state = httpf_direct_connect_sent;
	return 0;
}

static int httpf_redirect_connect(redsocks_client *client, HttpRedirectAction* red) {
	httpf_client* httpf = (void*)(client+1);
	struct bufferevent* redirect = bufferevent_socket_new(s_event_base, -1, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);	
	if (!redirect) {
		redsocks_drop_client(client);
		return -1;
	}
	bufferevent_setcb(redirect, NULL, &htttpf_connect_write_cb, &htttpf_connect_event_cb, client);
	bufferevent_enable(redirect, EV_WRITE);

	if (LOG_ENABLE) redsocks_log_error(client, LOG_INFO, "connecting to :%s:%d", red->host, (int)red->port);
	if (0 != bufferevent_socket_connect_hostname(redirect, s_dns_base, AF_INET, red->host, (int)red->port)) {
		redsocks_log_error(client, LOG_ERR, "connect redirect to %s:%d failed dns: %s", red->host, (int)red->port, evutil_gai_strerror(bufferevent_socket_get_dns_error(redirect)));
		
		bufferevent_free(redirect);
		redsocks_drop_client(client);
		return -1;			
	}

	if (LOG_ENABLE) redsocks_log_error(client, LOG_INFO, "connect hostname ok");
	httpf->redirect = redirect;
	client->state = httpf_redirect_connect_sent;
	return 0;
}

static void htttpf_data_write_cb(struct bufferevent* bev, void* user_data) {
	redsocks_client *client = user_data;
	httpf_client *httpf = (void*)(client + 1);
	UNUSED(httpf);

	if (evbuffer_get_length(bufferevent_get_output(bev)) == 0) {
		redsocks_drop_client(client);
	}
}

static int httpf_return_data(redsocks_client* client,  HttpDataAction* data) {
	// todo: write data to client's output buffer ,and wait for write again to close it

	if (LOG_ENABLE) log_error(LOG_INFO, "writing data direct to client:\n%s", (char*)data->data.data);

	if (0 != bufferevent_write(client->client, data->data.data, data->data.len)) {
		redsocks_log_error(client, LOG_ERR, "write data failed");	
		redsocks_drop_client(client);
		return -1;
	}
	
	client->state = httpf_data_returned;
	client->client->writecb = htttpf_data_write_cb;
	bufferevent_enable(client->client, EV_READ | EV_WRITE);	
	return 0;
}

static int httpf_action_call_cb(redsocks_client *client, httpf_action *action, int ok, httpf_action* new_action) {
	client->state = httpf_action_came;
	httpf_client* httpf = (void*)(client+1);
	httpf->action = action;

	// query action failed
	if (!ok) {
		return httpf_direct_connect(client);
	} 

	if (new_action) {
		httpf->action->release(httpf->action);
		httpf->action = new_action;
		action = new_action;
	}

	// todo
	httpf->need_filter_header = 1;
	HttpAction* at = action->action;

	if (LOG_ENABLE) redsocks_log_error(client, LOG_INFO, "action call back action: %d", (int)action->action->action);

	switch (at->action) {
		case ACTION_FLAG__ActionNone: {
			return httpf_direct_connect(client);
		}
		case ACTION_FLAG__ActionRedirect: {
			return httpf_redirect_connect(client, at->redirect);
		}
		case ACTION_FLAG__ActionData: {
			return httpf_return_data(client, at->data);
		}
		case ACTION_FLAG__ActionReplaceRequest: {
			return httpf_direct_connect(client);
		} 
	}

	return -1;
}

static int httpf_process_filter(redsocks_client *client, httpf_client *httpf) {
	// todo: query cache mgr for 
	// httpf_action_mgr* mgr = get_action_mgr();
	http_bzmgr* mgr = get_business_mgr();
	int hasHostRule = 0;

	if (mgr) {
		httpf_action* action = NULL; // mgr->query_cache_action(mgr, client, &httpf_action_call_cb);
		action = mgr->find_target(mgr, client, &httpf_action_call_cb, &hasHostRule);

		if (action == NULL) {
			// if we want to filter a host, the keep-alive connection header is harmful
			if (hasHostRule) {
				if (LOG_ENABLE) log_error(LOG_INFO, "cant find rule but has hostrule, will use force close");
				httpf->need_filter_header = 1;
				httpf->force_connection_close = 1;
			}
			return httpf_direct_connect(client);
		}
		else if (action->action == NULL) {
			// we need wait for action query, and wait for cb return
			client->state = httpf_action_request_sent;
			httpf->action = action;
			return 0;
		} else {
			// it must has call back
			if (0 != httpf_action_call_cb(client, action, 1, NULL)) {	
				redsocks_log_error(client, LOG_ERR, "action mgr process failed");
				redsocks_drop_client(client);
				return -1;
			}			
			return 0;
		}
	} else {
		return httpf_direct_connect(client);	
	}
	return -1;
}

static int httpf_init_client_header(httpf_client* httpf) {
	if (!httpf->client_header) {
		httpf->client_header = create_http_header_parse();
		if (!httpf->client_header) {
			return -1;
		}
	}
	return 0;
}

static void collect_ua_info(httpf_client* httpf, redsocks_client* client) {
	// collect ua
	client_mgr* mgr = get_client_mgr();
	if (mgr) {
		int uaid = httpf->client_header->get_header(httpf->client_header, "User-Agent");
		if (uaid != -1) {
			char* ua = httpf->client_header->header_vals[uaid];
			int ua_len = httpf->client_header->header_val_lens[uaid];
			int hi = httpf->client_header->host_index;
			char* host = NULL;
			int host_len = 0;
			if (hi != -1) {
				host = httpf->client_header->header_vals[hi];
				host_len = httpf->client_header->header_val_lens[hi];
			}
			char* url = httpf->client_header->url;
			int url_len = httpf->client_header->url_len;
			mgr->add_ua(mgr, client->clientaddr, ua, ua_len, host, host_len, url, url_len);
		}
	}
}

static void httpf_client_read_cb(struct bufferevent *buffev, void *_arg)
{
	redsocks_client *client = _arg;
	httpf_client *httpf = (void*)(client + 1);
	redsocks_touch_client(client);

	switch (client->state) {
		case httpf_new: {
			// todo: read headers info
			if (0 != httpf_init_client_header(httpf)) {
				redsocks_log_error(client, LOG_ERR, "init header failed");
				redsocks_drop_client(client);
				return;
			}	
			// try parse header
			// int nRes = httpf_read_httpheader(buffev, httpf->client_header);
			int nRes = httpf->client_header->read_header(httpf->client_header, buffev);
			if (nRes < 0) {
				// not a http request
				redsocks_log_error(client, LOG_ERR, "parse http heade failed: %d direct connectiong ", nRes);
				if (0 != httpf_direct_connect(client)) {
					return;
				}
				
				// we need no more client data now
				bufferevent_disable(client->client, EV_READ);
			} else if (nRes == 0) {
				// is a http request, but need more http header data, just wait later;
				return;
			} else {
				// http data is is parsed success, will query action 
				// dump_httpheader(httpf->client_header);

				client->state = httpf_recv_request_headers;
				// collect_ua_info(httpf, client);

				if ( 0 != httpf_process_filter(client, httpf)) {
					return;
				}
				
				// if is get request, we need more data after the server response
				bufferevent_disable(client->client, EV_READ);
			}
			break;
		}
		default: {
			if (is_post(httpf)) {
				if (client->state == httpf_direct_request_sent && httpf->direct) {
					if (0 != bufferevent_write_buffer(httpf->direct, bufferevent_get_input(client->client))) {
						redsocks_log_error(client, LOG_ERR, "write client input to server for post data failed");
						redsocks_drop_client(client);
						return;
					}

				} else if (client->state == httpf_redirect_request_sent && httpf->redirect) {
					if (0 != bufferevent_write_buffer(httpf->redirect, bufferevent_get_input(client->client))) {
						redsocks_log_error(client, LOG_ERR, "write client input to server for post data failed");
						redsocks_drop_client(client);
						return;
					}
					break;
				}
			}
			// should not be here
			redsocks_log_error(client, LOG_ERR, "httpf client read state should not be here, state:%d", client->state);
			redsocks_drop_client(client);
			break;
		}
	}		
}

static void httpf_connect_hook(redsocks_client *client)
{
	if (LOG_ENABLE) log_error(LOG_INFO, "client connect");
	struct sockaddr_in* bindaddr = &(client->instance->config.bindaddr);
	if ((client->destaddr.sin_addr.s_addr == bindaddr->sin_addr.s_addr) &&
		(client->destaddr.sin_port == bindaddr->sin_port)) {
		if (LOG_ENABLE) log_error(LOG_INFO, "find direct connet to bind addr");
		if (!adfilter_installed_oneday_before()) {
			if (LOG_ENABLE) log_error(LOG_INFO, "adfilter installed in one day saying hello to client");
			bufferevent_write(client->client, ADFILTER_HELLO, strlen(ADFILTER_HELLO));
		}
		redsocks_drop_client(client);
	} else {
		int error;
		client->client->readcb = httpf_client_read_cb;
		error = bufferevent_enable(client->client, EV_READ);
		if (error) {
			redsocks_log_errno(client, LOG_ERR, "bufferevent_enable");
			redsocks_drop_client(client);
		}
	}
}

static void httpf_instance_init(struct redsocks_instance_t* instance) {
	get_business_mgr();
	report_start_up();
	get_client_mgr();	
}

relay_subsys http_filter_subsys =
{
	.name                 = "http-filter",
	.payload_len          = sizeof(httpf_client),
	.instance_payload_len = sizeof(http_auth),
	.init                 = httpf_client_init,
	.fini                 = httpf_client_fini,
	.connect_relay        = httpf_connect_hook,

	.instance_init		  = httpf_instance_init,
	.instance_fini        = httpf_instance_fini,
};

/* vim:set tabstop=4 softtabstop=4 shiftwidth=4: */
/* vim:set foldmethod=marker foldlevel=32 foldmarker={,}: */
