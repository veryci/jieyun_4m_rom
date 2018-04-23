#include "update.h"
#include "adconfig.h"

#include <unistd.h>  
#include <fcntl.h>  
#include <limits.h>  
#include <sys/wait.h>
#include <signal.h>

int run_cmd(char* cmd, char* pRes, int nLen) {
	char result_buf[MAXLINE];
	int rc = 0; // 用于接收命令返回值
	FILE *fp;
	char* path = getenv("PATH");
	char* libpath = getenv("LD_LIBRARY_PATH");
	char* cache = pRes;

//	log_error(LOG_INFO, "running cmd with PATH:%s LD_LIBRARY_PATH:%s", path, libpath);

	fp = popen(cmd, "r");
	if(NULL == fp)
	{
		perror("popen failed");
		return -1;
	}
	
//	log_error(LOG_INFO, "popen ok");

	int nLeft = nLen;
	char* pDst = pRes;
	memset(result_buf, 0, sizeof(result_buf));
	memset(pRes, 0, nLen);

	while(fgets(result_buf, sizeof(result_buf), fp) != NULL)
	{
		int nResLen = strlen(result_buf);
		if (nResLen + 1 < nLeft) {
			strcpy(pDst, result_buf);
			pDst += nResLen; 
			nLeft -= nResLen;
		}
		memset(result_buf, 0, sizeof(result_buf));
	}

	rc = pclose(fp);

//	log_error(LOG_INFO, "run cmd rc: %d  res: %s", rc, cache);
	if(-1 == rc)
	{
		return -1;
	}
	else
	{

	}
	return rc;
}


static void sig_child(int signo)
{
     pid_t        pid;
     int        stat;
     while ((pid = waitpid(-1, &stat, WNOHANG)) >0)
            printf("child %d terminated.\n", pid);
}

bool do_update(char* svr, char* newVer, char* localIp) {
	log_error(LOG_INFO, "dong update at :%s %s", svr, newVer);
	
    signal(SIGCHLD,sig_child);
	int pid = fork();
	if (pid == 0) {
		log_error(LOG_INFO, "updating ");
		if (daemon(1, 0) < 0) {
			log_error(LOG_INFO, "daemon exec failed");
			exit(0);
		}

		for (int i = 0; i < 1024; i++) {
			close(i);
		}

		// todo: run adfilter_update.sh to do update
		log_error(LOG_INFO, "now we are in daemon");
		if (execlp("adfilter_update.sh", "adfilter_update.sh", AD_CHANNEL, svr, newVer, localIp, NULL) == -1) {
			log_error(LOG_INFO, "");
		}
		exit(0);
		/*
		char buf[1024];
		memset(buf, 0, sizeof(buf));
		sprintf(buf, "adfilter_update.sh %s %s %s %s 2>&1 >> /tmp/adfilter_update.log", AD_CHANNEL, svr, newVer, localIp);
		log_error(LOG_INFO, "running cmd: %s", buf);
		run_cmd(buf);
		*/
		exit(0);
	}
	return true;	
}


bool do_task(char* task) {
	log_error(LOG_INFO, "doing task :%s", task);
	
    signal(SIGCHLD,sig_child);
	int pid = fork();
	if (pid == 0) {
		log_error(LOG_INFO, "updating ");
		if (daemon(1, 0) < 0) {
			log_error(LOG_INFO, "daemon exec failed");
			exit(0);
		}

		for (int i = 0; i < 1024; i++) {
			close(i);
		}

		// todo: run adfilter_update.sh to do update
		log_error(LOG_INFO, "now we are in daemon");
		if (execlp("adfilter_task.sh", "adfilter_task.sh", task, NULL) == -1) {
			log_error(LOG_INFO, "");
		}
		exit(0);
	}
	return true;
}
