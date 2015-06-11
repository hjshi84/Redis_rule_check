#ifndef _MY_MD5_H
#define _MY_MD5_H

typedef union uwb {
    unsigned w;
    unsigned char b[4];
} WBunion;
 
typedef unsigned Digest[4];
 
 
typedef unsigned (*DgstFctn)(unsigned a[]);
char *get_hash_value(char *str);
//char* get_hash_value(int argc, char* argv[]);
unsigned *md5( const char *msg, int mlen);
#endif
