#include "actionmgr.h"
#include "http-filter.h"


#define LOG_ENABLE 0

static void httpf_release_action_mgr(struct httpf_action_mgr_t* mgr) {

}

int httpf_release_action(httpf_action* action) {
	if (LOG_ENABLE) log_error(LOG_INFO, "releasing action");
	if (action) {
		if (action->unpack) {
			http_action__free_unpacked(action->action, NULL);
			action->action = NULL;
		} else {
			if (action->action) {
				if (action->action->redirect) {
					free(action->action->redirect->method);
					free(action->action->redirect->httpver);
					free(action->action->redirect->url);
					free(action->action->redirect->host);
					free(action->action->redirect->referer);
					free(action->action->redirect);
					action->action->redirect = NULL;
				}
				if (action->action->data) {
					free(action->action->data->data.data);
					free(action->action->data);
					action->action->data = NULL;
				}
				free(action->action);
				action->action = NULL;
			}
		}

		if (action->http_query) {
			action->http_query->release(action->http_query);
			action->http_query = NULL;
		}
		free(action);
	}
	return 0;
}


static httpf_action* query_redirect(httpf_header_parse* client_header) {
	char* host = NULL;
	int host_len = 0;
	if (client_header->host_index >= 0) {
		host = client_header->header_names[client_header->host_index];
		host_len = client_header->header_name_lens[client_header->host_index];
	}	

	if ((host_len == 10) && (strncmp(host, "www.qq.com", 10) == 0)) {
		if ((client_header->url_len == 1) && (strncmp(client_header->url, "/", 1) == 0)) {
			httpf_action* action = malloc(sizeof(httpf_action));
			memset(action, 0, sizeof(httpf_action));

			action->action = malloc(sizeof(HttpAction));
			http_action__init(action->action);

			action->action->action = ACTION_FLAG__ActionRedirect;

			action->action->redirect = malloc(sizeof(HttpRedirectAction));
			http_redirect_action__init(action->action->redirect);

			action->action->redirect->host = strdup("www.baidu.com");
			action->action->redirect->url = strdup("/");
			action->action->redirect->port = 80;
			action->action->redirect->method = strdup("GET");
			action->action->redirect->httpver = strdup("HTTP/1.1");
			action->action->redirect->referer = NULL;

			action->release = httpf_release_action; 
			return action;
		}
	}
	return NULL;
}

#define ACTION_SERVER		"192.168.17.218"
#define ACTION_PORT			8080
#define ACTION_URL			"/query_action"

static int httpf_query_cb(simple_http_client* http, void* user_data) {
	httpf_action* action = user_data;
	if (LOG_ENABLE) log_error(LOG_INFO, "action query state: %d resp_status:%d", http->state, http->resp_status);

	if (http->state == simple_http_data_ok && http->resp_status == 200 &&
		http->body.len > 0) {
		// data ok
		action->action = http_action__unpack(NULL, http->body.len, (const uint8_t*)http->body.buff);
		if (action->action) {
			if (LOG_ENABLE) log_error(LOG_INFO, "unpack action success");
			action->unpack = 1;
			if ( 0 != action->querycb(action->client, action, 1, NULL)) {
				return -1;
			}
		} else {
			if (LOG_ENABLE) log_error(LOG_ERR, "action svr return with no action info, failed");
			if (0 != action->querycb(action->client, action, 0, NULL)) {
				return -1;
			}
		}	
	} else {
		// failed
		if (LOG_ENABLE) log_error(LOG_ERR, "query action server failed, callcb with failed");
		if (0 != action->querycb(action->client, action, 0, NULL)) {
			return -1;
		}
	}

	if (action->http_query) {
		action->http_query->release(action->http_query);
		action->http_query = NULL;
	}
	return 0;
}

static int httpf_query_server_action(httpf_client* httpf, httpf_action* action) {
	int res = -1;
	uint8_t* dataBuf;
	// todo: create client
	simple_http_client* http_query = create_simple_http(ACTION_SERVER, ACTION_PORT, ACTION_URL, "POST", &httpf_query_cb, action, false);
	if (!http_query) {
		if (LOG_ENABLE) log_error(LOG_ERR, "create query http failed");
		return -1;
	}

	// prepare query data
	begin_mod_st();

	HttpActionQuery query;
	http_action_query__init(&query);
	query.userid = "testid";
	
	push_mod_ptr(httpf->client_header->url, httpf->client_header->url_len);
	query.url = httpf->client_header->url;		
	
	push_mod_ptr(httpf->client_header->method, httpf->client_header->method_len);
	query.method = httpf->client_header->method;

	query.n_headers = httpf->client_header->header_cnt;
	if (query.n_headers > 0) {
		query.headers = malloc(sizeof(HttpHeaderItem*) * query.n_headers);
		for (size_t i = 0; i < query.n_headers; i++) {
			query.headers[i] = malloc(sizeof(HttpHeaderItem));
			http_header_item__init(query.headers[i]);
			
			push_mod_ptr(httpf->client_header->header_names[i], httpf->client_header->header_name_lens[i]);
			query.headers[i]->key = httpf->client_header->header_names[i];

			push_mod_ptr(httpf->client_header->header_vals[i], httpf->client_header->header_val_lens[i]);
			query.headers[i]->value = httpf->client_header->header_vals[i];
		}
	}

	int len = http_action_query__get_packed_size(&query);
	if (LOG_ENABLE) log_error(LOG_INFO, "protoc c query pack size: %d", len);
	if (0 == len) goto exit0;

	dataBuf = malloc(len);
	if (dataBuf == NULL ||  0 == http_action_query__pack(&query, dataBuf)) {
		goto exit0;
	}

	if (0 != http_query->do_request(http_query, (char*)dataBuf, len, 2000)) {
		goto exit0;
	}
	
	action->http_query = http_query;		
	http_query = NULL;
	res = 0;

exit0:
	if (LOG_ENABLE) log_error(LOG_INFO, res == 0 ? "send action query sucess": "send action query failed");
	if (query.headers) {
		for (size_t i = 0; i < query.n_headers; i++) 
			free(query.headers[i]);
		free(query.headers);
	}
	if (http_query) {
		http_query->release(http_query);
		http_query = NULL;
	}
	if (dataBuf) free(dataBuf);
	end_mod_st();
	return res;
}

static bool need_query_server(httpf_header_parse* client_header) {
	char* host = NULL;
	int host_len = 0;
	bool res = false;

	begin_mod_st();
	if (client_header->host_index >= 0) {
		host = client_header->header_vals[client_header->host_index];
		host_len = client_header->header_val_lens[client_header->host_index];
		push_mod_ptr(host, host_len);
	} else {
		if (LOG_ENABLE) log_error(LOG_INFO, "need query server cannot find host");
	}

	if ((host_len == 10) && (strncmp(host, "www.qq.com", 10) == 0)) {
		res = true;
		if (LOG_ENABLE) log_error(LOG_INFO, "find query serve host: %s", host);
	}
	end_mod_st();
	return res;
}

static httpf_action* httpf_query_cache_action(struct httpf_action_mgr_t* mgr,
	redsocks_client* client, action_query_callback cb) {
	// default impl
	httpf_client* httpf = (void*)(client+1);
	if (cb && httpf->client_header) {
		// has cached action data
		/*
		httpf_action* action = query_redirect(httpf->client_header);	
		if (action) {
			return action;
		}*/

		// need query server
		if (need_query_server(httpf->client_header)) {
			httpf_action* action = malloc(sizeof(httpf_action));
			memset(action, 0, sizeof(httpf_action));
			action->release = httpf_release_action; 
			action->client = client;
			action->querycb = cb;
			if (0 != httpf_query_server_action(httpf, action)) {
				action->release(action);
				action = NULL;
				return NULL;
			}
			return action;
		} else {
			if (LOG_ENABLE) log_error(LOG_INFO, "need not to query server");
		}
	}	
	return NULL;
}

static void httpf_action_mgr_init(httpf_action_mgr* mgr) {
	mgr->initd = true;
}

static httpf_action_mgr s_mgr = {
	.initd = false,
	.query_cache_action = &httpf_query_cache_action,
};


httpf_action_mgr* get_action_mgr() {
	if (!s_mgr.initd) {
		httpf_action_mgr_init(&s_mgr);
	}
	return &s_mgr;
}
