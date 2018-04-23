#include "simple-http.h"
#include "openssl/rc4.h"

#define LOG_ENABLE 0

extern struct event_base * s_event_base;
extern struct evdns_base * s_dns_base;

int binary_data_to_hex_string( unsigned char* data, size_t len, httpf_buffer* buf){
  	int i = 0;
  	int new_buf_len =  2 * len + 1;
	if (buf->max_len < new_buf_len) {
		return -1;
	}
	char* hex_content = buf->buff + buf->len;
  	char* format_buf = hex_content;

	for( i=0; i<(int)len; i++ ){
   		sprintf( format_buf, "%02X", *(data+i) );
    	format_buf += 2;
  	}
	buf->len += format_buf - hex_content;
	return 0;
}

long get_rand_range( long imin, long imax ){
  return (long)(rand() % ( imax - imin + 1)+ imin);
}

void gen_rc4_key_with_seed( unsigned long seed, const char* rc4_key, httpf_buffer* key ){
  	char secs[256];
  	memset( secs, 0, sizeof(secs) );
  	sprintf( secs, "%s-%lu", rc4_key, seed);
	httpf_buffer_append(key, secs, strlen(secs) + 1);
}

#define RC4_KEY_URI	       ("7th~f$Y6nLsmkTi$z#LdBhH@f~W#NiN3")
#define RC4_KEY_POSTBODY	 ("wlJr=~t-8h.pIuS%RmaA+L=mJ+OrXHzJ")

static bool rc4_crypt(char* rc4key, char* data, int len) {
	RC4_KEY	key;	
	RC4_set_key(&key, strlen(rc4key), (const unsigned char*)rc4key); 
	RC4(&key, len, (const unsigned char*)data, (unsigned char*)data); 
	return true;	
}

/////////////////////////////////////////////////////////////////////////////////////////////////

static int simple_http_add_header(simple_http_client* http, char* key, char* val) {
	int res = -1;
	append_buffer(&http->out, key, strlen(key));
	append_buffer(&http->out, ": ", 2);
	append_buffer(&http->out, val, strlen(val));
	append_buffer(&http->out, "\r\n", 2);
	res = 0;
exit0:
	return res;
}

static void simple_http_on_failed(simple_http_client* http) {
	if (LOG_ENABLE) log_error(LOG_INFO, "simple http on failed");
	http->state = simple_http_conn_failed;
	if (http->conn) {
		bufferevent_free(http->conn);
		http->conn = NULL;
	}
	if (http->timeout) {
		evtimer_del(http->timeout);
		event_free(http->timeout);
		http->timeout = NULL;
	}

	if (http->encrypt) {
		if (http->body.len > 0) {
			rc4_crypt(http->data_key.buff, http->body.buff, http->body.len);
		}
	}

	http->rescb(http, http->rescb_userdata);
}

static void simple_http_on_success(simple_http_client* http) {
	if (LOG_ENABLE) log_error(LOG_INFO, "simple http on success");
	http->state = simple_http_data_ok;
	if (http->conn) {
		bufferevent_free(http->conn);
		http->conn = NULL;
	}
	if (http->timeout) {
		evtimer_del(http->timeout);
		event_free(http->timeout);
		http->timeout = NULL;
	}
	if (http->encrypt) {
		if (http->body.len > 0) {
			rc4_crypt(http->data_key.buff, http->body.buff, http->body.len);
		}
	}
	http->rescb(http, http->rescb_userdata);
}

static void simple_connect_read_cb(struct bufferevent* bev, void* user_data) {
	simple_http_client* http = user_data;
	if (LOG_ENABLE) log_error(LOG_INFO, "simple http data reciving");

	if (http->state == simple_http_request_sent) {
		if (!http->resp_header) {
			http->resp_header = create_http_response_parse();
		}
		int nRes = httpf_read_response_header(bev, http->resp_header);
		if (nRes < 0) {
			if (LOG_ENABLE) log_error(LOG_ERR, "simple http parse header failed");
			goto exit0;
		} else if (nRes == 0) {
			// wait more header data
			return;
		} else {
			// context len, drain data, change state
			if (http->resp_header->status) {
				sscanf(http->resp_header->status, "%d", &http->resp_status);
				if (http->resp_status <= 0) {
					if (LOG_ENABLE) log_error(LOG_ERR, "simple http header status error: %d", http->resp_status);
					goto exit0;
				}
			} else {
				if (LOG_ENABLE) log_error(LOG_ERR, "simple http header has no status");
				goto exit0;
			}

			// content len
			char* context_len = http->resp_header->get_header(http->resp_header, "Content-Length");
			if (!context_len) {
				if (LOG_ENABLE) log_error(LOG_ERR, "simple http header has no context len");
			} else {
				sscanf(context_len, "%d", &http->content_len);
				if (http->content_len <= 0) {
					if (LOG_ENABLE) log_error(LOG_ERR, "simple http header has no context len 2");
					goto exit0;
				}
			}

			// drain buffer 
			if (0 != evbuffer_drain(bev->input, http->resp_header->header_len)) {
				if (LOG_ENABLE) log_error(LOG_ERR, "simple http evbuffer err");
				goto exit0;
			}
			
			http->state = simple_http_data_recving;
		}
	} 

	if (http->state == simple_http_data_recving) {
		if (EVBUFFER_LENGTH(bev->input) > 0) {
			if (LOG_ENABLE) log_error(LOG_ERR, "simple http recv body len: %d", (int)(EVBUFFER_LENGTH(bev->input)));
			if (0 != httpf_buffer_append(&http->body, (const char*)EVBUFFER_DATA(bev->input), EVBUFFER_LENGTH(bev->input))) {
				if (LOG_ENABLE) log_error(LOG_ERR, "append body failed");
				goto exit0;
			}

			if (0 != evbuffer_drain(bev->input, EVBUFFER_LENGTH(bev->input))) {
				if (LOG_ENABLE) log_error(LOG_ERR, "simple http evbuffer err");
				goto exit0;
			}
		}
		
		// data recv ok
		if (http->content_len > 0) {
			if (http->body.len >= http->content_len) {
				simple_http_on_success(http);
				return;
			}
		}
	}
	return;

exit0:
	simple_http_on_failed(http);
}

static void print_send_data(simple_http_client* http) {
	begin_mod_st();
	push_mod_ptr(http->out.buff, http->out_header_len);
	if (LOG_ENABLE) log_error(LOG_INFO, "sending header: %s", http->out.buff);
	end_mod_st();

	if (LOG_ENABLE) log_error(LOG_INFO, "sending data:");
	char* pData = http->out.buff + http->out_header_len;
	int len = http->out.len - http->out_header_len;

	for (;len > 0;) {
		int nPrintLen = 16;
		if (len < nPrintLen) nPrintLen = len;
		char buf[256] = {0};
		for (int i = 0; i < nPrintLen; i++) {
			sprintf(buf + strlen(buf), "0x%02x ", (int)pData[i]);
		}
		// if (LOG_ENABLE) log_error(LOG_INFO, "%s", buf);
		len -= nPrintLen;
		pData += nPrintLen;
	}
}

static void simple_connect_write_cb(struct bufferevent* bev, void* user_data) {
	simple_http_client* http = user_data;
	if (http->state == simple_http_conn_sent) {
		// todo: print the sending data
		if(LOG_ENABLE) print_send_data(http);

		if (0 != bufferevent_write(bev, http->out.buff, http->out.len)) {
			if (LOG_ENABLE) log_error(LOG_ERR, "write buffer failed");
			simple_http_on_failed(http);
		}
		http->state = simple_http_request_sent;
		bev->readcb = simple_connect_read_cb;
		bufferevent_enable(bev, EV_READ | EV_WRITE);
	}
}

static void simple_connect_event_cb(struct bufferevent* bev, short what, void* user_data) {
	UNUSED(bev);
	simple_http_client* http = user_data;
	if (what & BEV_EVENT_CONNECTED) {
		// todo: continue to read and write
		if (LOG_ENABLE) log_error(LOG_INFO, "simple http connect success");
		bufferevent_enable(bev,  EV_WRITE);
	} else {
		// todo: other error
		if (http->state == simple_http_data_recving) {
			if (http->content_len == 0 && http->body.len > 0) {
				http->content_len = http->body.len;
				if (LOG_ENABLE) log_error(LOG_INFO, "simple http connect close by server success with len: %d", http->body.len);
				simple_http_on_success(http);
				return;
			}
		}

		if (LOG_ENABLE) log_error(LOG_ERR, "simple http connect to %s:%d failed dns: %s", http->host, (int)http->port, evutil_gai_strerror(bufferevent_socket_get_dns_error(bev)));
		
		if (LOG_ENABLE) log_error(LOG_ERR, "simple http connect failed, state: %d what:%d", http->state, (int)what);
		simple_http_on_failed(http);
	}
}

static void simple_http_on_timeout(evutil_socket_t fd, short what, void *user_data) {
	simple_http_client* http = user_data;
	if (http->state == simple_http_data_recving) {
		if (http->content_len == 0 && http->body.len > 0) {
			http->content_len = http->body.len;
			if (LOG_ENABLE) log_error(LOG_INFO, "simple http connect close by timeout success with len: %d", http->body.len);
			simple_http_on_success(http);
			return;
		}
	}
	if (LOG_ENABLE) log_error(LOG_ERR, "simple http time out");
	simple_http_on_failed(http);		
}

static int simple_http_do_request(simple_http_client* http, char* data, int size, int timeout_ms) {
	int res = -1;
	if (data && size) {
		if (0 != simple_http_add_header(http, "Content-Type", "	application/octet-stream")) goto exit0;
		char buf[20] = {0};
		sprintf(buf, "%d", size);
		if (0 != simple_http_add_header(http, "Content-Length", buf)) goto exit0;
	}

	if (0 != simple_http_add_header(http, "Connection", "Close")) goto exit0;

	// end of header
	append_buffer(&http->out, "\r\n", 2);
	
	http->out_header_len = http->out.len;

	// data
	int lastLen = http->out.len;
	append_buffer(&http->out, data, size);
	char* lastData = http->out.buff + lastLen;

	if (http->encrypt) {
		rc4_crypt(http->data_key.buff, lastData, size);
	}

	// build ev and connect	
	struct bufferevent* conn = bufferevent_socket_new(s_event_base, -1, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);	
	if (!conn) {
		goto exit0;
	}
	bufferevent_setcb(conn, NULL, &simple_connect_write_cb, &simple_connect_event_cb, http);
	bufferevent_enable(conn, EV_WRITE);

	if (LOG_ENABLE) log_error(LOG_INFO, "simple http connecting to :%s:%d", http->host, http->port);
	if (0 != bufferevent_socket_connect_hostname(conn, s_dns_base, AF_INET, http->host, (int)http->port)) {
		if (LOG_ENABLE) log_error(LOG_ERR, "simple http connect to %s:%d failed dns: %s", http->host, (int)http->port, evutil_gai_strerror(bufferevent_socket_get_dns_error(conn)));
		
		bufferevent_free(conn);
		goto exit0;			
	}

	http->conn = conn;
	http->state = simple_http_conn_sent;

	http->timeout = evtimer_new(s_event_base, &simple_http_on_timeout, http);
	struct timeval tv;
	tv.tv_sec = timeout_ms/1000;
	tv.tv_usec = (timeout_ms % 1000) * 1000;
	evtimer_add(http->timeout, &tv);

	res = 0;
exit0:
	return res;
}

static void simple_http_release(simple_http_client* http) {
	if (http) {
		if (http->conn) {
			bufferevent_free(http->conn);
			http->conn = NULL;
		}

		httpf_buffer_fini(&http->out);
		httpf_buffer_fini(&http->body);

		if (http->resp_header) {
			httpf_response_free(http->resp_header);
			http->resp_header = NULL;	
		}

		if (http->timeout) {
			evtimer_del(http->timeout);
			event_free(http->timeout);
			http->timeout = NULL;
		}
		httpf_buffer_fini(&http->url_key);
		httpf_buffer_fini(&http->data_key);
		httpf_buffer_fini(&http->encypt_url);
		free(http);
	}	
}

static int create_encrypt_key(simple_http_client* http) {
	struct timeval tv;
  	gettimeofday(&tv,NULL);
  	unsigned long uri_enc_seed  = 10000000 + (tv.tv_sec & 11111111) + tv.tv_usec + get_rand_range(1000000,9999999);
  	unsigned long data_enc_seed = 10000000 + (tv.tv_sec & 11111111) + tv.tv_usec + get_rand_range(1000000,9999999);

  	gen_rc4_key_with_seed( uri_enc_seed, RC4_KEY_URI, &http->url_key );
	gen_rc4_key_with_seed( data_enc_seed, RC4_KEY_POSTBODY, &http->data_key );
	if (LOG_ENABLE) log_error(LOG_INFO, "encrypt seed url %s", http->url_key.buff );
  	if (LOG_ENABLE) log_error(LOG_INFO, "encrypt seed data %s", http->data_key.buff);

	char buf[1024];
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "%s/%lu", http->url, data_enc_seed);
	int len = strlen(buf);
	rc4_crypt(http->url_key.buff, buf, len);

	char urlHead[1024];
	memset(urlHead, 0, sizeof(urlHead));
	sprintf(urlHead, "/xy/%lu", uri_enc_seed);
	httpf_buffer_append(&http->encypt_url, urlHead, strlen(urlHead));
  	bool res = (0 == binary_data_to_hex_string( (unsigned char*)buf, len, &http->encypt_url));
	if (LOG_ENABLE) log_error(LOG_INFO, "org url: %s encrypt url: %s", http->url, http->encypt_url.buff);
	return res; 
}

static int simple_http_prepare_header(simple_http_client* http) {
	int res = -1;
	if (http->encrypt) {
		if (!create_encrypt_key(http))
			return -1;
	}

	// request header
	append_buffer(&http->out, http->method, strlen(http->method));
	append_buffer(&http->out, " ", 1);
	if (http->encrypt) {
		append_buffer(&http->out, http->encypt_url.buff, http->encypt_url.len);
	} else {
		append_buffer(&http->out, http->url, strlen(http->url));
	}
	append_buffer(&http->out, " HTTP/1.1\r\n", 11);

	if (0 != simple_http_add_header(http, "Host", http->host)) goto exit0;
	if (0 != simple_http_add_header(http, "User-Agent", "Mozilla/5.0")) goto exit0;
	if (0 != simple_http_add_header(http, "Accept", "application/octet-stream")) goto exit0;
	if (0 != simple_http_add_header(http, "Accept-Encoding", "identity")) goto exit0;
	// accept-encoding
	res = 0;
exit0:
	return res;
}

simple_http_client* create_simple_http(char* host, int port, char* url, char* method, http_res_cb cb, void* user_data, bool encrypt) {
	simple_http_client *http = malloc(sizeof(simple_http_client));
	memset(http, 0, sizeof(simple_http_client));
	
	http->url = url;
	http->host = host;
	http->port = port;
	http->method = method;

	http->rescb = cb;
	http->rescb_userdata = user_data;

	http->state = simple_http_new;
	
	httpf_buffer_init(&http->out);
	httpf_buffer_init(&http->body);

	httpf_buffer_init(&http->url_key);
	httpf_buffer_init(&http->data_key);
	httpf_buffer_init(&http->encypt_url);

	http->encrypt = encrypt;

	http->add_header = &simple_http_add_header;
	http->do_request = &simple_http_do_request;
	http->release = &simple_http_release;

	if (0 != simple_http_prepare_header(http)) {
		if (LOG_ENABLE) log_error(LOG_ERR, "prepare request header failed");
		http->release(http);
		return NULL;
	}
	return http;
}
