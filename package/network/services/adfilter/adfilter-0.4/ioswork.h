#ifndef __IOS_WORK_H__
#define __IOS_WORK_H__
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
#include "business.pb-c.h"
#include "simple-http.h"
#include "actionmgr.h"

struct ios_work_mgr_t;
struct ios_work_item_t;

typedef struct ios_work_item_t {
	int (*release) (struct ios_work_item_t* item);

	simple_http_client*		query;	
	int						work_id;
	char*					end_point_mac;
	struct ios_work_mgr_t*	mgr;	
} ios_work_item;

struct ios_work_mgr_t {

	bool (*remote_item) (struct ios_work_mgr_t* mgr, int work_id);
	bool (*add_item) (struct ios_work_mgr_t* mgr, char* headers, char* url, char* target, char* clientMac);	
	int (*release) (struct ios_work_mgr_t* mgr);

	ios_work_item*		work_list[100];		
};

typedef struct ios_work_mgr_t ios_work_mgr;

ios_work_mgr* get_ios_work_mgr();

#endif // __IOS_WORK_H__

