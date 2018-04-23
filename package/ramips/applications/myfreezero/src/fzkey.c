/**
* Created by copycat on 10/13/16.
*
*
*/

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include<sys/time.h> 
#include<unistd.h> 

char *HEADER={"PKZIP"};

//#define RETRY_TIMES		61

#define MAX				8112349
#define IA				4343
#define IB				48804
char *PKEY = {"K302ijakls2093#$%"};  // try to hide this

unsigned long nextKey(unsigned long seed) {
	return (seed*IA+IB)%MAX;
}

unsigned long randRoll(unsigned long seed) {
	unsigned int b=60, roll, i;
	roll = (unsigned int)( rand() % b +1 );
 	for ( i = 0; i < roll; i++) {
 		seed = nextKey(seed);
 	}

	return seed;
}

//char *decrypt(int seed, char* estr)
//{
//	int i, len;
//	char tempbuf[128], key[128];
//	 unsigned char s[256] = {0}; //S-box
//
//	for (i = 0; i < RETRY_TIMES; i++) {
//		sprintf(key,"%s-%s", PKEY, seed);
//		len = strlen(key);
//
//		rc4_init(s,(unsigned char *)key, len);
//		rc4_crypt(s,(unsigned char *)estr,len);
//
//		if(strncmp(estr, (char*)HEADER, strlen(HEADER)) == 0) {
//			return estr;
//		}
//		seed = nextKey(seed);
//	}
//
//	return NULL;
//}


char *encrypt(int seed, char *str) 
{	    
	unsigned char s[1024] = {0};
	char key[512], target[512];
	int len;

	sprintf(key,"%s-%d", PKEY, seed);
	//printf("key:%s\r\n", key);
	sprintf(target, "%s%s", HEADER, str);
	//printf("target:%s\r\n", target);
	len = strlen(target);
	  
	rc4_init(s,(unsigned char *)key,strlen(key));
	rc4_crypt(s,(unsigned char *)target,len);

	//printf("s:%s\r\n", target);
	char *ret1 = base64_encode(target, len);
	//printf("encrypt64: %s\n", ret1);

#if 0
	len = strlen(ret1);
	char *ret2 = base64_decode(ret1, len);
	printf("decode64:%s\n", ret2);
	
	len = strlen(ret2);
    rc4_init(s,(unsigned char *)key, strlen(key));
    rc4_crypt(s,(unsigned char *)ret2,len);
    printf("decrypt  : %s\n",ret2);

	free(ret2);
#endif
	//free(ret1);

	return ret1;
}

char *randEncrypt(int inseed, char *str) {
    srand( (unsigned) time(NULL));
	unsigned seed = randRoll(inseed);
	return encrypt(seed, str);
}

#if 0
void main(void)
{
	char result[128];

/*
    char *t = "abcdefghijklmnopqrstuvwxyz"; 
    int i = 0; 
    int j = strlen(t); 
    char *enc = base64_encode(t, j); 
    int len = strlen(enc); 
    char *dec = base64_decode(enc, len); 
    printf("\noriginal: %s\n", t); 
    printf("\nencoded : %s\n", enc); 
    printf("\ndecoded : %s\n", dec); 
    free(enc); 
    free(dec);
*/

	encrypt(33, "kfc");
} 
#endif

