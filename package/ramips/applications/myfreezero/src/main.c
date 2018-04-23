#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
//#include <linux/netlink.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <limits.h>
#include <sys/stat.h>
//#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h>    
#include <net/if_arp.h>  
#include "md5.h"

//#define ALPHA
#ifdef ALPHA
    #define SERVER_PORT 3003
    #define REGISTER_WEB_ADDR "http://alpha.veryci.cc"
#else
    #define SERVER_PORT 80
    #define REGISTER_WEB_ADDR "https://r.freezero.cn"
#endif

#define OUTPUTFORMAT    "{\n\"r1\":\"%d\",\n\"pid\":\"%s\",\n\"mac\":\"%s\",\n\"hostmac\":\"%s\",\n\"chk\":\"%s\",\n\"r2\":\"%d\"\n}"

#define PARTERID    "585b7595e61fb8b0387d3d52"
#define CGI_NAME    "freezero.cgi"


#define ACCESSWEBURL "<!DOCTYPE html>\n<html lang=\"en\">\n<head><meta http-equiv=\"X-UA-Compatible\" content=\"IE=Edge,chrome=1\"><meta name=\"renderer\" content=\"webkit\"></head>\n<body>\n<script type=text/javascript>\n\tvar ua = navigator.userAgent;\n\tif(ua.indexOf(\"Windows NT 5\")!=-1&&!+[1,]) { \n\t\t window.location.href=\"http://r.freezero.cn/creditcard/xp.html\"\n\t} else {\n\t\tvar parseResponse = function parseResponse(res) {\n\t\t\twindow.location.replace(\"http://%s/fzcgi/%s?net=%s&seed=\" + res.s);\n\t\t}\n\t}\n</script>\n<script type=\"text/javascript\"\n\tsrc=\"%s/init?callback=parseResponse\">\n</script>\n</body>\n</html>\n\n"

#define KEY1  200
#define KEY2  4004


in_addr_t my_inet_addr(u_char *text)
{
    u_char      *p, c;
    in_addr_t    addr;
    unsigned int   octet, n, len;

    addr = 0;
    octet = 0;
    n = 0;
    len = strlen(text);

    for (p = text; p < text + len; p++) {
        c = *p;

        if (c >= '0' && c <= '9') {
            octet = octet * 10 + (c - '0');

            if (octet > 255) {
                return INADDR_NONE;
            }

            continue;
        }

        if (c == '.') {
            addr = (addr << 8) + octet;
            octet = 0;
            n++;
            continue;
        }

        return INADDR_NONE;
    }

    if (n == 3) {
        addr = (addr << 8) + octet;
        return htonl(addr);
    }

    return INADDR_NONE;
}

in_addr_t inet_resolve_host(char *host)
{
    in_addr_t            in_addr;
    int           i;
    struct hostent      *h;

    in_addr = my_inet_addr(host);

    if (in_addr == INADDR_NONE) {
        h = gethostbyname(host);
        if (h == NULL || h->h_addr_list[0] == NULL) {
            return INADDR_NONE;
        }

        for (i = 0; h->h_addr_list[i] != NULL; i++) { /* void */ }

        in_addr = *(in_addr_t *) (h->h_addr_list[0]);
    }

    return in_addr;
}


int getmac(char *devname, char *macaddr) 
{ 
    struct   ifreq   ifreq; 
    int   sock; 

    if((sock=socket(AF_INET,SOCK_DGRAM,0)) <0) 
    { 
        printf( "socket \r\n"); 
        exit(0); 
    } 

    strcpy(ifreq.ifr_name,devname); 
    if(ioctl(sock,SIOCGIFHWADDR,&ifreq) <0) 
    { 
        printf( "ioctl 123\r\n"); 
        exit(0); 
    } 
    sprintf( macaddr, "%02x%02x%02x%02x%02x%02x", 
            (unsigned   char)ifreq.ifr_hwaddr.sa_data[0], 
            (unsigned   char)ifreq.ifr_hwaddr.sa_data[1], 
            (unsigned   char)ifreq.ifr_hwaddr.sa_data[2], 
            (unsigned   char)ifreq.ifr_hwaddr.sa_data[3], 
            (unsigned   char)ifreq.ifr_hwaddr.sa_data[4], 
            (unsigned   char)ifreq.ifr_hwaddr.sa_data[5]); 
    return   0; 
} 


int sendHttpCmdResp(char *szWeb, char *request, char *resp)
{
    struct hostent *pHost = gethostbyname(szWeb);
    

    if (pHost == NULL || pHost->h_addr_list[0] == NULL) {
        printf("dns fail\r\n");
        return 0;
    }

    struct sockaddr_in  webServerAddr;
    webServerAddr.sin_family = AF_INET;
    webServerAddr.sin_port = htons(SERVER_PORT);
    webServerAddr.sin_addr.s_addr = *(in_addr_t *) (pHost->h_addr_list[0]);
    //webServerAddr.sin_addr.s_addr = inet_addr(TEST_ADDR);
 
    // 创建客户端通信socket
    int sockClient = socket(AF_INET, SOCK_STREAM, 0);

    int nRet = connect(sockClient ,(struct sockaddr*)&webServerAddr, sizeof(webServerAddr));
    if(nRet < 0)
    {
        printf("connect error\n");
        return 1;
    }
 
    nRet = send(sockClient , request, strlen(request), 0);
    if(nRet < 0)
    {
        printf("send error\n");
        return 1;
    }

    nRet = recv(sockClient ,resp, 1024 ,0);
    // 接收错误
    if(nRet < 0)
    {
        printf("recv error\n");
    }           
        
    close(sockClient);   
    //printf("ok\r\n"); 

    return nRet;
}

void replaceStr(char *str, char replaced, char *replacestr)
{
    char buffer[1024];
    char *cphead;
    char *head = str;
    int len;
    char *ptr;
    
    cphead = buffer;
    len = 0;
    while(1){
        ptr = strchr(head, replaced);
        if(ptr)
        {
            memcpy(cphead, head, ptr-head);
            cphead = cphead + (ptr-head);
            len = len + (ptr-head);
            memcpy(cphead, replacestr, strlen(replacestr));
            cphead = cphead + strlen(replacestr);
            len = len + strlen(replacestr);
            head = ptr+1;
        }else{
            memcpy(cphead, head, strlen(head));
            len = len + strlen(head);
            break;
        }
        
    }

    memcpy(str, buffer, len);

}

      
int getRemoteMac(char *netif, in_addr_t ipaddr, char *macstr)  
{  
  
    int sd;  
    struct arpreq arpreq;  
    struct sockaddr_in *sin;  
    struct in_addr ina;  
    unsigned char *hw_addr;  
    sd = socket(AF_INET, SOCK_DGRAM, 0);  
    if (sd < 0)  
    {   
        return 0;  
    }  

    memset(&arpreq, 0, sizeof(struct arpreq));  
    sin = (struct sockaddr_in *) &arpreq.arp_pa;  
    memset(sin, 0, sizeof(struct sockaddr_in));  
    sin->sin_family = AF_INET;  
    ina.s_addr = ipaddr;  
    memcpy(&sin->sin_addr, (char *)&ina, sizeof(struct in_addr));  
    strcpy(arpreq.arp_dev, netif);
    ioctl(sd, SIOCGARP, &arpreq);       
    hw_addr = (unsigned char *) arpreq.arp_ha.sa_data;  
    sprintf(macstr, "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
        hw_addr[0], hw_addr[1], hw_addr[2], hw_addr[3], hw_addr[4], hw_addr[5]);  
  return 1;  
}   


int getSubnetMask(char *ifname, in_addr_t ipaddr) 
{
    int i=0;
    int sockfd;
    struct ifconf ifconf;
    unsigned char buf[512];
    struct ifreq *ifreq;

    //初始化ifconf
    ifconf.ifc_len = 512;
    ifconf.ifc_buf = buf;

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0))<0)
    {
        perror("socket");
        exit(1);
    }  
    ioctl(sockfd, SIOCGIFCONF, &ifconf);    //获取所有接口信息
    //接下来一个一个的获取IP地址
    ifreq = (struct ifreq*)buf;  
    for(i=(ifconf.ifc_len/sizeof(struct ifreq)); i>0; i--)
    {

        //printf("name = [%s]\n", ifreq->ifr_name);
        if(((struct sockaddr_in*)&(ifreq->ifr_addr))->sin_addr.s_addr == ipaddr)
        {
            strcpy(ifname, ifreq->ifr_name);
            return 1;
        }
        ifreq++;
    }
    return 0;
} 


int getHttpParm(char *params, char *key, char *result)
{
    char *index, *hindex;
    char head[64];
    char buffer[1024];

    if(!strstr(params, key))
        return 0;

    memset(result, 0, sizeof(result));

    strcpy(buffer, params);

    while(NULL != (index = strchr(buffer, '&' ))){
        memset(head, 0, sizeof(head));
        strncpy(head, buffer, index - buffer);
        if(hindex = strstr(head, key))
        {
            strncpy(result, hindex + strlen(key), strlen(hindex)-strlen(key));
            //printf("Head:%s\r\n", result);
            return 1;
        }
    
        strncpy(buffer, index + 1, strlen(buffer)-(index-buffer) + 1);
    }

    strcpy(head, buffer);
    if(hindex = strstr(head, key))
    {
        strncpy(result, hindex + strlen(key), strlen(hindex)-strlen(key));
        //printf("Head:%s\r\n", result);
        return 1;
    }

    strncpy(buffer, index + 1, strlen(buffer)-(index-buffer) + 1);


    return 0;
}


int main(int argc, char *argv[])
{
    char szHttpRest[1024] = {0};
    char *retstr;
    char strkey[128];
    char output[32];
    char key[512];
    char macaddr[32], seedstr[32], remotemac[32];
    char netif[32], ifname[32], hostip[32];
    int status;
    int seed = KEY1*10000+KEY2; // = 2004004  [hide this]
    int nRet =  2;
    int debug = 0;
    char buffer[128];
    unsigned int num1, num2;
    in_addr_t hostipaddr, remoteipaddr;

    memset(remotemac, 0, sizeof(remotemac));
    strcpy(remotemac, "000000000000");

    if(argc == 1)
    {
        /* Parameter by Get Method */
        char *param =getenv("QUERY_STRING");
        if( param == NULL )
        {
            nRet =  2;
        } else {
            char *s = NULL;
            s=getenv("HTTP_HOST");

            if(s!=NULL){
                strcpy(hostip, s);
            }else{
                return;
            }

            if(!strstr(param, "seed="))
            {
                nRet =  2;

                memset(netif, 0, sizeof(netif));
                status = getHttpParm(param, "net=", netif);
                if(!status)
                    return;
            } else {                

                memset(netif, 0, sizeof(netif));
                status = getHttpParm(param, "net=", netif);
                if(!status)
                    return;

                memset(buffer, 0, sizeof(buffer));
                status = getHttpParm(param, "debug=", buffer);
                if(status){
                    debug = 1;
                }

                status = getHttpParm(param,"seed=", seedstr);
                if(!status){
                    return;
                }

                memset(remotemac, 0, sizeof(remotemac));
                s=getenv("REMOTE_ADDR");
                remoteipaddr = inet_addr(s);

                if(remoteipaddr == INADDR_NONE)
                {
                    strcpy(remotemac, "000000000000");
                }else{
                    getRemoteMac(netif, remoteipaddr, remotemac);
                }

                nRet =  1;
            }
        }

    } else if(argc == 2) {
        nRet =  2;

        strcpy(netif, argv[1]);
    } else if(argc == 3) {
        nRet =  1;

        strcpy(netif, argv[1]);
        strcpy(seedstr, argv[2]);
    } else if(argc == 4) {
        nRet =  1;

        strcpy(netif, argv[1]);
        strcpy(seedstr, argv[2]);
        debug = 1;
    } else{
        return 0;
    }

    if(2 == nRet)
    {
        printf("Content-Type: text/html\n\n");
        if(SERVER_PORT == 80)
            sprintf(buffer, "%s", REGISTER_WEB_ADDR);
        else
            sprintf(buffer, "%s:%d", REGISTER_WEB_ADDR, SERVER_PORT);

        sprintf(szHttpRest, ACCESSWEBURL, hostip, CGI_NAME, netif, buffer);

        printf("%s", szHttpRest);
        printf("\n");
    }

    if(1 == nRet)
    {
        memset(macaddr, 0, sizeof(macaddr));
        //printf("netif:%s\r\n", netif);
        getmac(netif, macaddr);
#if 0
        sprintf(key, "%s-%s",parterid, macaddr);
#else
/*
{
  "r1": "随机字串1",
  "pid":"产品id",
  "mac":"aabbccddeeff",
  "chk": md5(pid +  mac),
  "r2": "随机字串2"
}
*/
        memset(strkey, 0, sizeof(strkey));
        sprintf(strkey, "%s%s",PARTERID, macaddr);
//printf("part:%s\r\n", strkey);
        memset(output, 0, sizeof(output));
        md5_digest( strkey, strlen(strkey), output );

        unsigned char *tempshort = (unsigned char*)output;
        memset(strkey, 0, sizeof(strkey));
        sprintf(strkey, "%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",                 tempshort[0], tempshort[1],
                tempshort[2], tempshort[3], tempshort[4], tempshort[5], 
                tempshort[6] , tempshort[7],
                tempshort[8], tempshort[9], tempshort[10], tempshort[11],
                tempshort[12], tempshort[13], tempshort[14], tempshort[15]);

        srand( (unsigned) time(NULL));
        num1 = rand();
        if(num1 < 10000000)
            num1 = num1 + 10000000;
        num2 = rand();
        if(num2 < 10000000)
            num2 = num2 + 10000000;
        sprintf(key, OUTPUTFORMAT, num1, PARTERID, macaddr, remotemac, strkey, num2);

#endif
        //printf("k=%s\r\n", key);

        retstr = randEncrypt(atoi(seedstr), key);
//      retstr = encrypt( seed, key);

        if( SERVER_PORT == 80 ){
            sprintf(szHttpRest, "Location: %s/creditcard/reg?k=%s", REGISTER_WEB_ADDR, retstr);
        }else{
            sprintf(szHttpRest, "Location: %s:%d/creditcard/reg?k=%s", REGISTER_WEB_ADDR, SERVER_PORT, retstr); //alpha has port
        }

        replaceStr(szHttpRest, '+', "%2b");
        //replaceStr(szHttpRest, '/', "%2f");
        if(retstr)  
            free(retstr);
            
        printf("Content-Type: text/html\n");      
        if(debug == 1){
            strcat(szHttpRest, "&debug=1\n\n");
        }else{
            strcat(szHttpRest, "\n\n");
        }   
        printf ("%s", szHttpRest);
        printf("\n");

    }

    return 0;
}



