/*
 *  main.c
 *
 *  Console Program
 *
 *  Copyright (C) 2008  Huang Guan
 *
 *  2008-7-9 10:23:51 Created.
 *
 *  Description: This file mainly includes the functions about 
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#ifdef __WIN32__
#include <winsock.h>
#include <wininet.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#endif
#include "config.h"
#include "debug.h"
#include "memory.h"
#include "server.h"
#include "connection.h"
static webserver* websrv;

void do_console()
{
	char buf[1024];
	while(1)
	{
		scanf( "%s", buf );
		if( strcmp( buf, "quit" ) == 0 ){
			break;
		}else if( strcmp( buf, "stop" ) == 0 ){
			server_stop( websrv );
		}else if( strcmp( buf, "start" ) == 0 ){
			server_start( websrv );
		}else if( strcmp( buf, "clear" ) == 0 ){
			server_clear( websrv );
		}else if( strcmp( buf, "reload" ) == 0 ){
			server_reload( websrv );
		}else if( strcmp( buf, "print" ) == 0 ){
			server_print( websrv );
			memory_print();
		}else{
			SLEEP(3); //Temperory fixed for linux nohup!
		}
	}
}

int main()
{
	//init win32 socket
#ifdef __WIN32__
	static WSADATA wsa_data; 
	int result = WSAStartup((WORD)(1<<8|1), &wsa_data); //初始化WinSocket动态连接库
	if( result != 0 ) // 初始化失败
		return -1;
#endif
#ifndef __WIN32__
	signal( SIGPIPE, SIG_IGN );	//ignore send,recv SIGPIPE error
#endif
	NEW( websrv, sizeof(webserver) );
	memset( websrv, 0, sizeof(webserver) );
	server_create( websrv, "config.xml" );
	server_start( websrv );
	do_console();
	server_stop( websrv );
	server_end( websrv );
	DEL( websrv );

#ifdef __WIN32__
	WSACleanup();
#endif
	memory_end();
	return 0;
}

