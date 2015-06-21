/*
 *  utf8.c
 *
 *  utf8
 *
 *  Copyright (C) 2008  Huang Guan
 *
 *  2008-7-10 13:31:57 Created.
 *
 *  Description: This file mainly includes the functions about 
 *  utf8
 *
 */

#ifdef __WIN32__
#include <windows.h>
#else
#include <iconv.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "debug.h"
#include "memory.h"
#include "config.h"
#include "utf8.h"



#ifdef __WIN32__
void utf8_to_gb ( char* src, char* dst, int len )
{
	int ret = 0;
	WCHAR* strA;
	int i= MultiByteToWideChar ( CP_UTF8, 0 , src, -1, NULL, 0 );
	if( i<=0 ){
		return;
	}
	strA = (WCHAR*)malloc( i*2 );
	MultiByteToWideChar ( CP_UTF8 , 0 , src, -1, strA , i);
	i= WideCharToMultiByte(CP_ACP,0,strA,-1,NULL,0,NULL,NULL);
	if( len >= i )
		ret = WideCharToMultiByte (CP_ACP,0,strA,-1,dst,i,NULL,NULL);
	if( ret<=0 ){
		free( strA ); return;
	}
	free( strA );
}
void gb_to_utf8 ( char* src, char* dst, int len )
{
	int ret = 0;
	WCHAR* strA;
	int i= MultiByteToWideChar ( CP_ACP, 0 , src, -1, NULL, 0 );
	if( i<=0 ){
		return;
	}
	strA = (WCHAR*)malloc( i*2 );
	MultiByteToWideChar ( CP_ACP , 0 , src, -1, strA , i);
	i= WideCharToMultiByte(CP_UTF8,0,strA,-1,NULL,0,NULL,NULL);
	if( len >= i )
		ret = WideCharToMultiByte (CP_UTF8,0,strA,-1,dst,i,NULL,NULL);
	if( ret<=0 ){
		free( strA ); return;
	}
	free( strA );
}
#else   //Linux
void utf8_to_gb( char* src, char* dst, int len )
{
	int ret = 0;
	uint inlen = strlen( src );
	uint outlen = len;
	char* inbuf = src;
	char* outbuf = dst;
	iconv_t cd;
	cd = iconv_open( "GBK18030", "UTF-8" );
	if ( cd != (iconv_t)-1 ){
		ret = iconv( cd, &inbuf, &inlen, &outbuf, &outlen );
		if( ret != 0 )
			printf("iconv failed err: %s\n", strerror(errno) );
		iconv_close( cd );
	}
}
void gb_to_utf8( char* src, char* dst, int len )
{
	int ret = 0;
	uint inlen = strlen( src );
	uint outlen = len;
	char* inbuf = src;
	char* outbuf = dst;
	iconv_t cd;
	cd = iconv_open( "UTF-8", "GBK18030" );
	if ( cd != (iconv_t)-1 ){
		ret = iconv( cd, &inbuf, &inlen, &outbuf, &outlen );
		if( ret != 0 )
			printf("iconv failed err: %s\n", strerror(errno) );
		iconv_close( cd );
	}
}
#endif



void decode_uri( char* input )
{
	#define HEX2BIN(c)(unsigned char)((c<='9')?c-'0':(c<='F')?c-'A'+10:c-'a'+10)
	char* s, *d;
	for( s=d=input; *s; s++ )
	{
		if( *s == '%' )
		{
			unsigned char b = HEX2BIN(*(s+1))<<4 | HEX2BIN(*(s+2));
			* d++ = b;
			s+=2;
		}else{
			* d++ = *s;
		}
	}
	*d = 0;
}


int if_UTF8(char *str)
{
// UTF8: %Ex%xx%xx

char *p =strstr(str, "%E"), *p2;

if(p ==NULL || strlen(p) <9 || *(p+6) !='%')
   return 0;
if(p > str+3)
{
   if(*(p-3) =='%' && (*(p-2) < '1' || *(p-2) > '3'))
    return 0;
}

while(*p)
{
   p2 =strstr(p+9, "%E");
   if(p2)
   {
    if(p2-p <9 || strlen(p2) <9 || *(p2+6) !='%') // 忽略单个中文被隔开的情况
     return 0;
    if(*(p2+7) > '0' && *(p2+7) <'4')
     return 0;
   }
   else
   {
    if(strlen(p) <9 || *(p+6) !='%')
     return 0;
    if(*(p+9) =='%')
    {
     if(*(p+10) >= 'A' && *(p+10) <='F')
      return 0;
    }
    if(*(p+7) > '0' && *(p+7) <'4')
     return 0;

    break;
   }
   p =p2;
}

return 1;
}
