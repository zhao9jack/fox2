/*
 *  proxy.c
 *
 *  Proxy Server
 *
 *  Copyright (C) 2009  Huang Guan
 *
 *  2009-10-06 11:32:50 Created.
 *
 *  Description: This file mainly includes the functions about 
 *  Proxy Server
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#ifdef __WIN32__
#include <winsock.h>
#include <wininet.h>
#include <io.h>
#define SHUT_RDWR SD_BOTH
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#else
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

#include "debug.h"
#include "memory.h"
#include "util.h"
#include "server.h"
#include "connection.h"
#include "proxy.h"

#ifndef __WIN32__
#define closesocket close 
#endif

#define MAX_CONTENT_LEN MB(1024)

/*
 *  Updated 20100421
 *  It's very very complicated to handle the received http data.
 *  I wrote this rubbishy code and tested, it worked :)
 *  Maybe somebody would give me a better version.
 */
static int proxy_recv( connection* conn, int sock_xy )
{
	char* buffer, *start, *end;
	int ret, pos, size, readheader, body_len, send_size, chunked=0, chunked_final=0;
	int tx=0,rx=0;
	NEW( buffer, KB(16)+4 );
	if( !buffer )
		return -1;
	
	pos = size = 0;
	readheader = 1;
	//skip proxy header
	while((ret = recv(sock_xy, buffer + size, KB(16) - size, 0)) > 0){
		// make the connection active!
		conn->time_alive = time( NULL );
		size += ret;
		rx+=ret;
		buffer[size]='\0';
PARSE:
		if( readheader==1 ){
			//RFC2616 states that check transfer-encoding before the content-length! But the following does not.
			char * header_end = strstr( buffer + pos, "\r\n\r\n" );
			if( header_end ){
				start = strstr( buffer + pos, "Content-Length: " );
				if( !start )
					start = strstr( buffer + pos, "content-length: " );
				if( start ){
					end = strstr( start, "\r\n" );
					if( !end )
						break;
					*end = '\0';
					body_len = atol( start + 16 );
					*end = '\r';
				}else if( strstr( buffer + pos, "chunked" ) ){
					chunked = 1;
					body_len = 0;
					conn->keep_alive = 0;
				}else if( strstr( buffer + pos, "Connection: close" ) || strstr( buffer+ pos, "Connection: Close" ) ){
					body_len = MAX_CONTENT_LEN;
					conn->keep_alive = 0;
				}else{
					body_len = 0;
				}
				//send header first
				send_size = (header_end - buffer + 4) - pos;
				if((ret = send( conn->socket, buffer + pos, send_size, 0)) != send_size ){
					DBG("send error : %d", ret );
					break;
				}
				tx += ret;
				pos = header_end - buffer + 4;
				readheader = 2;
				if( body_len == 0 && !chunked )
					break;
				if( size > pos )
					goto PARSE;
			}
		}else if(readheader==2){
			if( body_len == 0  ){
				if( !chunked )
					break;
				if( chunked_final ){
					readheader=3;
					goto PARSE;
				}
				end = strstr( buffer + pos, "\r\n" );
				if( !end )
					continue;
				*end = 0;
				sscanf( buffer+pos, "%x", &body_len );
				*end = '\r';
				if( body_len == 0 ){
					body_len += end -( buffer + pos ) + 2;
					chunked_final = 1;
				}else{
					body_len += end -( buffer + pos ) + 4;
				}
			}
			send_size = size - pos;
			if( body_len < send_size )
				send_size = body_len;
			if((ret = send( conn->socket, buffer + pos, send_size, 0)) != send_size){
				DBG("send error : %d", ret );
				break;
			}
			tx += ret;
			if( ret != send_size )
				perror("send error");
			if( body_len < size - pos )
				pos += send_size;
			else
				size = pos = 0;
			
			body_len -= send_size;
			if( body_len == 0 ){
				if( chunked )
					goto PARSE;
				if( size - pos > 0 )
					DBG("error, data ...");
				break;
			}
		}
		else if(readheader==3){ //chunked footer
			send_size = size - pos;
			if( body_len < send_size )
				send_size = body_len;
			if((ret = send( conn->socket, buffer + pos, send_size, 0)) != send_size){
				DBG("send error : %d", ret );
				break;
			}
			tx += ret;
			char * header_end = strstr( buffer + pos, "\r\n" );
			if( header_end && (header_end == buffer+pos || strstr( buffer + pos, "\r\n\r\n" ) ) )
				break; //finished footer
			size = pos = 0;
		}
	}
	if( tx!=rx ){
		DBG("error: Tx:%d  rx:%d in url: %s conn:%d  ret:%d", tx, rx, conn->uri, sock_xy, ret );
	}
	DEL( buffer );
	conn->responsed = 1;
	return ret;
}

static int proxy_connect( connection* conn, const char* ip, int port )
{
	struct sockaddr_in addr;
	if ( (conn->proxy_socket = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP )) <=0 ){
		perror("[proxy] Init Socket Error.");
		return -1;
	}
	memset( &addr, 0, sizeof(struct sockaddr_in) );
	addr.sin_family = PF_INET;
	addr.sin_addr.s_addr = inet_addr( ip );
	addr.sin_port = htons( port );
	if( connect( conn->proxy_socket, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0 )
	{
		perror("[proxy] connect failed.");
		return -2;
	}
	return 0;
}

int proxy_request( connection* conn, const char* ip, int port )
{
	int i, len=0;
	char* header;
	if( conn->proxy_socket <= 0 ){
		if( (i=proxy_connect( conn, ip, port )) < 0 )
			return i;
	}
	// prepare header
	NEW( header, KB(16) );
	if( !header ){
		return -3;
	}
	len = sprintf( header, "%s %s %s\r\n", conn->request_method, conn->uri, conn->http_version );
	
	for( i=0; i<conn->header_num; i++ ){
		len += sprintf( header, "%s%s: %s\r\n", header, 
			conn->headers[i].name, conn->headers[i].value );
		if( KB(16)-len < KB(1) )
			break;
	}
	//Maybe this could be optional!!
	len += sprintf( header, "%s%s: %s\r\n", header, 
		"X-Forwarded-For", conn->client->ip_string );
	strcat( header, "\r\n" );
	if( (i=send( conn->proxy_socket, header, strlen(header), 0 )) <0 ){
		DBG("send error: %d", i); 
		//re-establish the connection to the proxy server.
		pthread_mutex_lock( &conn->client->mutex_conn );
		closesocket( conn->proxy_socket );
		conn->proxy_socket = 0;
		pthread_mutex_unlock( &conn->client->mutex_conn );
		if( (i=proxy_connect( conn, ip, port )) < 0 ){
			return i;
		}
		if( (i=send( conn->proxy_socket, header, strlen(header), 0 )) <= 0 ){
			DBG("resend error %d.", i );
			return -4;
		}
		return -4;
	}
	DEL( header );
	if( conn->form_size > 0 ){
		if( (i=send( conn->proxy_socket, conn->form_data, conn->form_size, 0 )) !=conn->form_size ){
			return -5;
		}
	}
	if( (i=proxy_recv( conn, conn->proxy_socket ))<0 ){
		DBG("%d: proxy_recv error: %d", conn->proxy_socket, i);
		return -6;
	}
	return 0;
}
