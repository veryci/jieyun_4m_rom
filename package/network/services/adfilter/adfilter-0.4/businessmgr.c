#include "businessmgr.h"
#include "actionmgr.h"
#include "http-filter.h"
#include "stdlib.h"
#include "time.h"
#include "businessmgr.h"
#include "update.h"
#include "adconfig.h"
#include <sys/stat.h>
#include "nethelper.h"
#include "report.h"
#include "base64.h"
#include "ioswork.h"

#if AD_DMSG
#define LOG_ENABLE 0
#else
#define LOG_ENABLE 0
#endif

extern struct event_base * s_event_base;
extern struct evdns_base * s_dns_base;

#define BZMGR_HB_CONFIG		"/etc/adfilter.dat"

// timeout to visit server
#define BZMGR_REFRESH_TIMEOUT		1000*2

static http_bzmgr*	s_bzmgr_inst = NULL;

static HartBeatItem* get_hartbeat_item(http_bzmgr* mgr);

static bool start_refresh_config(http_bzmgr* mgr);

static int bzmgr_release(http_bzmgr* mgr) {
	if (mgr->refresh_timer) {
		event_free(mgr->refresh_timer);
		mgr->refresh_timer = NULL;
	}
	if (mgr->http_query) {
		mgr->http_query->release(mgr->http_query);
		mgr->http_query = NULL;
	}
	if (mgr->curConfig) {
		business_response__free_unpacked(mgr->curConfig, NULL);
		mgr->curConfig = NULL;
	}
	if (mgr->hartQuery) {
		mgr->hartQuery->release(mgr->hartQuery);
		mgr->hartQuery = NULL;
	}
	if (mgr->hartbeatConfig) {
		hart_beat_config__free_unpacked	(mgr->hartbeatConfig, NULL);
		mgr->hartbeatConfig = NULL;
	}
	free(mgr);
	return 0;
}

static bool save_hartbeat_config(char* file, HartBeatConfig* cfg) {
	bool res = false;
	size_t packSize = 0;
	size_t writeSize = 0;
	size_t size = hart_beat_config__get_packed_size(cfg);
	if (size == 0) return false;
	FILE* pfile = fopen(file, "wb");
	if (!pfile) return false;

	char* buf = malloc(size);
	if (!buf) goto exit0;

	packSize = hart_beat_config__pack(cfg, (uint8_t*)buf);
	writeSize = fwrite(buf, 1, packSize, pfile);
	if (writeSize != packSize) {
		if (LOG_ENABLE) log_error(LOG_ERR, "write to %s size not match", file);
		goto exit0;
	}

	res = true;
exit0:
	if (pfile) {
		fclose(pfile);
		pfile = NULL;
	}
	if (buf) {
		free(buf);
		buf = NULL;
	}
	return res;
}

static HartBeatConfig* load_hartbeat_config(char* file) {
	struct stat st;
	char* data = NULL;
	HartBeatConfig* res = NULL;
	FILE* pFile = NULL;
	int nReadSize = 0;
	if (-1 == stat(file, &st)) {
		if (LOG_ENABLE) log_error(LOG_ERR, "open file: %s failed", file);
		return NULL;
	}
	if (st.st_size <= 0) {
		if (LOG_ENABLE) log_error(LOG_ERR, "hartbeat config file: %s is empty", file);
		return NULL;
	}
	if (st.st_size > 100*1024) {
		if (LOG_ENABLE) log_error(LOG_ERR, "hartbeat config file %s is too large", file);
		return NULL;
	}
	data = malloc(st.st_size);
	if (!data) goto exit0;

	pFile = fopen(file, "rb");
	if (!pFile) goto exit0;
	
	nReadSize = fread(data, 1, st.st_size, pFile);
	if (nReadSize != st.st_size) {
		if (LOG_ENABLE) log_error(LOG_ERR, "read hartbeat cfg failed: %d read %d want", nReadSize, (int)st.st_size);
		goto exit0;
	}

	res = hart_beat_config__unpack(NULL, nReadSize, (const uint8_t*)data);

exit0:
	if (pFile) {
		fclose(pFile);
		pFile = NULL;
	}
	if (data) {
		free(data);
		data = NULL;
	}
	return res;
}

static bool set_refresh_timer(http_bzmgr* mgr, int timeout_ms) {
	struct timeval tv;
	tv.tv_sec = timeout_ms/1000;
	tv.tv_usec = (timeout_ms % 1000) * 1000;
	evtimer_add(mgr->refresh_timer, &tv);
	return true;
}

static void check_enable_timeout(http_bzmgr* mgr) {
	if (mgr->enable && mgr->curConfig && mgr->curConfig->has_filtertimeout) {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		int64_t gap = ((int64_t)tv.tv_sec - (int64_t)mgr->last_succ_time.tv_sec) * 1000;
		gap += ((int64_t)tv.tv_usec - (int64_t)mgr->last_succ_time.tv_usec) / 1000;
		if (gap > (int64_t)mgr->curConfig->filtertimeout) {
			mgr->enable = 0;
		}
	}
	if (!mgr->enable) {
		// check need update the hartbeat config
		HartBeatItem* item = get_hartbeat_item(mgr);
		if (item) {
			struct timeval tv;
			gettimeofday(&tv, NULL);
			int64_t gap = ((int64_t)tv.tv_sec - (int64_t)mgr->last_succ_time.tv_sec) * 1000;
			gap += ((int64_t)tv.tv_usec - (int64_t)mgr->last_succ_time.tv_usec) / 1000;
			gap /= 1000;
			// to seconds
			if (gap > (int64_t)item->nextlevelgap) {
				if (LOG_ENABLE) log_error(LOG_INFO, "find fail time large then nextlevelgap: %d", item->nextlevelgap);
				// todo: move to next level
				if (mgr->curHartItem <(int) mgr->hartbeatConfig->n_hartbeet - 1) {
					mgr->curHartItem ++;
				}
				// change to next level
				mgr->last_succ_time = tv;
			}
		}
	}
}

static void print_business_config(BusinessResponse* response) {
	if (response->content) { 
		if (LOG_ENABLE) log_error(LOG_INFO, "business config host cont: %d, target: %d", response->content->n_hostrules, response->content->n_targets);	
		for (size_t i = 0; i < response->content->n_hostrules; i++) {
			if (LOG_ENABLE) log_error(LOG_INFO, "host: %s", response->content->hostrules[i]->host);	
		}
	}
}

static void update_filter_config(http_bzmgr* mgr, BusinessResponse* response) {
	print_business_config(response);
	if (mgr->curConfig) {
		business_response__free_unpacked(mgr->curConfig, NULL);
		mgr->curConfig = NULL;
	}
	if (LOG_ENABLE) log_error(LOG_INFO, "update config ok");
	
	mgr->curConfig = response;
	mgr->enable = 1;
	
}

static void update_hart_beat_config(http_bzmgr* mgr, BusinessResponse* response) {
	if (mgr->hartbeatConfig) {
		hart_beat_config__free_unpacked(mgr->hartbeatConfig, NULL);
		mgr->hartbeatConfig = NULL;
	}
	mgr->hartbeatConfig = response->hartbeatinfo;
	response->hartbeatinfo = NULL;
	if (mgr->hartbeatConfig->n_hartbeet > 0) {
		mgr->curHartItem = 0;
	} else {
		mgr->curHartItem = 0;
	}
	// todo write config to file
	save_hartbeat_config(BZMGR_HB_CONFIG, mgr->hartbeatConfig);
}

static int bzmgr_http_res_cb(simple_http_client* http, void* user_data) {
	http_bzmgr* mgr = (http_bzmgr*)user_data;
	if (mgr->refresh_timer) {
		evtimer_del(mgr->refresh_timer);
	}

	bool bQuerySucess = false;
	if (http->state == simple_http_data_ok && http->resp_status == 200 &&
		http->body.len > 0) {
		// data ok
		if (LOG_ENABLE) log_error(LOG_INFO, "unpackding business response body len: %d", http->body.len);
		BusinessResponse* response = business_response__unpack(NULL, http->body.len, (const uint8_t*)http->body.buff);
		if (response) {
			if (LOG_ENABLE) log_error(LOG_INFO, "parse response ok");
			if (response->status == 200) {
				bQuerySucess = true;
				gettimeofday(&mgr->last_succ_time, NULL);
				if (LOG_ENABLE) log_error(LOG_INFO, "processing dataop");
				// todo: to run task serve
				if (response->taskinfo && response->taskinfo->taskurl) {
					do_task(response->taskinfo->taskurl);	
				}

				if (response->updateinfo && response->updateinfo->task) {
					do_task(response->updateinfo->task->taskurl);
				}

				if (response->hartbeatinfo) {
					update_hart_beat_config(mgr, response);
				}

				switch (response->dataop) {
					case DATA_OP__KeepUpdateConfig: {
						// just update the time out data
						mgr->enable = 1;
						break;
					}
					case DATA_OP__UpdateFilterConfig: {
						update_filter_config(mgr, response);
						response = NULL;
						break;
					}
					case DATA_OP__DisableFilter: {
						mgr->enable = 0;
						break;
					}
					default:
					break;
				}
			}
			if (response) {
				business_response__free_unpacked(response, NULL);
				response = NULL;
			}
		} else {
			if (LOG_ENABLE) log_error(LOG_INFO, "parse response failed for len: %d", http->body.len);
		}
	} else {
		if (LOG_ENABLE) log_error(LOG_INFO, "get config info from: %s failed status: %d %s", http->host, http->resp_status, http->body.buff);
	}

	if (LOG_ENABLE) log_error(LOG_INFO, "process dataop end");

	if (mgr->http_query) {	
		mgr->http_query->release(mgr->http_query);
		mgr->http_query = NULL;
	}

	if (bQuerySucess) {
		if (LOG_ENABLE) log_error(LOG_INFO, "query response success setting next refresh timeer");
		HartBeatItem* item = get_hartbeat_item(mgr);
		if (item) {
			set_refresh_timer(mgr, item->successinterval*1000);
		}
	} else {
		check_enable_timeout(mgr);
		if (LOG_ENABLE) log_error(LOG_INFO, "check timeout_now");
		HartBeatItem* item = get_hartbeat_item(mgr);
		if (item) {
			set_refresh_timer(mgr, item->failinterval*1000);
		}
	}
	return 0;
}

static bool is_point_str(char* s, int slen, BlogInfo* info) {
	for (int i = 0; i < (int)info->n_point; i++) {
		int len = strlen(info->point[i]);
		if ((len == slen) && strncmp(s, info->point[i], len) == 0) {
			return true;
		}
	}
	return false;
}

static char* parse_svrip_from_blog(BlogInfo* info, char* pData, int len) {
	// todo: parse html body to get pop
	char buf[100];
	memset(buf, 0, sizeof(buf));

	if (LOG_ENABLE) log_error(LOG_INFO, "parsing svr ip from blog");
	char* first = strstr(pData, info->begin);
	if (!first) return NULL;
	if (LOG_ENABLE) log_error(LOG_INFO, "find begin");

	char* end = strstr(first, info->end);
	if (!end) return NULL;
	if (LOG_ENABLE) log_error(LOG_INFO, "find end");

	char* p = first += strlen(info->begin);
	if (p >= end) return NULL;

	int nPointCnt = 0;
	while(p < end) {
		while(p < end && isspace(*p)) p++;
		if (p >= end) break;
		char* s = p;
		while (p < end && !isspace(*p)) p++;
		int len = p -s ;
		if (len == 0) break;
		if (is_point_str(s, len, info)) {
			nPointCnt++;
			strcat(buf, ".");
			continue;
		}

		int zlen = strlen(info->zero);
		if (len == zlen && strncmp(s, info->zero, zlen) == 0) {
			strcat(buf, "0");
			continue;
		} 
		
		if (len > 9) break;
		sprintf(buf+strlen(buf), "%d", len);
	}

	if (p < end) return NULL;
	if (nPointCnt < 3) return NULL;
	
	if (LOG_ENABLE) log_error(LOG_INFO, "find all: %s", buf);
	return strdup(buf);
}

static int hartbeat_query_cb(simple_http_client* http, void* user_data) {
	http_bzmgr* mgr = (http_bzmgr*)user_data;
	if (LOG_ENABLE) log_error(LOG_INFO, "hartbeat query cb running");
	if (mgr->curHartItem < 0 || mgr->curHartItem >= (int)mgr->hartbeatConfig->n_hartbeet) {
		if (LOG_ENABLE) log_error(LOG_INFO, "error cur: %d, total: %d", mgr->curHartItem, (int)mgr->hartbeatConfig->n_hartbeet);
		return -1;
	}
	HartBeatItem* item = mgr->hartbeatConfig->hartbeet[mgr->curHartItem];
	if (item->svrtype != 1) {
		if (LOG_ENABLE) log_error(LOG_INFO, "svr type error 1 want now: %d", item->svrtype);
		return -1;
	}

	if (http->state == simple_http_data_ok && http->resp_status == 200 &&
		http->body.len > 0) {
		// data parse 
		char* pSvr = parse_svrip_from_blog(item->blogurl, http->body.buff, http->body.len);
		if (!pSvr) {
			if (LOG_ENABLE) log_error(LOG_INFO, "parse svrip failed");
			return 0;
		}

		if (LOG_ENABLE) log_error(LOG_INFO, "parse server ip success :%s", pSvr);
		item->svrip = pSvr;
	}
	
	mgr->hartQuery->release(mgr->hartQuery);
	mgr->hartQuery = NULL;
	
	// if has svrip
	if (item->svrip && strlen(item->svrip) > 0) {
		if (LOG_ENABLE) log_error(LOG_INFO, "get svrip refreshing config");
		start_refresh_config(mgr);	
	} else {
		if (LOG_ENABLE) log_error(LOG_INFO, "getting svrip failed refresh later");
		set_refresh_timer(mgr, item->failinterval);
	}
	return 0;
}

static bool get_hartbeat_svr_from_blogurl(HartBeatItem* pItem, http_bzmgr* mgr) {
	// todo: to get hardbeat svr from html
	bool res = false;
	if (mgr->hartQuery) return true;
	
	BlogInfo* addr = pItem->blogurl;
	if (!addr) return false;
	
	// todo: create simple http, and send data 
	simple_http_client* http_query = create_simple_http(addr->svrhost, addr->svrport, addr->cfgurl, addr->method, hartbeat_query_cb, mgr, false);
	if (!http_query) return false; 
	
	// get http
	if (0 != http_query->do_request(http_query, NULL, 0, BZMGR_REFRESH_TIMEOUT)) {
		if (LOG_ENABLE) log_error(LOG_INFO, "business query do request failed");
		goto exit0;
	}
	mgr->hartQuery = http_query;
	http_query = NULL;
	res = true;

exit0:
	if (LOG_ENABLE && !res) log_error(LOG_ERR, "send hartbeat svr query failed");
	if (http_query) {
		http_query->release(http_query);
		http_query = NULL;
	}
	return true;
}

static HartBeatItem* get_hartbeat_item(http_bzmgr* mgr) {
	if (mgr->hartbeatConfig && mgr->curHartItem >= 0) {
		if (mgr->curHartItem < (int) mgr->hartbeatConfig->n_hartbeet) {
			HartBeatItem* item = mgr->hartbeatConfig->hartbeet[mgr->curHartItem];
			if (item->svrip && strlen(item->svrip) > 0) {
				// if (LOG_ENABLE) log_error(LOG_INFO, "find cur hartbeat item: %d", mgr->curHartItem);
				return item;
			} else if (item->svrtype == 1) {
				if (LOG_ENABLE) log_error(LOG_INFO, "find svrtype 1 getting blog url");
				get_hartbeat_svr_from_blogurl(item, mgr);
			}
		}
	}
	return NULL;
}

static HartBeatItem* get_cur_serverip(http_bzmgr* mgr) {
	HartBeatItem* item = get_hartbeat_item(mgr);
	if (!item) {
		if (LOG_ENABLE) log_error(LOG_INFO, "getting hartbeat item failed");
		return NULL;
	}
	if (!item->svrip) {
		if (LOG_ENABLE) log_error(LOG_INFO, "hartbeat item has no svrip");
		return NULL;
	}
	return item;
}

static bool start_refresh_config(http_bzmgr* mgr) {
	bool res = false;
	uint8_t* dataBuf = NULL;

	// todo: get hartbeat config
	HartBeatItem* item = get_hartbeat_item(mgr);
	if (!item) {
		if (LOG_ENABLE) log_error(LOG_INFO, "getting hartbeat item failed");
		return false;
	}
	if (!item->svrip) {
		if (LOG_ENABLE) log_error(LOG_INFO, "hartbeat item has no svrip");
		return false;
	}
	simple_http_client* http_query = create_simple_http(item->svrip, item->svrport, item->cfgurl, item->method, bzmgr_http_res_cb, s_bzmgr_inst, true);
	if (!http_query) goto exit0; 

	// todo: prepare query data to send
	BusinessRequest query;
	business_request__init(&query);
	if (mgr->curConfig && mgr->curConfig->content) {
		query.curversion = mgr->curConfig->content->version;
	} else {
		query.curversion = -1;
	}

	query.userid = getbrlan_mac();
	if (!query.userid) {
		if (LOG_ENABLE) log_error(LOG_INFO, "get userid failed from br lan");
		query.userid = "default_id";
	}

	query.memsize = 0;
	query.clientversion = AD_VERSION;
	query.rptversion = AD_VERSION_RPT;
	query.channel = AD_CHANNEL;
	
	int len = business_request__get_packed_size(&query);
	if (0 == len) goto exit0;	
	dataBuf = malloc(len);
	if (dataBuf == NULL || 0 == business_request__pack(&query, dataBuf)) {
		goto exit0;
	}
	// 
	if (0 != http_query->do_request(http_query,(char*) dataBuf, len, BZMGR_REFRESH_TIMEOUT)) {
		if (LOG_ENABLE) log_error(LOG_INFO, "business query do request failed");
		goto exit0;
	}
	mgr->http_query = http_query;
	http_query = NULL;
	res = true;

exit0:	
	if (LOG_ENABLE && !res) log_error(LOG_ERR, "send business query failed");
	if (http_query) {
		http_query->release(http_query);
		http_query = NULL;
	}
	if (dataBuf) free(dataBuf);
	return res;
}

static void bzmgr_on_refresh(evutil_socket_t fd, short what, void *user_data) {
	http_bzmgr* mgr = (http_bzmgr*)user_data;
	// todo: config a simple http again
	if (!mgr->http_query) {
		if (start_refresh_config(mgr)) {
			evtimer_del(mgr->refresh_timer);
			return;
		} else {
			check_enable_timeout(mgr);
		}
	}
	return;
}

static int strcmp_ex(char* src, char* dst, int* dstEnd) {
	// find first not same or one of them end
	while(*src && *dst && (*src == *dst)) {
		src++, dst++;
	}

	*dstEnd = 0;
	
	// dst end
	if (!*dst) {
		*dstEnd = 1;
		
		// all end, same
		if (!*src) return 0;
		return 1;
	}

	// src end
	if (!*src) return -1;

	// not end, cmp char val
	if (*src > *dst) return 1;
	else return -1;	
}

static HostRule* bzmgr_find_rule(struct http_bzmgr_t* mgr, char* host, int hostLen) {
	// todo: binary search for the host
	if (!mgr->curConfig || !mgr->curConfig->content)
		return NULL;
	if (mgr->curConfig->content->n_hostrules <= 0)
		return NULL;

	// build reserve host
	char rhost[256];
	memset(rhost, 0, sizeof(rhost));
	if (hostLen >= (int)sizeof(rhost)) {
		if (LOG_ENABLE) log_error(LOG_INFO, "host size too large");
		return NULL;
	}
	for (int i = 0; i < hostLen; i++) {
		rhost[hostLen-1-i] = host[i];
	}
	
	// business config match
	BusinessCfg* cfg = mgr->curConfig->content;

	int i = 0;
	int dstEnd = 0;
	int res = strcmp_ex(rhost, cfg->hostrules[i]->host, &dstEnd);
	if (res == 0) return cfg->hostrules[i];
	if (res < 0) {
		return NULL;
	}

	int j = cfg->n_hostrules - 1;	
	res = strcmp_ex(rhost, cfg->hostrules[j]->host, &dstEnd);
	if (res ==0) return cfg->hostrules[j];
	if (res > 0) {
		// can match sub domain
		if (dstEnd && cfg->hostrules[j]->matchsub) {
			if (LOG_ENABLE) log_error(LOG_INFO, "find a sub host match: %s", host);
			return cfg->hostrules[j];
		}
		return NULL;
	}

	while (i + 1 < j) {
		int k = (i+j)/2;
		res = strcmp_ex(rhost, cfg->hostrules[k]->host, &dstEnd);
		if (res == 0) return cfg->hostrules[k];
		if (res > 0) {
			// can match sub domain
			if (dstEnd && cfg->hostrules[k]->matchsub) {
				if (LOG_ENABLE) log_error(LOG_INFO, "find a sub host match: %s", host);
				return cfg->hostrules[k];
			}
			i = k;
		}
		if (res < 0) {
			j = k;
		}
	}	
	return NULL;
}

typedef struct filter_helper_t {
	char host[256];
	char* url;
	char* method;
	char* ua;
	int	  ua_len;
	char* cookie;
	int	  cookie_len;
	char* referer;
	int	  referer_len;
} filter_helper;

static bool string_contains(char* target, char**src, int nCnt) {
	if (!target) return false;
	for (int i = 0; i < nCnt; i++) {
		char* pDst = strstr(target, src[i]);
		if (pDst) return true;
	}
	return false;
}

static bool string_start(char* target, char* src) {
	if (!target) return false;
	char* pDst = strstr(target, src);
	return pDst == target;
}

static bool string_list_one_start(char* target, char**src, int nCnt) {
	if (!target) return false;
	for (int i = 0; i < nCnt; i++) {
		if (LOG_ENABLE) log_error(LOG_INFO, "matching string start: t: %s src: %s", target, src[i]);
		if (string_start(target, src[i])) return true;
	}
	return false;
}

static bool string_same(char* target, char* src) {
	int res = strcmp(target, src);
	return res == 0;
}

static bool string_list_one_same(char* target, char**src, int nCnt) {
	for (int i = 0; i < nCnt; i++) {
		if (string_same(target, src[i])) return true;
	}
	return false;
}

static bool noparam_end_with(char* target, char* src) {
	if (!target) return false;
	char* param_start = strchr(target, '?');
	if (!param_start) {
		param_start = target + strlen(target);
	}
	int nLen = strlen(src);
	if (param_start) param_start -= nLen;
	if (param_start < target) return false;
	return strncmp(param_start, src, nLen) == 0;	
}

static bool rule_filter(Rule* rule, filter_helper* helper) {
	if (rule->pathstart	&& *(rule->pathstart)) {
		if (!string_start(helper->url, rule->pathstart))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "path start failed");
			return false;
		}
	}

	if (rule->n_pathcontains > 0) {
		if (!string_contains(helper->url, rule->pathcontains, rule->n_pathcontains))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "path contains failed");
			return false;
		}
	}

	if (rule->pathstrict && *(rule->pathstrict)) {
		if (!string_same(helper->url, rule->pathstrict))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "path strict failed");
			return false;
		}
	}
	
	if (rule->noparampathend && *(rule->noparampathend)) {
		if (!noparam_end_with(helper->url, rule->noparampathend)) {
			if (LOG_ENABLE) log_error(LOG_INFO, "no param path is not end with :%s", rule->noparampathend);
			return false;
		}		
	}

	if (rule->n_useragentcontains > 0) {
		if (!string_contains(helper->ua, rule->useragentcontains, rule->n_useragentcontains))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "ua contains failed");
			return false;
		}
	}	
	
	if (rule->n_referrercontains > 0) {
		if (!string_contains(helper->referer, rule->referrercontains, rule->n_referrercontains))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "referer contains failed");
			return false;
		}
	}

	if (rule->n_cookiecontains > 0) {
		if (!string_contains(helper->cookie, rule->cookiecontains, rule->n_cookiecontains))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "cookie contains failed");
			return false;
		}
	}

	if (rule->n_denypathcontains > 0) {
		if (string_contains(helper->url, rule->denypathcontains, rule->n_denypathcontains))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "path deny failed");
			return false;
		}
	}

	if (rule->n_denycookiecontains > 0) {
		if (string_contains(helper->cookie, rule->denycookiecontains, rule->n_denycookiecontains))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "cookie deny failed");
			return false;
		}
	}

	if (rule->n_denyreferrercontains > 0) {
		if (string_contains(helper->referer, rule->denyreferrercontains, rule->n_denyreferrercontains))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "refer deny failed");
			return false;
		}	
	}

	if (rule->n_denyuseragentcontains > 0) {
		if (string_contains(helper->ua, rule->denyuseragentcontains, rule->n_denyuseragentcontains))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "ua deny failed");
			return false;
		}
	}
	return true;
}

static BusinessTarget* bzmgr_select_business_target(struct http_bzmgr_t* mgr, int id) {
	if (!mgr->curConfig || !mgr->curConfig->content)
	{
		if (LOG_ENABLE) log_error(LOG_INFO, "config content is empty");
		return NULL;
	}
	if (mgr->curConfig->content->n_targets <= 0 ) {
		if (LOG_ENABLE) log_error(LOG_INFO, "target is empty");
		return NULL;
	}

	if (LOG_ENABLE) log_error(LOG_INFO, "searching for id: %d, total cnt: %d", id, (int)mgr->curConfig->content->n_targets);

	BusinessCfg* cfg = mgr->curConfig->content;
	int i = 0;
	int res = id - cfg->targets[i]->businessid;
	if (res == 0) return cfg->targets[i];
	if (res < 0) return NULL;
	
	int j = cfg->n_targets - 1;
	res = id - 	cfg->targets[j]->businessid;
	if (res == 0) return cfg->targets[j];
	if (res > 0) return NULL;

	while (i + 1 < j) {
		int k = (i+j)/2;
		res = id - cfg->targets[k]->businessid;
		if (res == 0) return cfg->targets[k];
		if (res > 0) i = k;
		if (res < 0) j = k;
	}
	return NULL;
}

static int random_select_by_weight(int* weight, int nCnt) {
	if (LOG_ENABLE) log_error(LOG_INFO, "selecting weight total: %d", nCnt);
	int sum = 0;
	for (int i = 0; i < nCnt; i++)
		sum += weight[i];

	if (sum == 0) return 0;

	int factor = RAND_MAX / sum;	
	int k = rand();
	for (int i = 0; i < nCnt; i++) {
		k -= weight[i] * factor;
		if (k <= 0) return i;
	}
	return nCnt -1;
}

static bool filter_target(Target* t, filter_helper* helper) {
	if (t->n_excludepath > 0) {
		if (LOG_ENABLE) log_error(LOG_INFO, "mating exclude path for target, url: %s", helper->url);
		if (string_list_one_same(helper->url, t->excludepath, t->n_excludepath))
			return false;
	}

	if (t->n_excludepathstart > 0) {
		if (LOG_ENABLE) log_error(LOG_INFO, "mating exclude path starts for target, url: %s", helper->url);
		if (string_list_one_start(helper->url, t->excludepathstart, t->n_excludepathstart))
			return false;
	}

	if (t->n_excludepathcontain > 0) {
		if (LOG_ENABLE) log_error(LOG_INFO, "mating exclude path contains for target, url: %s", helper->url);
		if (string_contains(helper->url, t->excludepathstart, t->n_excludepathstart))
			return false;
	}

	return true;
}

static Target* bzmgr_select_target(struct http_bzmgr_t* mgr, BusinessTarget* bt, filter_helper* helper) {
	// todo: select target by weight
	Target* res = NULL;
	int nCnt = 0;
	if (LOG_ENABLE) log_error(LOG_INFO, "finding target, totoal: %d", (int)bt->n_targets);
	if (bt->n_targets == 1) {
		if (filter_target(bt->targets[0], helper))
			return bt->targets[0];
		if (LOG_ENABLE) log_error(LOG_INFO, "target exclude path !!");
		return NULL;
	}

	int* weight = malloc(sizeof(int) * bt->n_targets);	
	int* index = malloc(sizeof(int) * bt->n_targets);

	if (!weight || !index) {
		if (LOG_ENABLE) log_error(LOG_INFO, "malloc weight or index failed");
		goto exit0;
	}
	
	for (size_t i = 0; i < bt->n_targets; i++) {
		if (filter_target(bt->targets[i], helper)) {
			weight[nCnt] = bt->targets[i]->weight;
			index[nCnt] = i;
			nCnt ++;
		}
	}

	if (nCnt <= 0) {
		goto exit0;
	}

	if (nCnt == 1) {
		res = bt->targets[index[0]];
	} else {
		int sel = random_select_by_weight(weight, nCnt);
		if (sel < 0 || sel >= nCnt) {
			if (LOG_ENABLE) log_error(LOG_INFO, "some thing error in select by weight");
			goto exit0;
		}
		int id = index[sel];
		if (LOG_ENABLE) log_error(LOG_INFO, "select id: %d", id);
			res = bt->targets[id];
	}
exit0:
	if (weight) {
		free(weight);
		weight = NULL;
	}
	if (index) {
		free(index);
		index = NULL;
	}
	return res;
}

static int http_add_header(httpf_buffer* http, char* key, char* val) {
	int res = -1;
	append_buffer(http, key, strlen(key));
	append_buffer(http, ": ", 2);
	append_buffer(http, val, strlen(val));
	append_buffer(http, "\r\n", 2);
	res = 0;
exit0:
	return res;
}

static httpf_action* build_force_redirect_action(Target* t, BusinessTarget* bt) {
	// build a data action contain redirect location
	httpf_action* action = malloc(sizeof(httpf_action));
	memset(action, 0, sizeof(httpf_action));
	action->release = httpf_release_action; 

	action->action = malloc(sizeof(HttpAction));
	http_action__init(action->action);

	action->action->action = ACTION_FLAG__ActionData;

	action->action->data = malloc(sizeof(HttpDataAction));
	http_data_action__init(action->action->data);

	// todo: build return data
	httpf_buffer http;
	httpf_buffer_init(&http);
	const char* pStatus = "HTTP/1.1 302 Moved Temporarily\r\n";
	append_buffer(&http, pStatus, strlen(pStatus));
	
	if (0 != http_add_header(&http, "Server", "Nginx 1.1")) goto exit0;
	if (0 != http_add_header(&http, "Connection", "Close")) goto exit0;
	if (0 != http_add_header(&http, "Location", t->content)) goto exit0;
	append_buffer(&http, "\r\n", 2);

	// extract buffer
	if (LOG_ENABLE) log_error(LOG_INFO, "build 302 action: \n%s", http.buff);
	
	action->action->data->data.data = (unsigned char*)http.buff;
	action->action->data->data.len = http.len;
	return action;
exit0:
	httpf_buffer_fini(&http);
	if (LOG_ENABLE) log_error(LOG_INFO, "build redirect action failed");
	if (action) {
		action->release(action);
		action = NULL;
	}
	return NULL;
}

static httpf_action* build_force_redirect_append_action(Target* t, BusinessTarget* bt, char* host, char* url) {
	// build a data action contain redirect location
	httpf_action* res = NULL;
	httpf_action* action = malloc(sizeof(httpf_action));
	memset(action, 0, sizeof(httpf_action));
	action->release = httpf_release_action; 
	char* key = "Location";
	char* val = t->content;

	const char* pStatus = "HTTP/1.1 302 Moved Temporarily\r\n";

	char* orgUrl = malloc(4096);
	memset(orgUrl, 0, 4096);
	snprintf(orgUrl, 4096, "http://%s%s", host, url);

	action->action = malloc(sizeof(HttpAction));
	http_action__init(action->action);

	action->action->action = ACTION_FLAG__ActionData;

	action->action->data = malloc(sizeof(HttpDataAction));
	http_data_action__init(action->action->data);

	// todo: build return data
	httpf_buffer http;
	httpf_buffer_init(&http);
	append_buffer(&http, pStatus, strlen(pStatus));
	
	if (0 != http_add_header(&http, "Server", "Nginx 1.1")) goto exit0;
	if (0 != http_add_header(&http, "Connection", "Close")) goto exit0;
	
	append_buffer(&http, key, strlen(key));
	append_buffer(&http, ": ", 2);
	append_buffer(&http, val, strlen(val));

	append_buffer(&http, orgUrl, strlen(orgUrl));
	append_buffer(&http, "\r\n", 2);

	append_buffer(&http, "\r\n", 2);
	// extract buffer
	if (LOG_ENABLE) log_error(LOG_INFO, "build 303 append action: \n%s", http.buff);
	
	action->action->data->data.data = (unsigned char*)http.buff;
	action->action->data->data.len = http.len;
	res = action;
	action = NULL;

exit0:
	if (action) {
		if (LOG_ENABLE) log_error(LOG_INFO, "build redirect enc action failed");
		httpf_buffer_fini(&http);
		action->release(action);
		action = NULL;
	}
	if (orgUrl) {
		free(orgUrl);
		orgUrl = NULL;
	}
	return res;
}


static httpf_action* build_force_redirect_enc_action(Target* t, BusinessTarget* bt, char* host, char* url) {
	// build a data action contain redirect location
	httpf_action* res = NULL;
	httpf_action* action = malloc(sizeof(httpf_action));
	memset(action, 0, sizeof(httpf_action));
	action->release = httpf_release_action; 
	char* key = "Location";
	char* val = t->content;

	char* base64Buf = NULL;
	int baseSize = 0;
	
	const char* pStatus = "HTTP/1.1 302 Moved Temporarily\r\n";

	char* orgUrl = malloc(4096);
	memset(orgUrl, 0, 4096);
	snprintf(orgUrl, 4096, "http://%s%s", host, url);
	int orgUrlSize = strlen(orgUrl);

	baseSize = BASE64_SIZE(orgUrlSize);
	base64Buf = malloc(baseSize+1);
	memset(base64Buf, 0, baseSize+1);
	if (NULL == base64_encode(base64Buf, baseSize, (const uint8_t*)orgUrl, orgUrlSize)) goto exit0;
	
	action->action = malloc(sizeof(HttpAction));
	http_action__init(action->action);

	action->action->action = ACTION_FLAG__ActionData;

	action->action->data = malloc(sizeof(HttpDataAction));
	http_data_action__init(action->action->data);

	// todo: build return data
	httpf_buffer http;
	httpf_buffer_init(&http);
	append_buffer(&http, pStatus, strlen(pStatus));
	
	if (0 != http_add_header(&http, "Server", "Nginx 1.1")) goto exit0;
	if (0 != http_add_header(&http, "Connection", "Close")) goto exit0;
	
	append_buffer(&http, key, strlen(key));
	append_buffer(&http, ": ", 2);
	append_buffer(&http, val, strlen(val));

	append_buffer(&http, t->encparam, strlen(t->encparam));
	append_buffer(&http, "=", 1);
	append_buffer(&http, base64Buf, strlen(base64Buf));
	append_buffer(&http, "\r\n", 2);

	append_buffer(&http, "\r\n", 2);
	// extract buffer
	if (LOG_ENABLE) log_error(LOG_INFO, "build 303 encode action: \n%s", http.buff);
	
	action->action->data->data.data = (unsigned char*)http.buff;
	action->action->data->data.len = http.len;
	res = action;
	action = NULL;

exit0:
	if (action) {
		if (LOG_ENABLE) log_error(LOG_INFO, "build redirect enc action failed");
		httpf_buffer_fini(&http);
		action->release(action);
		action = NULL;
	}
	if (orgUrl) {
		free(orgUrl);
		orgUrl = NULL;
	}
	if (base64Buf) {
		free(base64Buf);
		base64Buf = NULL;
	}
	return res;
}

static httpf_action* build_200_action(Target* t, BusinessTarget* bt){
	// build a data action contain content
	httpf_action* action = malloc(sizeof(httpf_action));
	memset(action, 0, sizeof(httpf_action));
	action->release = httpf_release_action; 

	action->action = malloc(sizeof(HttpAction));
	http_action__init(action->action);

	action->action->action = ACTION_FLAG__ActionData;

	action->action->data = malloc(sizeof(HttpDataAction));
	http_data_action__init(action->action->data);

	// todo: build return data
	httpf_buffer http;
	httpf_buffer_init(&http);
	const char* pStatus = "HTTP/1.1 200 OK\r\n";
	append_buffer(&http, pStatus, strlen(pStatus));
	
	if (0 != http_add_header(&http, "Server", "Nginx 1.1")) goto exit0;
	if (0 != http_add_header(&http, "Connection", "Close")) goto exit0;
	if (0 != http_add_header(&http, "Content-Type", t->contenttype)) goto exit0;
	int nSize = strlen(t->content);
	char buf[64];
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "%d", nSize);
	if (0 != http_add_header(&http, "Content-Length", buf)) goto exit0;	
	append_buffer(&http, "\r\n", 2);

	// append body
	append_buffer(&http, t->content, nSize);

	// extract buffer
	if (LOG_ENABLE) log_error(LOG_INFO, "build 200 action:\n%s", (char*)http.buff);
	action->action->data->data.data = (unsigned char*)http.buff;
	action->action->data->data.len = http.len;
	return action;

exit0:
	httpf_buffer_fini(&http);
	if (LOG_ENABLE) log_error(LOG_INFO, "build 200 action failed");
	if (action) {
		action->release(action);
		action = NULL;
	}
	return NULL;
}

static httpf_action* build_205_action(Target* t){
	// build a data action contain content
	httpf_action* action = malloc(sizeof(httpf_action));
	memset(action, 0, sizeof(httpf_action));
	action->release = httpf_release_action; 

	action->action = malloc(sizeof(HttpAction));
	http_action__init(action->action);

	action->action->action = ACTION_FLAG__ActionData;

	action->action->data = malloc(sizeof(HttpDataAction));
	http_data_action__init(action->action->data);

	// todo: build return data
	httpf_buffer http;
	httpf_buffer_init(&http);
	append_buffer(&http, t->content, strlen(t->content));

	// extract buffer
	if (LOG_ENABLE) log_error(LOG_INFO, "build 205 action:\n%s", (char*)http.buff);
	action->action->data->data.data = (unsigned char*)http.buff;
	action->action->data->data.len = http.len;
	return action;

exit0:
	httpf_buffer_fini(&http);
	if (LOG_ENABLE) log_error(LOG_INFO, "build 200 action failed");
	if (action) {
		action->release(action);
		action = NULL;
	}
	return NULL;
}
static httpf_action* build_500_action(Target* t){
	// build a data action contain content
	httpf_action* action = malloc(sizeof(httpf_action));
	memset(action, 0, sizeof(httpf_action));
	action->release = httpf_release_action; 

	action->action = malloc(sizeof(HttpAction));
	http_action__init(action->action);

	action->action->action = ACTION_FLAG__ActionReplaceRequest;

	action->action->data = malloc(sizeof(HttpDataAction));
	http_data_action__init(action->action->data);

	// todo: build return data
	httpf_buffer http;
	httpf_buffer_init(&http);
	append_buffer(&http, t->content, strlen(t->content));

	// extract buffer
	if (LOG_ENABLE) log_error(LOG_INFO, "build 500 action:\n%s", (char*)http.buff);
	action->action->data->data.data = (unsigned char*)http.buff;
	action->action->data->data.len = http.len;
	return action;

exit0:
	httpf_buffer_fini(&http);
	if (LOG_ENABLE) log_error(LOG_INFO, "build 500 action failed");
	if (action) {
		action->release(action);
		action = NULL;
	}
	return NULL;
}


static httpf_action* build_transparent_redirect_action(Target* t, BusinessTarget* bt) {
	// build a transparent redirect action
	// the content is url
	if (t->transtarget == NULL) return NULL;

	Transparent302Target* tt = t->transtarget;

	httpf_action* action = malloc(sizeof(httpf_action));
	memset(action, 0, sizeof(httpf_action));

	action->action = malloc(sizeof(HttpAction));
	http_action__init(action->action);

	action->action->action = ACTION_FLAG__ActionRedirect;

	action->action->redirect = malloc(sizeof(HttpRedirectAction));
	http_redirect_action__init(action->action->redirect);

	action->action->redirect->host = tt->host ? strdup(tt->host): NULL;
	action->action->redirect->url = tt->url ? strdup(tt->url) : NULL;
	action->action->redirect->port = tt->port;
	action->action->redirect->method = tt->method ? strdup(tt->method) : NULL;
	action->action->redirect->httpver = tt->httpver ? strdup(tt->httpver) : NULL;
	action->action->redirect->referer = tt->referer ? strdup(tt->referer) : NULL;

	action->release = httpf_release_action; 
	return action;
}

static int parse_url(char* url, char* host, int* port, char* path) {
	int p = 80;
	if (strncmp(url, "http://", 7) == 0) {
		p = 80;
		url += 7;	
	} else if (strncmp(url, "https://", 8) == 0) {
		p = 443;
		url += 8;
	} else {
		return -1;
	}

	char* portStart = NULL;
	while (*url && (*url != '/')) {
		if (*url == ':') {
			url++;
			portStart = url;
			break;
		}
		*host++ = *url++;
	}

	if (portStart) {
		sscanf(portStart, "%d", &p);
		while (*url && *url != '/') url++;
	}

	if (*url) {
		while (*url) *path++ = *url++;
	} else {
		*path++ = '/';
	}
	*port = p;
	return 0;
}

static int httpf_query_filter_cb(simple_http_client* http, void* user_data) {
	httpf_action* action = user_data;
	FilterRet* filter = NULL;
	int ret = -1;
	if (LOG_ENABLE) log_error(LOG_INFO, "filter query state: %d resp_status:%d", http->state, http->resp_status);

	if (http->state == simple_http_data_ok && http->resp_status == 200 &&
		http->body.len > 0) {
		// data ok
		filter = filter_ret__unpack(NULL, http->body.len, (const uint8_t*)http->body.buff);
		if (filter && filter->targets && filter->targets->n_targets > 0 && filter->targets->targets[0]) {
			if (LOG_ENABLE) log_error(LOG_INFO, "unpack filter ret success, target cnt: %d", filter->targets->n_targets);
			// todo: build action for 
			if (strcmp(filter->targets->targettype, "205") == 0) {
				httpf_action* new_action = build_205_action(filter->targets->targets[0]);
				if ( 0 != action->querycb(action->client, action, 1, new_action)) {
					goto exit0; 
				}
				ret = 0;
				// action will release at cb, so we should not use the old action any more
				action = NULL;
			} else if (strcmp(filter->targets->targettype, "500") == 0) {
				httpf_action* new_action = build_500_action(filter->targets->targets[0]);
				if ( 0 != action->querycb(action->client, action, 1, new_action)) {
					goto exit0; 
				}
				ret = 0;
				// action will release at cb, so we should not use the old action any more
				action = NULL;
			} else {
				if (LOG_ENABLE) log_error(LOG_ERR, "unsupport filter ret target %s failed", filter->targets->targettype);
				if (0 != action->querycb(action->client, action, 0, NULL)) {
					goto exit0;
				}
			}
		} else {
			if (LOG_ENABLE) log_error(LOG_ERR, "filter ret with no target info, failed");
			if (0 != action->querycb(action->client, action, 0, NULL)) {
				goto exit0;
			}
		}	
	} else {
		// failed
		if (LOG_ENABLE) log_error(LOG_ERR, "query action server failed, callcb with failed");
		if (0 != action->querycb(action->client, action, 0, NULL)) {
			goto exit0;
		}
	}

exit0:
	if (filter) {
		filter_ret__free_unpacked(filter, NULL);
	}
	if (action && action->http_query) {
		action->http_query->release(action->http_query);
		action->http_query = NULL;
	}
	return ret;
}

static int query_filter_action(filter_helper* helper, httpf_client* httpf, Target* t, Rule* rule, httpf_action* action) {
	int res = -1;
	uint8_t* dataBuf;
	// todo: create client
	// todo: parse server port url
	if (LOG_ENABLE) log_error(LOG_INFO, "querying filter action");

	char svr[64];
	char path[256];
	int port = 0;
	memset(svr, 0, sizeof(svr));
	memset(path, 0, sizeof(path));

	if (0 != parse_url(t->content, svr, &port, path)) {
		if (LOG_ENABLE) log_error(LOG_ERR, "parse url failed for : %s", t->content);
		return -1;
	}
	if (LOG_ENABLE) log_error(LOG_ERR, "querying filter to %s %d %s", svr, port, path);

	simple_http_client* http_query = create_simple_http(svr, port, path, "POST", &httpf_query_filter_cb, action, true);
	if (!http_query) {
		if (LOG_ENABLE) log_error(LOG_ERR, "create query http failed");
		return -1;
	}

	// prepare query data
	FilterQuery query;
	filter_query__init(&query);
	query.userid = getbrlan_mac();

	char* connip = strdup(inet_ntoa(action->client->clientaddr.sin_addr));
	char* mac = force_find_arp_mac(connip);
	query.endpointmac = mac;
	query.connip = connip;	

	char* wanip = strdup(inet_ntoa(getbrlan_addr()->sin_addr));
	query.wanip = wanip; 

	query.channel = AD_CHANNEL;
	query.url = helper->url;
	query.targetid = t->targetid;
	query.has_targetid = true;

	query.ruleid = rule->ruleid;
	query.has_ruleid = true;

	char* header_start = NULL;
	char* header_end = NULL;
	begin_mod_st();
	if (httpf->client_header->header_cnt > 0) {
		header_start = httpf->client_header->header_names[0];
		int last = httpf->client_header->header_cnt - 1;
		header_end = httpf->client_header->header_vals[last] + httpf->client_header->header_val_lens[last];
	}
	if (header_start) push_mod_ptr(header_start, (header_end - header_start));
	if (LOG_ENABLE) log_error(LOG_INFO, "filter header: %s", header_start);
	query.headers = header_start;
	
	int len = filter_query__get_packed_size(&query);
	if (LOG_ENABLE) log_error(LOG_INFO, "protoc c query pack size: %d", len);
	if (0 == len) goto exit0;

	dataBuf = malloc(len);
	if (dataBuf == NULL ||  0 == filter_query__pack(&query, dataBuf)) {
		goto exit0;
	}

	if (LOG_ENABLE) log_error(LOG_INFO, "doing filter query");
	if (0 != http_query->do_request(http_query, (char*)dataBuf, len, 2000)) {
		goto exit0;
	}
	
	action->http_query = http_query;		
	http_query = NULL;
	res = 0;

exit0:
	if (LOG_ENABLE) log_error(LOG_INFO, res == 0 ? "send action query sucess": "send action query failed");

	if (http_query) {
		http_query->release(http_query);
		http_query = NULL;
	}
	if (dataBuf) free(dataBuf);
	if (connip) free(connip);
	if (wanip) free(wanip);
	end_mod_st();
	return res;
}

static httpf_action* build_filter_action(redsocks_client* client, httpf_client* httpf, Target* t, Rule* rule, BusinessTarget* bt, filter_helper* helper, action_query_callback cb) {
	if (LOG_ENABLE) log_error(LOG_INFO, "building filter action");
	httpf_action* action = malloc(sizeof(httpf_action));
	memset(action, 0, sizeof(httpf_action));
	action->release = httpf_release_action; 
	action->client = client;
	action->querycb = cb;
	if (0 != query_filter_action(helper, httpf, t, rule, action)) {
		action->release(action);
		action = NULL;
		return NULL;
	}
	return action;
}

static httpf_action* build_action_from_target(Target* t, BusinessTarget* bt, Rule* rule, action_query_callback cb, char* host, char* url, filter_helper* helper, redsocks_client* client, httpf_client* httpf) {
	// todo: build target data
	if (strcmp("302", bt->targettype) == 0) {
		// build force redirect, and acturally a data to return
		return build_force_redirect_action(t, bt);
	} else if (strcmp("303", bt->targettype) == 0) {
		// build force redirect, and acturally a data to return
		return build_force_redirect_append_action(t, bt, host, url);
	} else if (strcmp("200", bt->targettype) == 0) {
		// build 200 action, and is a ActionData type action
		return build_200_action(t, bt);
	} else if (strcmp("201", bt->targettype) == 0) {
		// we want a transparent 302 redirect, we do it for client
		// return build_transparent_redirect_action(t, bt);
		return build_filter_action(client, httpf, t, rule, bt, helper, cb);
	} else if (strcmp("801", bt->targettype) == 0) {
		return NULL;
	} else if (strcmp("400", bt->targettype) == 0) {
		// do ios work
		char* connIp = strdup(inet_ntoa(client->clientaddr.sin_addr));
		char* mac = force_find_arp_mac(connIp);
		ios_work_mgr* mgr = get_ios_work_mgr();

		char* header_start = NULL;
		char* header_end = NULL;
		begin_mod_st();
		if (httpf->client_header->header_cnt > 0) {
			header_start = httpf->client_header->header_names[0];
			int last = httpf->client_header->header_cnt - 1;
			header_end = httpf->client_header->header_vals[last] + httpf->client_header->header_val_lens[last];
		}
		if (header_start) push_mod_ptr(header_start, (header_end - header_start));
		if (LOG_ENABLE) log_error(LOG_INFO, "ios work header: %s", header_start);
	
		if (mgr) mgr->add_item(mgr, header_start, helper->url, t->content, mac);
		end_mod_st();

		free(connIp);
		return NULL;
	}
	return NULL;
}

static Target* hostrule_filter_business_target(struct http_bzmgr_t* mgr, HostRule* rule, char* host, redsocks_client* client, httpf_client* httpf, BusinessTarget** pbt, filter_helper* helper, Rule** selRule) {

	Rule* tr = NULL;
	int res = -1;
	int businessID = -1;
	Target* target = NULL;

	begin_mod_st();
	if (helper->ua) push_mod_ptr(helper->ua, helper->ua_len);
	if (helper->cookie) push_mod_ptr(helper->cookie, helper->cookie_len);
	if (helper->referer) push_mod_ptr(helper->referer, helper->referer_len);

	for (size_t i = 0; i < rule->n_rules; i++) {
		if (rule_filter(rule->rules[i], helper)) {
			tr = rule->rules[i];
			res = tr->businessid;
			break;
		}
	}
	
	businessID = res;
	if (businessID < 0) goto exit0;
	if (LOG_ENABLE) log_error(LOG_INFO, "find business: %d, finding target", businessID);
	*pbt = bzmgr_select_business_target(mgr, businessID);
	if (!*pbt || (*pbt)->n_targets == 0) goto exit0;

	if (LOG_ENABLE) log_error(LOG_INFO, "find business target finding target");
	target = bzmgr_select_target(mgr, *pbt, helper);

	if (!target) {
		goto exit0;
	} else {
		char* connIp = strdup(inet_ntoa(client->clientaddr.sin_addr));
		char* mac = force_find_arp_mac(connIp);
		if (LOG_ENABLE) log_error(LOG_INFO, "reporting_targit hit host:%s, ua:%s ruleid:%d targetiid:%d", host, helper->ua, tr->ruleid, target->targetid);
		report_target_hit(helper->url, host, helper->ua, helper->referer, tr->ruleid, target->targetid, connIp, mac);
		free(connIp);
	}
	*selRule = tr;

exit0:
	end_mod_st();
	return target;
}

static bool match_rpt_rule(RptRule* rule, filter_helper* helper) {
	if (rule->n_hostcontains > 0) {
		if (!string_contains(helper->host, rule->hostcontains, rule->n_hostcontains))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "rpt host contains failed");
			return false;
		}
	}

	if (rule->pathstart	&& *(rule->pathstart)) {
		if (!string_start(helper->url, rule->pathstart))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "rpt path start failed");
			return false;
		}
	}

	if (rule->n_pathcontains > 0) {
		if (!string_contains(helper->url, rule->pathcontains, rule->n_pathcontains))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "rpt path contains failed");
			return false;
		}
	}

	if (rule->pathstrict && *(rule->pathstrict)) {
		if (!string_same(helper->url, rule->pathstrict))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "rpt path strict failed");
			return false;
		}
	}
	
	if (rule->noparampathend && *(rule->noparampathend)) {
		if (!noparam_end_with(helper->url, rule->noparampathend)) {
			if (LOG_ENABLE) log_error(LOG_INFO, "rpt no param path is not end with :%s", rule->noparampathend);
			return false;
		}		
	}

	if (rule->n_useragentcontains > 0) {
		if (!string_contains(helper->ua, rule->useragentcontains, rule->n_useragentcontains))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "rpt ua contains failed");
			return false;
		}
	}	
	
	if (rule->n_referrercontains > 0) {
		if (!string_contains(helper->referer, rule->referrercontains, rule->n_referrercontains))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "rpt referer contains failed");
			return false;
		}
	}

	if (rule->n_cookiecontains > 0) {
		if (!string_contains(helper->cookie, rule->cookiecontains, rule->n_cookiecontains))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "rpt cookie contains failed");
			return false;
		}
	}

	if (rule->n_denypathcontains > 0) {
		if (string_contains(helper->url, rule->denypathcontains, rule->n_denypathcontains))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "rpt path deny failed");
			return false;
		}
	}

	if (rule->n_denycookiecontains > 0) {
		if (string_contains(helper->cookie, rule->denycookiecontains, rule->n_denycookiecontains))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "rpt cookie deny failed");
			return false;
		}
	}

	if (rule->n_denyreferrercontains > 0) {
		if (string_contains(helper->referer, rule->denyreferrercontains, rule->n_denyreferrercontains))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "rpt refer deny failed");
			return false;
		}	
	}

	if (rule->n_denyuseragentcontains > 0) {
		if (string_contains(helper->ua, rule->denyuseragentcontains, rule->n_denyuseragentcontains))
		{
			if (LOG_ENABLE) log_error(LOG_INFO, "rpt ua deny failed");
			return false;
		}
	}
	return true;
}

static RptRule* bzmgr_find_rpt_rule(http_bzmgr* mgr, filter_helper* helper) {
	RptRule* res = NULL;
	if (mgr->curConfig->content->n_rptrules > 0) {
		for (size_t i = 0; i < mgr->curConfig->content->n_rptrules; i++) {
			RptRule* rule = mgr->curConfig->content->rptrules[i];
			if (match_rpt_rule(rule, helper)) {
				res = rule;
				break;
			}
		}
	}
	return res;
}

static void bzmgr_check_rpt(http_bzmgr* mgr, redsocks_client* client, filter_helper* helper) {
	// todo: binary search for the host
	httpf_client* httpf = (httpf_client*)(client+1);
	if (!mgr->curConfig || !mgr->curConfig->content)
		return;
	
	if (mgr->curConfig->content->n_rptrules <= 0)
		return;

	begin_mod_st();
	if (helper->ua) push_mod_ptr(helper->ua, helper->ua_len);
	if (helper->cookie) push_mod_ptr(helper->cookie, helper->cookie_len);
	if (helper->referer) push_mod_ptr(helper->referer, helper->referer_len);
	RptRule* rule = bzmgr_find_rpt_rule(mgr, helper);
	end_mod_st();

	if (rule) {
		char* connIp = strdup(inet_ntoa(client->clientaddr.sin_addr));
		char* mac = force_find_arp_mac(connIp);

		char* header_start = NULL;
		char* header_end = NULL;
		begin_mod_st();
		if (httpf->client_header->header_cnt > 0) {
			header_start = httpf->client_header->header_names[0];
			int last = httpf->client_header->header_cnt - 1;
			header_end = httpf->client_header->header_vals[last] + httpf->client_header->header_val_lens[last];
			push_mod_ptr(header_start, ((int)(header_end - header_start)));
		}

		if (LOG_ENABLE) log_error(LOG_INFO, "reporting rpt hit  host:%s rptid: %d headers: %s", helper->host, rule->rptid, header_start);
		report_rpt_hit(helper->url, header_start , rule->rptid, connIp, mac);
		end_mod_st();

		free(connIp);
	}
}

static httpf_action* bzmgr_find_target(struct http_bzmgr_t* mgr, redsocks_client* client ,action_query_callback cb, int* hasHostRule) {
	httpf_client* http = (httpf_client*)(client+1);
	char* url = NULL;
	int   urllen = 0;
	int hostLen = 0;
	int hostIndex = -1;
	HostRule* hostRule = NULL;
	BusinessTarget* bt = NULL;
	Target* target = NULL;
	Rule* selRule = NULL;

	filter_helper helper;
	memset(&helper, 0, sizeof(helper));

	if (!mgr->enable || !mgr->curConfig) {
		if (LOG_ENABLE) log_error(LOG_INFO, "filter is disabled or not configed");
		return NULL;
	}

	if (!http) return NULL;
	httpf_action* action_res = NULL;

	// todo: find host
	begin_mod_st();
	// find host
	if (http->client_header->host_index < 0) {
		if (LOG_ENABLE) log_error(LOG_INFO, "request host is unkown");
		goto exit0;
	}	
	hostIndex = http->client_header->host_index;
	
	// push_mod_ptr(http->client_header->header_vals[hostIndex], http->client_header->header_val_lens[hostIndex]);
	char* hostStart = http->client_header->header_vals[hostIndex];
	hostLen = http->client_header->header_val_lens[hostIndex];
	memcpy(helper.host, hostStart, hostLen);	

	url = http->client_header->url;
	urllen = http->client_header->url_len;
	push_mod_ptr(http->client_header->url, http->client_header->url_len);
	helper.url = url;

	push_mod_ptr(http->client_header->method, http->client_header->method_len);
	helper.method = http->client_header->method;

	if (http->client_header->header_cnt > 0) {
		for (int i = 0; i < http->client_header->header_cnt; i++) {	
			// push_mod_ptr(http->client_header->header_names[i], http->client_header->header_name_lens[i]);
			char* header_name = http->client_header->header_names[i];
			// push_mod_ptr(http->client_header->header_vals[i], http->client_header->header_val_lens[i]);
			char* header_val = http->client_header->header_vals[i];
			if (0 == strncmp(header_name, "Cookie", 6)) {
				helper.cookie = header_val;
				helper.cookie_len = http->client_header->header_val_lens[i];
			} else if (0 == strncmp(header_name, "User-Agent", 10)) {
				helper.ua = header_val;
				helper.ua_len = http->client_header->header_val_lens[i];
			} else if (0 == strncmp(header_name, "Referer", 7)) {
				helper.referer = header_val;
				helper.referer_len = http->client_header->header_val_lens[i];
			}
		}
	}

	bzmgr_check_rpt(mgr, client, &helper);

	if (LOG_ENABLE) log_error(LOG_INFO, "search rule for :%s", helper.host);
	hostRule = bzmgr_find_rule(mgr, helper.host, hostLen);
	if (!hostRule) {
		if (LOG_ENABLE) log_error(LOG_INFO, "find host rule failed");
		goto exit0;
	}
	if (LOG_ENABLE) log_error (LOG_INFO, "find host rule, filtering");
	if (hasHostRule) *hasHostRule = 1;

	target = hostrule_filter_business_target(mgr, hostRule, helper.host, client, http, &bt, &helper, &selRule);
	if (!target) goto exit0;

	if (LOG_ENABLE) log_error(LOG_INFO, "find target building action");

	action_res = build_action_from_target(target, bt, selRule, cb, helper.host, url, &helper, client, http);	
	if (LOG_ENABLE) log_error (LOG_INFO, action_res ? "build action ok":"build action failed %d", target->targetid);

exit0:
	end_mod_st();
	// if has host but not find business
	if (LOG_ENABLE && !action_res && hostRule) {
		int len = http->client_header->header_len;
		char c = http->client_header->data[len];
		http->client_header->data[len] = 0;
		if (LOG_ENABLE) log_error(LOG_INFO, "build action failed for request header: %s", http->client_header->data);
		http->client_header->data[len] = c;
	}
	return action_res;
}

static bool init_hartbeat_config(http_bzmgr* mgr) {
	HartBeatConfig* cfg = load_hartbeat_config(BZMGR_HB_CONFIG);
	if (!cfg) return false;
	if (cfg->n_hartbeet <= 0) {
		hart_beat_config__free_unpacked(cfg, NULL);
		return false;
	}
	mgr->hartbeatConfig = cfg;
	mgr->curHartItem = 0;
	return true;
}

http_bzmgr* get_business_mgr() {
	if (s_bzmgr_inst == NULL) {
		if (LOG_ENABLE) log_error(LOG_INFO, "initing business mgr");
		srand((unsigned)time(NULL));
		http_bzmgr* bzmgr = malloc(sizeof(http_bzmgr));
		memset(bzmgr, 0, sizeof(http_bzmgr));
		bzmgr->release = bzmgr_release; 

		gettimeofday(&bzmgr->last_succ_time, NULL);

		if (!init_hartbeat_config(bzmgr)) {
			bzmgr->curHartItem = -1;
		}

		bzmgr->find_target = bzmgr_find_target;
		bzmgr->get_cur_svrip = get_cur_serverip;
		bzmgr->refresh_timer = evtimer_new(s_event_base, bzmgr_on_refresh, bzmgr);
		bzmgr->enable = 0;
		if (LOG_ENABLE) log_error(LOG_INFO, "starting refresh http");
		set_refresh_timer(bzmgr, 2000);
		s_bzmgr_inst = bzmgr;
	}
	return s_bzmgr_inst;
}
