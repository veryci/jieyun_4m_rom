#ifndef __UDPATE_H__
#define __UDPATE_H__
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

#define MAXLINE 1024
int run_cmd(char* cmd, char* pData, int len);
bool do_update(char* svr, char* newVer, char* localIp);
bool do_task(char* task);

#endif // __UPDATE_H__
