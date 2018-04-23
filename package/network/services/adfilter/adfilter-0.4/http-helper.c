#include "http-helper.h"
#include "libh3/h3.h"
#include "libh3/scanner.h"

#define LOG_ENABLE 0 

int httpf_buffer_init(httpf_buffer *buff)
{
	buff->max_len = 4096;
	buff->len = 0;
	buff->buff = calloc(buff->max_len, 1);
	if (!buff->buff)
		return -1;
	return 0;
}

void httpf_buffer_fini(httpf_buffer *buff)
{
	free(buff->buff);
	buff->buff = NULL;
}

int httpf_buffer_append(httpf_buffer *buff, const char *data, int len)
{
	if (buff->len + len + 1 > buff->max_len) {
		while (buff->len + len + 1 > buff->max_len) {
			/* double the buffer size */
			buff->max_len *= 2;
		}
		char *new_buff = calloc(buff->max_len, 1);
		if (!new_buff) {
			return -1;
		}
		memset(new_buff, 0, buff->max_len);
		memcpy(new_buff, buff->buff, buff->len);
		memcpy(new_buff + buff->len, data, len);
		buff->len += len;
		new_buff[buff->len] = '\0';
		free(buff->buff);
		buff->buff = new_buff;
	} else {
		memcpy(buff->buff + buff->len, data, len);
		buff->len += len;
		buff->buff[buff->len] = '\0';
	}
	return 0;
}

static bool is_invalid_http_method(char* method, int len) {
	if ((len == 3) && (strncmp(method, "GET", len) == 0)) return true;
	if ((len == 4) && (strncmp(method, "POST", len) == 0)) return true;
	return false;
}

static bool is_invalid_http_version(char* method, int len) {
	if ((len == 8) && (strncmp(method, "HTTP/1.", 7) == 0)) return true;
	return false;
}

static char * request_line_parse(httpf_header_parse* parse) {
    // Parse the request-line
    // http://tools.ietf.org/html/rfc2616#section-5.1
    // Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
    char * p = parse->data;
	parse->method = p;

    while (notend(p) && ! isspace(*p) ) p++;

    if ( end(p) || iscrlf(p) ) {
        // set error
		if (LOG_ENABLE) log_error(LOG_INFO, "end for clrf without method");
        return NULL;
    }

	parse->method_len = p - parse->data;
	if (!is_invalid_http_method(parse->method, parse->method_len)) { 
		 char c = parse->method[parse->method_len];
		parse->method[parse->method_len] = 0;
		 if (LOG_ENABLE) log_error(LOG_INFO, "unsupport method: %s", parse->method);
		parse->method[parse->method_len] = c;
		return NULL;
	}

    // Skip space
    while (notend(p) && isspace(*p) && notcrlf(p) ) p++;
	if (end(p)) {
		if (LOG_ENABLE) log_error(LOG_INFO, "end after method");
		return NULL;
	}

    // parse RequestURI
	parse->url = p;
    while (!isspace(*p) && notcrlf(p) && notend(p) ) p++;
	parse->url_len = p - parse->url;

    // Skip space
    while (notend(p) && isspace(*p) && notcrlf(p) ) p++;
	if (end(p))  {	
		if (LOG_ENABLE) log_error(LOG_INFO, "end after url");
		return NULL;
	}

    if ( iscrlf(p)) {
		parse->ver = H3_DEFAULT_HTTP_VERSION;
		p += 2;
		return p;
    } else {
		parse->ver = p;
        while (notend(p) && !isspace(*p) && notcrlf(p) ) p++;
		parse->ver_len = p - parse->ver;
		if (!is_invalid_http_version(parse->ver, parse->ver_len)) {
			if (LOG_ENABLE) log_error(LOG_INFO, "invalied http ver");
			return NULL;
		}	
	
		// SKIP space
	    while (notend(p) && isspace(*p) && notcrlf(p) ) p++;
		if (end(p)) {
			if (LOG_ENABLE) log_error(LOG_INFO, "end without crlf");
			return NULL;
		}

		// must end with crlf
		if ( iscrlf(p)) {
			p += 2;
			return p;
		}
    }
    return NULL;
}

////////////////////////////////////////////////
static int dump_httpheader(httpf_header_parse* parse) {
	begin_mod_st();

	push_mod_ptr(parse->method, parse->method_len);
	push_mod_ptr(parse->ver, parse->ver_len);
	push_mod_ptr(parse->url, parse->url_len);

	log_error(LOG_INFO, "begin http header info:");
	log_error(LOG_INFO, "\tmethod:%s\n\turl:%s\n\tver:%s\n",
		parse->method, parse->url, parse->ver
		);
	for (int i = 0 ; i < parse->header_cnt; i++) {
		push_mod_ptr(parse->header_names[i], parse->header_name_lens[i]);
		push_mod_ptr(parse->header_vals[i], parse->header_val_lens[i]);
		log_error(LOG_INFO, "\theader key:val : %s: %s", parse->header_names[i], parse->header_vals[i]); 
	}
	end_mod_st();
	return 0;
}

static void httpf_header_get_common_info(httpf_header_parse* parse) {
	if (LOG_ENABLE) log_error(LOG_INFO, "getting common info");
	parse->host_index = parse->get_header(parse, "Host");
}

static int httpf_read_httpheader(httpf_header_parse* parse, struct bufferevent* buffer) {
	// todo: peek data from buffer
	size_t nDataSize = EVBUFFER_LENGTH(buffer->input);
	unsigned char* pData = EVBUFFER_DATA(buffer->input);

	size_t nCopySize = nDataSize;
	if (nCopySize > sizeof(parse->data) -1 ) {
		nCopySize = sizeof(parse->data) - 1;
	}
	
	memcpy(parse->data, pData, nCopySize);
	parse->data[nCopySize] = 0;
	parse->data_len = nCopySize;

	if (LOG_ENABLE) log_error(LOG_INFO, "client　request: \n%s", parse->data);

	char* pHeader = request_line_parse(parse);

	// not a http request we care
	if (!pHeader) return -1;

	char* pEnd = strstr(parse->data, "\r\n\r\n");
	if (!pEnd) {
		// maybe the header is not finished yet	
		return 0;
	}

	// parse the rest headers
	char* p = pHeader;
	int cnt = 0;
	do {
		char* name = p;
		while(notend(p) && *p != ':' ) p++;

		// unespaced header info
		if (end(p)) return -2;
		int name_len = p - name;

		// skip :
		p++;
 
		if (end(p) || iscrlf(p)) return -3;

		// skip space
		while (notend(p) && isspace(*p) && notcrlf(p) ) p++;
		if (end(p)) return -4;

		// CRLF is not allowed here
        if (end(p) || iscrlf(p)) return -5;

		char* val = p;
		while(notend(p) && notcrlf(p) ) p++;
		if (end(p)) return -6;
		int val_len = p - val;

		if (iscrlf(p)) {
			p += 2;
			parse->header_names[cnt] = name;
			parse->header_name_lens[cnt] = name_len; 
			parse->header_vals[cnt] = val;
			parse->header_val_lens[cnt] = val_len;
			parse->header_cnt = ++cnt;
		} else {
			// not end with crlf 
			return -7;
		}

		// we meet the header end return the header's size
		if (iscrlf(p)) {
			p += 2;
			httpf_header_get_common_info(parse);
			parse->header_len = p - parse->data;
			return parse->header_len;
		}
	} while (notend(p));	
	return -8;
}

static char* response_line_parse(httpf_response_parse* parse) {
    // Parse the request-line
    // http://tools.ietf.org/html/rfc2616#section-5.1
    // Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
    char * p = parse->data;
	parse->ver = p;

    while (notend(p) && ! isspace(*p) ) p++;
    if ( end(p) || iscrlf(p) ) {
		log_error(LOG_ERR, "response_line_parse 1");
		return NULL;
	}

	parse->ver_len = p - parse->data;
	if (!is_invalid_http_version(parse->ver, parse->ver_len)) { 
		log_error(LOG_ERR, "response_line_parse 2");
		return NULL;
	}

    // Skip space
    while (notend(p) && isspace(*p) && notcrlf(p) ) p++;
	if (end(p) || iscrlf(p)) {
		log_error(LOG_ERR, "response_line_parse 3");
		return NULL;
	}

    // parse status
	parse->status = p;
    while (!isspace(*p) && notcrlf(p) && notend(p) ) p++;
	parse->status_len = p - parse->status;

    // Skip space
    while (notend(p) && isspace(*p) && notcrlf(p) ) p++;
	if (end(p)) {
		log_error(LOG_ERR, "response_line_parse 4");
		return NULL;
	}

	// may not has reason on same server(nginx)
	if (iscrlf(p)) {
		p += 2;
		return p;
	}

	// parse reason
	parse->reason = p;
    while (notcrlf(p) && notend(p) ) p++;
	parse->reason_len = p - parse->reason;

	// SKIP space
	while (notend(p) && isspace(*p) && notcrlf(p) ) p++;
	if (end(p)) {
		log_error(LOG_ERR, "response_line_parse 5");
		return NULL;
	}

	// must end with crlf
	if ( iscrlf(p)) {
		p += 2;
		return p;
	}
	
	log_error(LOG_ERR, "response_line_parse 6");
    return NULL;
}

int httpf_read_response_header(struct bufferevent* buffer, httpf_response_parse* parse)  {
	// todo: peek data from buffer
	size_t nDataSize = EVBUFFER_LENGTH(buffer->input);
	unsigned char* pData = EVBUFFER_DATA(buffer->input);
	
	size_t nCopySize = nDataSize;
	if (nCopySize > sizeof(parse->data) -1 ) {
		nCopySize = sizeof(parse->data) - 1;
	}
	memcpy(parse->data, pData, nCopySize);
	parse->data[nCopySize] = 0;
	parse->data_len = nCopySize;

	char* pEnd = strstr(parse->data, "\r\n\r\n");
	if (!pEnd) {
		// maybe the header is not finished yet	
		return 0;
	}
	
	// not a http response we care
	char* pHeader = response_line_parse(parse);
	if (!pHeader) {
		log_error(LOG_ERR, "parse response failed: %s", parse->data);	
		return -1;	
	}

	int status = 0;
	sscanf(parse->status, "%d", &status);
	char oldc = *(pEnd+4);
	*(pEnd+4) = 0;
	// if (LOG_ENABLE) log_error(LOG_INFO, "server response: %d\n%s", status, parse->data);
	*(pEnd+4) = oldc;

	// parse the rest headers
	char* p = pHeader;
	int cnt = 0;
	do {
		char* name = p;
		while(notend(p) && *p != ':' ) p++;

		// unespaced header info
		if (end(p)) return -2;
		int name_len = p - name;

		// skip :
		p++;
 
		if (end(p) || iscrlf(p)) return -3;

		// skip space
		while (notend(p) && isspace(*p) && notcrlf(p) ) p++;
		if (end(p)) return -4;

		// CRLF is not allowed here
        if (end(p) || iscrlf(p)) return -5;

		char* val = p;
		while(notend(p) && notcrlf(p) ) p++;
		if (end(p)) return -6;
		int val_len = p - val;

		if (iscrlf(p)) {
			p += 2;
			parse->header_names[cnt] = name;
			parse->header_name_lens[cnt] = name_len; 
			parse->header_vals[cnt] = val;
			parse->header_val_lens[cnt] = val_len;
			parse->header_cnt = ++cnt;
		} else {
			// not end with crlf 
			return -7;
		}

		// we meet the header end return the header's size
		if (iscrlf(p)) {
			p += 2;
			parse->header_len = p - parse->data;
			return parse->header_len;
		}
	} while (notend(p));	
	return -8;
}

char* httpf_response_get_header(httpf_response_parse* header, char* key) {
	// copy other header info
	int keyLen = strlen(key);
	for (int i = 0; i < header->header_cnt; i++) {
		char* name = header->header_names[i];
		int name_len = header->header_name_lens[i];
		char* val = header->header_vals[i];

		if ((name_len == keyLen) && (0 == strncmp(key, name, keyLen))) {
			return val;
		}
	}
	return NULL;
}

httpf_response_parse* create_http_response_parse() {
	httpf_response_parse* parse = malloc(sizeof(httpf_response_parse));
	memset(parse, 0, sizeof(httpf_response_parse));
	parse->get_header = &httpf_response_get_header;
	return parse;
}

void httpf_response_free(httpf_response_parse* header) {
	free(header);
}

static int httpf_request_get_header(httpf_header_parse* header, char* key) {
	// copy other header info
	if (LOG_ENABLE) log_error(LOG_INFO, "getting header :%s", key);
	int keyLen = strlen(key);
	for (int i = 0; i < header->header_cnt; i++) {
		char* name = header->header_names[i];
		int name_len = header->header_name_lens[i];
		if ((name_len == keyLen) && (0 == strncmp(key, name, keyLen))) {
			return i;
		}
	}
	return -1;
}

static void httpf_header_free(httpf_header_parse* header) {
	free(header);
}

httpf_header_parse* create_http_header_parse() {
	httpf_header_parse* header = malloc(sizeof(httpf_header_parse));
	memset(header, 0, sizeof(httpf_header_parse));
	header->host_index = -1;
	header->get_header = &httpf_request_get_header;
	header->release = &httpf_header_free;
	header->read_header = &httpf_read_httpheader;
	return header;
}



