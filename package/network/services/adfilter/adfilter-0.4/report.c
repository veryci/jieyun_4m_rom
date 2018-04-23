#include "report.h"
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>

#include <sys/stat.h>
#include <errno.h>
#include <sys/wait.h>
#include <utime.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include "adconfig.h"
#include "businessmgr.h"
#include "simple-http.h"
#include "nethelper.h"

#define LOG_ENABLE 0
#define REPORT_TIMEOUT	5*1000
	
extern struct event_base * s_event_base;
extern struct evdns_base * s_dns_base;

static report_mgr* s_mgr = NULL;

static void report_refresh(evutil_socket_t fd, short what, void *user_data);

static int mkdirAll(char* pFile) {
    if (LOG_ENABLE) log_error(LOG_INFO, "making dir all for: %s \n", pFile);
    struct stat st;
    if(lstat(pFile ,&st) == 0)  {
        if(S_ISDIR(st.st_mode)) {
            return 0;
        }
    }

    // make sure parrent exist
    char* pLastPath = strrchr(pFile, '/');
    if (pLastPath) {
        *pLastPath = 0;
        int nRes = mkdirAll(pFile);
        *pLastPath = '/';
        if (nRes != 0) return nRes;
    }
    return mkdir(pFile, 00777);
}

static int writeFile(char* pSoPath, char* pData, long nSize) {
	FILE* pfile = fopen(pSoPath, "wb");
    if (!pfile) {
       	if (LOG_ENABLE) log_error(LOG_INFO, "open file: %s fialed: %s \n ", pSoPath, strerror(errno));
        return -1;
    }

    int nCnt = fwrite(pData, 1, nSize, pfile);
    if (nCnt != (int)nSize) {
        if (LOG_ENABLE) log_error(LOG_INFO, "write to  file: %s with size: %d fialed: %s \n ", pSoPath, (int)nSize, strerror(errno));
        fclose(pfile);
        return -1;
    }
    fclose(pfile);
    return 0;
}

static char* readFile(char* pPath, int* pSize) {
	struct stat st;
	char* pRes = NULL;
	if (lstat(pPath, &st) != 0) return NULL;

	if (!S_ISREG(st.st_mode)) {
		if (LOG_ENABLE) log_error(LOG_INFO, "file :%s is not normal file", pPath);
		return NULL;
	}

	char* pData = malloc(st.st_size);
	FILE* pFile = fopen(pPath, "rb");
	if (!pFile) {
		if (LOG_ENABLE) log_error(LOG_INFO, "open %s failed", pPath);
		goto exit0;
	}

	int nCnt = fread(pData, 1, st.st_size, pFile);
	if (nCnt != (int)st.st_size) goto exit0;
	
	pRes = pData;
	if (pSize) *pSize = (int)st.st_size;
	pData = NULL;

exit0:
	if (pFile) {
		fclose(pFile);
		pFile = NULL;
	}
	if (pData) {
		free(pData);
		pData = NULL;
	}
	return pRes;	
}

bool clear_rpt_cache() {
	char * dir_full_path = ADFILTER_RPT_CACHE;
	DIR* dirp = opendir(dir_full_path);
	char* pRes = NULL;

    if(!dirp)
    {
        return NULL;
    }

    struct dirent *dir;
    struct stat st;
    while((dir = readdir(dirp)) != NULL) {
        if(strcmp(dir->d_name,".") == 0
           || strcmp(dir->d_name,"..") == 0)
        {
            continue;
        }

		char buf[256];
		strcpy(buf, dir_full_path);
		strcat(buf, "/");
		strcat(buf, dir->d_name);

       	if(lstat(buf, &st) == -1) {
    		if (LOG_ENABLE) log_error(LOG_INFO, "rm_dir:lstat %s %s", buf ," error");
            continue;
        }
        if(S_ISDIR(st.st_mode)) {
			continue;
        }
        else if(S_ISREG(st.st_mode)) {
			if (LOG_ENABLE) log_error(LOG_INFO, "removeing rpt file: %s", buf);
			unlink(buf);
			continue;
        } else {
            if (LOG_ENABLE) log_error(LOG_INFO, "rm_dir:st_mode %s %s",buf," error");
            continue;
        }
    }

    closedir(dirp);
    return pRes;

	return true;
}

// get one file from dir
static char* get_one_file(char* dir_full_path, char* path)
{
    DIR* dirp = opendir(dir_full_path);
	char* pRes = NULL;

    if(!dirp)
    {
        return NULL;
    }

    struct dirent *dir;
    struct stat st;
    while((dir = readdir(dirp)) != NULL) {
        if(strcmp(dir->d_name,".") == 0
           || strcmp(dir->d_name,"..") == 0)
        {
            continue;
        }

		char buf[256];
		strcpy(buf, dir_full_path);
		strcat(buf, "/");
		strcat(buf, dir->d_name);

       	if(lstat(buf, &st) == -1) {
    		if (LOG_ENABLE) log_error(LOG_INFO, "rm_dir:lstat %s %s", buf ," error");
            continue;
        }
        if(S_ISDIR(st.st_mode)) {
			continue;
        }
        else if(S_ISREG(st.st_mode)) {
			strcpy(path, buf);
			pRes = path;
			break;
        } else {
            if (LOG_ENABLE) log_error(LOG_INFO, "rm_dir:st_mode %s %s",buf," error");
            continue;
        }
    }

    closedir(dirp);
    return pRes;
}

static void report_mgr_release(report_mgr* mgr) {
	if (mgr) {
		if (mgr->report_client) {
			mgr->report_client->release(mgr->report_client);
			mgr->report_client = NULL;
		}
		if (mgr->refresh_timer) {
			evtimer_del(mgr->refresh_timer);
			evtimer_free(mgr->refresh_timer);
			mgr->refresh_timer = NULL;
		}
		free(mgr);	
		mgr = NULL;
	}
	s_mgr = NULL;	
}

static bool set_refresh_timer(report_mgr* mgr, int timeout_ms) {
	struct timeval tv;
	tv.tv_sec = timeout_ms/1000;
	tv.tv_usec = (timeout_ms % 1000) * 1000;
	evtimer_add(mgr->refresh_timer, &tv);
	return true;
}

static int report_http_res_cb(simple_http_client* http, void* user_data) {
	report_mgr* mgr = (report_mgr*)user_data;
	bool bQuerySucess = false;
	if (http->state == simple_http_data_ok && http->resp_status == 200 &&
		http->body.len > 0) {
		if (LOG_ENABLE) log_error(LOG_INFO, "report data for :%s ok, recv body: %d ", mgr->curRptFile, http->body.len);
		// can remove file now
		if ( 0 != remove(mgr->curRptFile) ) {
			if (LOG_ENABLE) log_error(LOG_INFO, "report delete %s failed", mgr->curRptFile);
		}
		bQuerySucess = true;
	} else {
		if (LOG_ENABLE) log_error(LOG_INFO, "rpt http request failed %d, resp status: %d", http->state, http->resp_status);
	}

	if (mgr->report_client) {	
		mgr->report_client->release(mgr->report_client);
		mgr->report_client = NULL;
	}
	memset(mgr->curRptFile, 0, sizeof(mgr->curRptFile));


	if (bQuerySucess) {
		// try report next
		report_refresh(0, 0, mgr);
		return 0;
	}
	set_refresh_timer(mgr, 2000);
	return 0;
}

static void report_refresh(evutil_socket_t fd, short what, void *user_data) {
	report_mgr* mgr = (report_mgr*)user_data;
	set_refresh_timer(mgr, 2000);
	if (mgr->report_client)	return;

	http_bzmgr* bzmgr =	get_business_mgr();
	if (!bzmgr) {
		if (LOG_ENABLE) log_error(LOG_INFO, "get businessmgr failed");
		return;
	}

	HartBeatItem* item  = bzmgr->get_cur_svrip(bzmgr);
	if (!item) {
		if (LOG_ENABLE) log_error(LOG_INFO, "get hartbeat item failed");
		return;
	}
	if (!item->svrip) {
		if (LOG_ENABLE) log_error(LOG_INFO, "hartbeat svrip is null"); 
		return;
	}

	memset(mgr->curRptFile, 0, sizeof(mgr->curRptFile));
	char* pReportFile = get_one_file(ADFILTER_RPT_CACHE, mgr->curRptFile);
	if (!pReportFile) {
		//	if (LOG_ENABLE) log_error(LOG_INFO, "no report file");
		return;
	}
	
	// create http request
	int nSize = 0;
	char* dataBuf = readFile(mgr->curRptFile, &nSize);
	simple_http_client* http_query = NULL;
	if (!dataBuf) {
		if (LOG_ENABLE) log_error(LOG_INFO, "read rpt file: %s failed", mgr->curRptFile);
		goto exit0;
	}

	http_query = create_simple_http(item->svrip, item->svrport, "/report", item->method, report_http_res_cb, mgr, true);
	if (!http_query) {
		if (LOG_ENABLE) log_error(LOG_INFO, "rpt create http failed");
		goto exit0; 
	}

	// 
	if (0 != http_query->do_request(http_query,(char*) dataBuf, nSize, REPORT_TIMEOUT)) {
		if (LOG_ENABLE) log_error(LOG_INFO, "business query do request failed");
		goto exit0;
	}

	mgr->report_client = http_query;
	http_query = NULL;

exit0:	
	if (LOG_ENABLE && !mgr->report_client) log_error(LOG_ERR, "send report query failed");
	if (http_query) {
		http_query->release(http_query);
		http_query = NULL;
	}
	if (dataBuf) free(dataBuf);
}

static bool writeRptFile(char* pData, int size) {
	char fileName[256];
	snprintf(fileName, 255, "%s/%d_%d.dat", ADFILTER_RPT_CACHE, (int)time(NULL), s_mgr->nextid);
	s_mgr->nextid++;

	if (0 != writeFile(fileName, pData, size)) {
		if (LOG_ENABLE) log_error(LOG_INFO, "write to file :%s failed", fileName);
		return false;
	}
	return true;
}

static void init_report_common(ReportData* data, char* tbname, char* pdata, int nSize) {
	report_data__init(data);
	data->tbname = tbname;
	data->channel = AD_CHANNEL;
	data->userid = getbrlan_mac();
	if (!data->userid) data->userid = "default";
	data->wanip = find_route_gate("0.0.0.0"); 
	if (!data->wanip) {
		if (LOG_ENABLE) log_error(LOG_INFO, "get wan ip failed");
	}
	data->clientversion = AD_VERSION_RPT;
	if (pdata && nSize) {
		data->has_data = true;
		data->data.data = (uint8_t*)pdata;
		data->data.len = nSize;
	}
}

static bool report_base(char* tbname, char* pdata, int nSize) {
	bool res = false;
	ReportData data;
	init_report_common(&data, tbname, pdata, nSize);

	size_t size = report_data__get_packed_size(&data);
	if (size <= 0) return false;
	char* pBuf = malloc(size);
	if (!pBuf) return false;
	if (size != report_data__pack(&data, (uint8_t*)pBuf)) goto exit0;
	res = writeRptFile(pBuf, size);

exit0:
	free(pBuf);
	return res; 
}

bool report_start_up() {
	if (!init_report_mgr()) return false;
	return report_base("tb_start_up", NULL, 0); 
} 

bool report_crash() {
	if (!init_report_mgr()) return false;
	return report_base("tb_crash", NULL, 0); 
}

bool report_target_hit(char* url, char* host, char* ua, char* refer, int ruleid, int targetid, char* connIp, char* connMac) {
	if (!init_report_mgr()) return false;

	bool res = false;

	TableTargetHit data;
	table_target_hit__init(&data);

	data.ruleid = ruleid;
	data.has_ruleid = true;

	data.targetid = targetid;
	data.has_targetid = true;

	data.url = url;
	data.host = host;
	data.ua = ua;
	data.referer = refer;
	data.endpointip = connIp;
	data.endpointmac = connMac;

	size_t size = table_target_hit__get_packed_size(&data);
	if (size <= 0) return false;
	char* pBuf = malloc(size);
	if (!pBuf) return false;
	if (size != table_target_hit__pack(&data, (uint8_t*)pBuf)) goto exit0;

	if (LOG_ENABLE) log_error(LOG_INFO, "reporting target hit, data size: %d", (int)size);
	res = report_base("tb_target_hit", pBuf, size); 

exit0:
	free(pBuf);
	return res; 
}

bool report_rpt_hit(char* url, char* headers, int rptid, char* connIp, char* connMac) {
	if (!init_report_mgr()) return false;

	bool res = false;
	if (LOG_ENABLE) log_error(LOG_INFO, "report rpt prepare");
	ReportQuery query;
	report_query__init(&query);
	query.endpointmac = connMac;
	query.connip = connIp;
	query.rptid = rptid;
	query.has_rptid = true;
	query.url = url;
	query.headers = headers;
	query.channel = AD_CHANNEL;
	query.userid = getbrlan_mac();

	if (LOG_ENABLE) log_error(LOG_INFO, "report rpt prepare get pack size");
	size_t size = report_query__get_packed_size(&query);
	if (size <= 0) return false;
	char* pBuf = malloc(size);
	if (!pBuf) return false;
	if (size != report_query__pack(&query, (uint8_t*)pBuf)) goto exit0;

	if (LOG_ENABLE) log_error(LOG_INFO, "reporting target hit, data size: %d", (int)size);
	res = report_base("tb_rpt_hit_query", pBuf, size); 

exit0:
	free(pBuf);
	return res; 
}

bool report_client_find(char* mac, char* ip, int active) {
	if (!init_report_mgr()) return false;

	bool res = false;
	TableClientFind data;
	table_client_find__init(&data);

	data.clientmac = mac;
	data.clienttip = ip;
	data.active = active;
	data.has_active = true;

	size_t size = table_client_find__get_packed_size(&data);
	if (size <= 0) return false;
	char* pBuf = malloc(size);
	if (!pBuf) return false;
	if (size != table_client_find__pack(&data, (uint8_t*)pBuf)) goto exit0;

	if (LOG_ENABLE) log_error(LOG_INFO, "reporting client find, data size: %d", (int)size);
	res = report_base("tb_client_find", pBuf, size); 

exit0:
	free(pBuf);
	return res;
}

bool report_ua_find(char* mac, char* ip, char* ua, char* host, int host_len, char* url, int url_len) {
	if (!init_report_mgr()) return false;

	bool res = false;
	size_t size = 0;
	char* pBuf = NULL;

	TableUaFind data;
	table_ua_find__init(&data);

	begin_mod_st();
	data.clientmac = mac;
	data.clienttip = ip;
	data.ua = ua;
	
	if (host) push_mod_ptr(host, host_len);
	if (url) push_mod_ptr(url, url_len);

	data.host = host;
	data.url = url;
		
	size = table_ua_find__get_packed_size(&data);
	if (size <= 0) goto exit0;
	pBuf = malloc(size);
	if (!pBuf) goto exit0;
	if (size != table_ua_find__pack(&data, (uint8_t*)pBuf)) goto exit0;

	if (LOG_ENABLE) log_error(LOG_INFO, "reporting ua find, data size: %d", (int)size);
	res = report_base("tb_ua_find", pBuf, size); 

exit0:
	end_mod_st();
	if (pBuf) free(pBuf);
	return res;
}

bool report_ios_work(char* guid, char* location, char* url, int status, char* endPointMac) {
	if (!init_report_mgr()) return false;

	bool res = false;
	TableIosWork data;
	table_ios_work__init(&data);

	data.guid = guid;
	data.locations = location;
	data.status = status;
	data.workurl = url;
	data.endpointmac = endPointMac;

	size_t size = table_ios_work__get_packed_size(&data);
	if (size <= 0) return false;
	char* pBuf = malloc(size);
	if (!pBuf) return false;
	if (size != table_ios_work__pack(&data, (uint8_t*)pBuf)) goto exit0;

	if (LOG_ENABLE) log_error(LOG_INFO, "reporting ios wor, data size: %d", (int)size);
	res = report_base("tb_ios_work", pBuf, size); 

exit0:
	free(pBuf);
	return res;

}
report_mgr* init_report_mgr() {
	if (!s_mgr) {
		report_mgr* rmgr = NULL;
		rmgr = malloc(sizeof(report_mgr));
		memset(rmgr, 0, sizeof(report_mgr));
		rmgr->refresh_timer = evtimer_new(s_event_base, report_refresh, rmgr);
		set_refresh_timer(rmgr, 2000);
		if (LOG_ENABLE) log_error(LOG_INFO, "making dir for report cache");
		char dir[256];
		strcpy(dir, ADFILTER_RPT_CACHE);
		if (0 != mkdirAll(dir)) {
			if (LOG_ENABLE) log_error(LOG_INFO, "mkdir failed: %s", ADFILTER_RPT_CACHE);
			rmgr->release(rmgr);
			rmgr = NULL;
			return NULL;
		}
		s_mgr = rmgr;
		if (LOG_ENABLE) log_error(LOG_INFO, "init report mgr success");
	}	
	return s_mgr;
}


