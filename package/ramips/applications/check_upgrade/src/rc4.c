#include <stdlib.h>
#include <stdio.h> 


 unsigned char* RC4(unsigned char *plaintext, unsigned int textlen, unsigned char *key, unsigned int keylen) 
 { 
 	//Variables 
 	unsigned char S[256];		//permutation table 
 	unsigned int i = 0;		//index pointer 
 	unsigned int j = 0;		//index pointer 
 	unsigned char swap;		//swap varaible 
 	unsigned int  k;		//iterator 
 	unsigned char *keystream;	//stream use to encrypt text 
 	unsigned char *ciphrotext;	//encrypted plaintext 
 
 
 	//Alloc memory 
 	keystream  = (unsigned char*)malloc(textlen); 
 	ciphrotext = (unsigned char*)malloc(textlen+1); 
 
 
 	//Key-sheduling algorithm (KSA) 
 	for (i = 0; i < 256; i++) 
 	{ 
 		S[i] = i; 
 	} 
 
 
 	for (i = 0; i < 256; i++) 
 	{ 
 		j = (j + S[i] + key[i % keylen]) % 256; 
 		swap = S[i]; 
 		S[i] = S[j]; 
 		S[j] = swap; 
 	} 
 
 
 	//Pseudo-random generation algorithm (PRGA) 
 	i = 0; 
 	j = 0; 
 
 
 	for (k = 0; k < textlen; k++) 
 	{ 
 		i = (i + 1) % 256; 
 		j = (j + S[i]) % 256; 
 
 
 		swap = S[i]; 
 		S[i] = S[j]; 
 		S[j] = swap; 
 
 
 		keystream[k] = S[(S[i] + S[j]) % 256]; 
 	} 
 
 
 	//Creating a ciphrotext 
 	for (k = 0; k < textlen; k++) 
 	{ 
 		ciphrotext[k] = plaintext[k] ^ keystream[k]; 
 	} 
 
 
 	//Free memory 
 	free(keystream); 
 
 
 	return ciphrotext; 
 } 
 
 
 
#if 0 
 int main() 
 { 
 	//TEST 
 	unsigned char *plaintext = (unsigned char*)"Plaintext"; 
 	unsigned char *key = (unsigned char*)"Key"; 
 	unsigned char *ciphertext; 
 	unsigned int plaintext_len = 9; 
 	unsigned int key_len = 3; 
	unsigned char *ptr = NULL;
 	int i; 
 
 
 	ciphertext  = RC4(plaintext, plaintext_len, key, key_len); 
 
 
 	for (i = 0; i < plaintext_len; i++) 
 	{ 
 		printf("%02X ", ciphertext[i]); 
 	} 
 	
	printf("\n");
 	ptr =	RC4(ciphertext, plaintext_len, key, key_len); 
	printf("cipher : %s\n",ptr);
 	return 0; 
 } 
#endif
