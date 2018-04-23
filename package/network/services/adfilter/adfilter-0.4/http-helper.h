#ifndef __HTTP_HELPER_H__
#define __HTTP_HELPER_H__  
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

#define HTTPF_HEAD_WM_HIGH (4096)

typedef struct httpf_buffer_t {
	char *buff;
	int len;
	int max_len;
} httpf_buffer;

int httpf_buffer_init(httpf_buffer *buff);
int httpf_buffer_init2(httpf_buffer *buff, int size);
void httpf_buffer_fini(httpf_buffer *buff);
int httpf_buffer_append(httpf_buffer *buff, const char *data, int len);

/////////////////////////////////////////////////////////////////
#define HTTPF_HEADER_DATA_SIZE	4096+4
#define HTTPF_MAX_HEADER_CNT	64
typedef struct httpf_header_parse_t httpf_header_parse;
struct httpf_header_parse_t {
	char	data[HTTPF_HEADER_DATA_SIZE];
	int		data_len;
	int		header_len;

	char*	method;
	int		method_len;
	
	char*	url;
	int		url_len;

	char*	ver;
	int		ver_len;

	char*	header_names[HTTPF_MAX_HEADER_CNT];
	int		header_name_lens[HTTPF_MAX_HEADER_CNT];
	char*	header_vals[HTTPF_MAX_HEADER_CNT];
	int		header_val_lens[HTTPF_MAX_HEADER_CNT];
	int		header_cnt;

	int		host_index;

	int		(*get_header)(httpf_header_parse* header, char* key);
	int		(*read_header)(httpf_header_parse* header, struct bufferevent *buffer);
	void	(*release)(httpf_header_parse* header);
};

httpf_header_parse* create_http_header_parse();
// void httpf_header_free(httpf_header_parse* header);
// int httpf_read_httpheader(struct bufferevent *buffer, httpf_header_parse* parse);
 
typedef struct httpf_resspond_parse_t httpf_response_parse;
struct httpf_resspond_parse_t
{
	char	data[HTTPF_HEADER_DATA_SIZE];
	int		data_len;
	int		header_len;

	char*	status;
	int		status_len;

	char*	reason;
	int		reason_len;

	char*	ver;
	int		ver_len;

	char*	header_names[HTTPF_MAX_HEADER_CNT];
	int		header_name_lens[HTTPF_MAX_HEADER_CNT];
	char*	header_vals[HTTPF_MAX_HEADER_CNT];
	int		header_val_lens[HTTPF_MAX_HEADER_CNT];
	int		header_cnt;

	char*	(*get_header)(httpf_response_parse* header, char* key);
};

httpf_response_parse* create_http_response_parse();
void httpf_response_free(httpf_response_parse* header);

int httpf_read_response_header(struct bufferevent* buffer, httpf_response_parse* parse); 

//////////////////////////////////////////
struct str_mod_stack_t {
	int	cnt;
	struct {
		char* pos;
		char  val;
	} items[100];
};

#define begin_mod_st()	struct str_mod_stack_t __st = {.cnt = 0};

#define push_mod_ptr(pos_start, pos_len)	{				\
	if (pos_start && pos_len)	{							\
		__st.items[__st.cnt].pos = pos_start + pos_len;		\
		__st.items[__st.cnt].val = *(pos_start+pos_len);	\
		*(pos_start+pos_len) = 0;							\
		__st.cnt++;											\
	}														\
}

#define end_mod_st()				{						\
	for (int i = 0; i < __st.cnt; i++) {					\
		*(__st.items[i].pos) = __st.items[i].val;			\
	}														\
}
//////////////////////////////////////////////////

#define append_buffer(buffer, data, len) {				\
	if (0 != httpf_buffer_append(buffer, data, len)) {	\
		goto exit0;										\
	}													\
}
	
#endif // __HTTP_HELPER_H__
