/*
 *  plugin_fox.c
 *
 *  Fox Client & Server Plugin
 *
 *  Copyright (C) 2010  Xiaoxia
 *
 *  2010-05-28 created.
 *
 *  Description: This file mainly includes the functions about 
 *  
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#ifdef __WIN32__
#include <winsock.h>
#include <wininet.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#endif

#include "config.h"
#include "server.h"
#include "connection.h"
#include "vdir.h"
#include "session.h"
#include "client.h"
#include "loop.h"
#include "util.h"

#ifndef __WIN32__
#define closesocket close 
#endif

#define NEW( p, size ) { p = malloc(size); if(!p){printf("failed to allocate %d bytes\n", size);exit(1);} }
#define DEL( p ) { if(p) free((void*)p); p = NULL; }

#define MAGIC_CODE 126

#define MAX_CONTENT_LEN MB(1024)

#define MAX_ONLINE_CLIENT 1024

#define FOX_VERSION "1.0.3"

#define MAX_PROXY_ADDR 64

enum FOX_MODE{
	ENCODE,
	DECODE
};

typedef struct foxclient{
	char	ip[24];
	char	session_key[24];
	char	user_name[64];
	time_t	login_time;
}foxclient;

static char entry[64], proxy_host[64], login_url[130], admin_password[64];
static int sendMode, receiveMode, login_url_len;
static struct sockaddr_in proxy_addr, proxy_connect_addr;
static char proxy_header[]="HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
static struct loop loop_clients; 
static int session_timeout;
static char client_session_key[30]="", client_user[64]="", client_ip[32]="";

static networkaddress proxy_addrs[MAX_PROXY_ADDR];
static int proxy_addrs_count;

static int encode_and_send( connection* conn, int sock_xy, char* data, int data_size, int content_len );

static void decode( char* data, int size )
{
	unsigned char* p = (unsigned char*) data;
	if( p ){
		int j;
		for(j=0; j<size; j++)
			p[j] ^= MAGIC_CODE;
	}
}

static void encode( char* data, int size )
{
	decode( data, size );
}


#ifndef __WIN32__
#define closesocket close 
#endif


static int parse_http_header( connection* conn, char* header, int* chunked )
{
	char* start, *end;
	int body_len;
	start = strstr( header, "Content-Length: " );
	*chunked = 0;
	if( !start )
		start = strstr( header, "content-length: " );
	if( start ){
		end = strstr( start, "\r\n" );
		if( !end )
			return 0;
		*end = '\0';
		body_len = atol( start + 16 );
		*end = '\r';
	}else if( strstr( header, "chunked" ) ){
		*chunked = 1;
		body_len = 0;
		conn->keep_alive = 0;
	}else if( strstr( header, "Connection: close" ) || strstr( header, "Connection: Close" ) ){
		body_len = MAX_CONTENT_LEN;
		conn->keep_alive = 0;
	}else{
		body_len = 0;
	}
	return body_len;
}

/* Receive http message from the Fox Server*/
static int recv_and_decode( connection* conn, int sock_xy, int sock_to )
{
	char* buffer, *start, *end;
	int ret, pos, size, readheader, body_len, send_size, chunked, chunked_final;
	pos = size = 0;
	readheader = 1;
	NEW( buffer, KB(16)+4 );
	chunked = chunked_final = 0;
	//skip proxy header
	while((ret = recv(sock_xy, buffer + size, KB(16) - size, 0)) > 0){
		// make the connection active!
		conn->time_alive = time( NULL );
		size += ret;
		buffer[size]='\0';
PARSE:
		if( readheader==1 ){ //Get HTTP Header
			char * header_end = strstr( buffer + pos, "\r\n\r\n" );
			if( header_end ){
				body_len = parse_http_header( conn, buffer+pos, &chunked );
				pos = header_end - buffer + 4;
				readheader = 2;
				if( body_len <= 0 && !chunked ) //no message body!
					break;
				else if( size > pos ) //there is message data in buffer!
					goto PARSE;
			}
		}else if(readheader==2){ //Read HTTP Message Body
			if( body_len == 0  ){
				if( !chunked )
					break;
				if( chunked_final ){
					readheader=3;
					goto PARSE;
				}
				if( buffer[pos]=='\r' )
					pos += 2;
				if( pos >= size )
					continue;
				end = strstr( buffer + pos, "\r\n" );
				if( !end )
					continue;
				*end = 0;
				sscanf( buffer+pos, "%x", &body_len );
				*end = '\r';
				if( body_len == 0 ){
					chunked_final = 1;
					pos += end -( buffer + pos ) + 2;
				}else{
					pos += end -( buffer + pos ) + 2;
				}
			}
			if( pos >= size )
				continue;
			send_size = size - pos;
			if( body_len < send_size )
				send_size = body_len;
			decode( buffer+pos, send_size );
			if((ret = send( sock_to, buffer + pos, send_size, 0)) != send_size){
				printf("[FoxProject]send error : %d\n", ret );
				break;
			}
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
				if( size - pos > 0 ){
					break;
					printf("[FoxProject]warning, too much data to handle!\n");
					readheader = 1;
					chunked = chunked_final = 0;
					goto PARSE;
				}
				break;
			}
		}
		else if(readheader==3){ //chunked footer
			char * header_end = strstr( buffer + pos, "\r\n" );
			if( header_end && (header_end == buffer+pos || strstr( buffer + pos, "\r\n\r\n" ) ) )
				break; //finished footer
			size = pos = 0;
		}
	}
	DEL( buffer );
	return ret;
}


/*
 *  Updated 20100421
 *  It's very very complicated to handle the received http data.
 *  I wrote this rubbishy code and tested, it worked :)
 *  Maybe somebody would give me a better version.
 */
/* Receive http from Website */
static int proxy_recv( connection* conn, int sock_xy, int sock_to )
{
	if( receiveMode == ENCODE ){
		char tmp[16];
		char* buffer, *start, *end;
		int ret, pos, size, readheader, body_len, send_size, chunked=0, chunked_final=0;
		NEW( buffer, KB(16)+4 );
		pos = size = 0;
		readheader = 1;
		//skip proxy header
		while((ret = recv(sock_xy, buffer + size, KB(16) - size, 0)) > 0){
			// make the connection active!
			conn->time_alive = time( NULL );
			size += ret;
			buffer[size]='\0';
	PARSE:
			if( readheader==1 ){
				char * header_end = strstr( buffer + pos, "\r\n\r\n" );
				if( header_end ){
					body_len = parse_http_header( conn, buffer+pos, &chunked );
					
					//send header
					send_size = (header_end - buffer + 4) - pos;
					
					//Send proxy header goes first!
					if( chunked ){
						send( sock_to, proxy_header, sizeof(proxy_header)-1, 0);
						int t_len = sprintf( tmp, "%x\r\n", send_size );
						send( sock_to, tmp, t_len, 0 );
					}else{
						char t[128];
						int t_len;
						if( body_len == MAX_CONTENT_LEN ){
							t_len = sprintf( t, "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n" );
						}else{
							t_len = sprintf( t, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", send_size + body_len );
						}
						send( sock_to, t, t_len, 0 );
					}
					encode( buffer+pos, send_size );
					if((ret = send( sock_to, buffer + pos, send_size, 0)) != send_size ){
						printf("[FoxProject]send error : %d\n", ret );
						break;
					}
					if( chunked )
						send( sock_to, "\r\n", 2, 0 );
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
				if( chunked ){
					int t_len = sprintf( tmp, "%x\r\n", send_size );
					send( sock_to, tmp, t_len, 0 );
				}
				encode( buffer+pos, send_size );
				if((ret = send( sock_to, buffer + pos, send_size, 0)) != send_size){
					printf("[FoxProject]send error : %d\n", ret );
					break;
				}
				if( chunked )
					send( sock_to, "\r\n", 2, 0 );
				if( ret != send_size )
					perror("send error");
				if( body_len < size - pos )
					pos += send_size;
				else
					size = pos = 0;
				
				body_len -= send_size;
				if( body_len == 0 ){
					if( chunked ){
						if(size>pos)
							goto PARSE;
						else
							continue;
					}
					if( size - pos > 0 )
						printf("error, data ...");
					break;
				}
			}
			else if(readheader==3){ //chunked footer
				int endf = 0;
				char * header_end = strstr( buffer + pos, "\r\n" );
				if( header_end && (header_end == buffer+pos || strstr( buffer + pos, "\r\n\r\n" ) ) )
					endf = 1;
				send_size = size - pos;
				if( chunked ){
					int t_len = sprintf( tmp, "%x\r\n", send_size );
					send( sock_to, tmp, t_len, 0 );
				}
				encode( buffer+pos, send_size );
				if((ret = send( sock_to, buffer + pos, send_size, 0)) != send_size){
					printf("send error : %d", ret );
					break;
				}
				if( chunked )
					send( sock_to, "\r\n", 2, 0 );
				if( endf && chunked ){
					send( sock_to, "0\r\n\r\n", 5, 0 );
					break; //finished footer
				}
			}
		}
		DEL( buffer );
		return ret;
	}else{ //DECODE Mode
		return recv_and_decode( conn, sock_xy, sock_to );
	}
}

static int proxy_connect( connection* conn, struct sockaddr_in* addr )
{
	//re-establish the connection to the proxy server.
	if( conn->proxy_socket ){
		pthread_mutex_lock( &conn->client->mutex_conn );
		closesocket( conn->proxy_socket );
		conn->proxy_socket = 0;
		pthread_mutex_unlock( &conn->client->mutex_conn );
	}
	if ( (conn->proxy_socket = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP )) <=0 ){
		perror("[FoxProject] Init Socket Error.");
		return -1;
	}
	if( connect( conn->proxy_socket, (struct sockaddr*)addr, sizeof(struct sockaddr_in)) < 0 )
	{
		perror("[FoxProject] connect failed.");
		return -2;
	}
	return 0;
}

static int client_searcher( const void* p , const void* q )
{
	foxclient* fc = (foxclient*)p;
	if( strcmp(fc->user_name, q) == 0 )
		return 1;
	return 0;
}

/* Fox2 Check Login Program */
static int check_login( connection* conn )
{
	foxclient* fc = (foxclient*)loop_search(&loop_clients, conn->header_value(conn, "FoxClientUser"), client_searcher );
	time_t t = time(NULL);
	if( !fc ) //Not logged yet!
		return 0;
	if( t-fc->login_time > session_timeout )  //Timeout
		return 0;
	if( !conn->header_equal( conn, "FoxClientKey", fc->session_key ) )	//Logged in different place
		return 0;
	fc->login_time = t;
	return 1;
}


/* SSL data is already encoded. Do we need to encode it again? */
static int start_connect_mode( connection* conn )
{
	int ret;
	fd_set fdreads;
	char* buffer;
	struct timeval tv;
	NEW( buffer, KB(16)+4 );
	printf("[FoxProject] Start CONNECT mode\n");
	if( conn->http_read_size > conn->http_read_pos ){
		conn->http_read_buf[conn->http_read_size] = 0;
		send( conn->proxy_socket, conn->http_read_buf+conn->http_read_pos, conn->http_read_size-conn->http_read_pos, 0 );
		conn->http_read_buf[conn->http_read_size]=0;
	}
	set_socket_nonblocking( conn->socket );
	set_socket_nonblocking( conn->proxy_socket );
	for( ;conn->state!=C_END&&conn->server->state==S_RUNNING; ){
		int maxfd;
		FD_ZERO(&fdreads);
		FD_SET( conn->socket, &fdreads );
		FD_SET( conn->proxy_socket, &fdreads );
		maxfd = conn->socket>conn->proxy_socket?conn->socket:conn->proxy_socket; 
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		ret = select(maxfd+1, &fdreads, 0, 0, 0);
		if( ret <= 0 )
			break;
		if( ret > 0 ){
			if( FD_ISSET(conn->socket, &fdreads) ){ //receive from browser
				ret = recv( conn->socket, buffer, KB(16), 0 );
				if( ret > 0 )
					ret = send_and_wait( conn->proxy_socket, buffer, ret );
			}
			if( FD_ISSET(conn->proxy_socket, &fdreads) ){ //receive from fox server
				ret = recv( conn->proxy_socket, buffer, KB(16), 0 );
				if( ret > 0 )
					ret = send_and_wait( conn->socket, buffer, ret );
			}
			if( ret <= 0 ) //one or more socket error
				break;
		}
	}
	DEL( buffer );
	printf("[FoxProject] End CONNECT mode\n");
	return ret;
}

static int encode_and_send( connection* conn, int sock_xy, char* data, int data_size, int content_len )
{
	int ret;
	char *proxy_header;
	encode( data, data_size );
	// Build proxy header
	NEW( proxy_header, KB(2) );
	int proxy_header_len = sprintf( proxy_header, "POST http://%s/fox2/jumps.do HTTP/1.1\r\nHost: %s\r\nProxy-Connection: keep-alive\r\n"
		"FoxClientVersion: %s\r\nFoxClientKey: %s\r\nFoxClientUser: %s\r\nFoxClientIP: %s\r\nFoxClientPort: %d\r\nContent-Length: %d\r\n\r\n", 
		proxy_host, proxy_host, FOX_VERSION, client_session_key, client_user, conn->server->server_ip, conn->server->server_port, content_len );
	if( (ret=send( sock_xy, proxy_header, proxy_header_len, 0 )) != proxy_header_len ){
		printf("[FoxProject]send to proxy server error: %d\n", ret);
		if( (ret=proxy_connect( conn, &proxy_addr )) == 0 ){
			if( (ret=send( sock_xy, proxy_header, proxy_header_len, 0 )) != proxy_header_len ){
				printf("[FoxProject]resend error %d.", ret );
				DEL( proxy_header );
				return -4;
			}
		}
	}
	DEL( proxy_header );
	if( (ret=send( sock_xy, data, data_size, 0 )) != data_size )
		printf("[FoxProject]send header failed: %d\n", ret);
	return ret;
}

static int proxy_request( connection* conn )
{
	int ret, len=0;
	int connect_mode = 0;
	char* header;
	if( conn->proxy_socket <= 0 ){
		if( (ret=proxy_connect( conn, &proxy_addr )) < 0 )
			return ret;
	}
	if( sendMode == ENCODE ){ //Add a extra header and send encoded data.
		if( strcmp( conn->request_method, "CONNECT" ) == 0 ){
			connect_mode = 1;
			//re-establish the connection to the fox server without going through any proxy servers.
			if( (ret=proxy_connect( conn, &proxy_connect_addr )) < 0 )
				return ret;
		}
		// Build initial header
		NEW( header, KB(16) );
		if( !header )
			return -3;
		len = sprintf( header, "%s %s %s\r\n", conn->request_method, conn->uri, conn->http_version );
		int i;
		for( i=0; i<conn->header_num; i++ ){
			len += sprintf( header+len, "%s: %s\r\n", conn->headers[i].name, conn->headers[i].value );
			if( KB(16)-len < KB(1) )
				break;
		}
		strcat( header, "\r\n" );
		len += 2;
		encode_and_send( conn, conn->proxy_socket, header, len, len+conn->form_size );
		if( conn->form_data > 0 ){
			encode( conn->form_data, conn->form_size );
			if( (ret=send( conn->proxy_socket, conn->form_data, conn->form_size, 0 )) !=conn->form_size ){
				printf("[FoxProject]failed in sending form data. ret=%d\n", ret);
				return -5;
			}
		}
		DEL( header );
	}else{ //DECODE Mode
		if( conn->form_size > 0 ){
			char request_url[128+4], *first_sp;
			int pos = 0;
			decode( conn->form_data, conn->form_size );
			request_url[0] = 0;
			len = get_line( conn->form_data, conn->form_size, &pos, request_url, 128 );
			/* Fox2 Login Program */
			/* If len==0, it means we failed to get the uri, so we regard it as cheating */
			first_sp = strchr( request_url, ' ');
			if( !first_sp || strncmp( first_sp+1, login_url, login_url_len )!=0 ){ //if not our login program
				if( 1 && ( len==0 || !check_login(conn) ) ){ //if not logged in
					//Redirect to login page.
					char* proxy_header;
					int proxy_header_len;
					NEW( header, KB(2)+4 );
					NEW( proxy_header, KB(2) );
					len = sprintf( header, "HTTP/1.1 302 Moved Temporarily\r\nLocation: %s?direct_ip=%s&client_ip=%s&client_port=%s&client_version=%s\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n", 
						login_url, conn->client->ip_string, conn->header_value(conn, "FoxClientIP"), conn->header_value(conn, "FoxClientPort"),
						conn->header_value(conn, "FoxClientVersion") );
					proxy_header_len = sprintf( proxy_header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", len );
					encode( header, len );
					send( conn->socket, proxy_header, proxy_header_len, 0 );
					send( conn->socket, header, len, 0 );
					return 0;
				}
			}
			if( strncmp( request_url, "CONNECT", 7 ) == 0 )
				connect_mode = 1;
			if( (ret=send( conn->proxy_socket, conn->form_data, conn->form_size, 0 )) !=conn->form_size )
				return -5;
		}
		
	}
	if( (ret=proxy_recv( conn, conn->proxy_socket, conn->socket ))<0 ){
		printf("[FoxProject] proxy_recv error: %d\n", ret);
		return -6;
	}
	if( connect_mode ){
		//启用CONNECT模式
		conn->keep_alive = 0;
		return start_connect_mode( conn );
	}
	return 0;
}

static int fox_jumps( connection* conn )
{
	int ret = 0;
	if( stricmp( conn->file_name, "update_ip.do" ) == 0 ){	//Server Method
		char pwd[70];
		conn->param_get( conn, "password", pwd, 64 );
		if( strcmp( pwd, admin_password )==0 ){
			char user_name[64];
			char cmd[16];
			char key[30];
			user_name[0] = '\0';
			conn->param_get( conn, "user", user_name, 60 );
			conn->param_get( conn, "cmd", cmd, 12 );
			conn->param_get( conn, "key", key, 24 );
			foxclient* fc = loop_search(&loop_clients, user_name, client_searcher );
			if(fc==NULL ){
				NEW(fc, sizeof(foxclient) );
				strcpy( fc->user_name, user_name );
				loop_push_to_head( &loop_clients, fc );
			}
			if( strcmp( cmd, "logout" )==0 ){
				if( strcmp( fc->session_key, key ) == 0 ){
					fc->login_time = 0;
				}else{
					printf("[FoxProject]wrong session_key\n");
				}
			}else if(strcmp( cmd, "login")==0 ){
				fc->login_time = time(NULL);
				strcpy( fc->session_key, key );
			}else 
				printf("[FoxProject]unknown cmd: %s\n", cmd );
		}else{
			printf("[FoxProject]wrong password.\n");
		}
	}else if( stricmp( conn->file_name, "update_session.do" ) == 0 ){	//Client Method
		conn->param_get( conn, "key", client_session_key, 24 );
		conn->param_get( conn, "user", client_user, 24 );
		printf("[FoxProject] Login with user=%s and key=%s\n", client_user, client_session_key );
	}else{
		ret = proxy_request( conn );
		conn->responsed = 1;
	}
	if( ret < 0 )
		return -1;
	return 0;
}


static void client_eraser( const void * p )
{
	DEL(p);
}


#ifdef __WIN32__
#define EXPORT __declspec(dllexport) __stdcall 
#else
#define EXPORT
#endif
int EXPORT plugin_entry( webserver* srv )
{
	// Read Fox Configuration from server config file.
	read_network_addr( proxy_addrs, xml_readstr( srv->config, "/foxProject/serverList" ), &proxy_addrs_count, MAX_PROXY_ADDR );
	strncpy( entry, xml_readstr( srv->config, "/foxProject/entry" ), 63 );
	strncpy( proxy_host, xml_readstr( srv->config, "/foxProject/proxyHost" ), 63 );
	strncpy( login_url, xml_readstr( srv->config, "/foxProject/loginURL" ), 128 );
	login_url_len = strlen(login_url);
	strncpy( admin_password, xml_readstr( srv->config, "/foxProject/password" ), 63 );
	session_timeout = xml_readnum( srv->config, "/foxProject/sessionTimeout");
	if( session_timeout <=0 ){
		session_timeout = 60;
	}
	if( strcmp(xml_readstr( srv->config, "/foxProject/sendMode" ), "encode" )==0 )
		sendMode = ENCODE;
	else
		sendMode = DECODE;
	if( strcmp(xml_readstr( srv->config, "/foxProject/receiveMode" ), "encode" )==0 )
		receiveMode = ENCODE;
	else
		receiveMode = DECODE;
	//Choose a random server.
	srand(time(NULL));
	int i = rand()%proxy_addrs_count;
	memset( &proxy_addr, 0, sizeof(proxy_addr) );
	proxy_addr.sin_family = PF_INET;
	proxy_addr.sin_addr.s_addr = inet_addr( proxy_addrs[i].ip );
	proxy_addr.sin_port = htons( proxy_addrs[i].port );
	memset( &proxy_connect_addr, 0, sizeof(proxy_connect_addr) );
	proxy_connect_addr.sin_family = PF_INET;
	if( netaddr_set( proxy_host, &proxy_connect_addr ) < 0 )
		printf("[FoxProject] Failed to parse %s\n", proxy_host );
	proxy_connect_addr.sin_port = htons(80);
	
	srv->vdir_create( srv, entry, (vdir_handler)fox_jumps );
	loop_create( &loop_clients, MAX_ONLINE_CLIENT, client_eraser );
	printf("[FoxProject] Fox jumps over %s:%d on %s\n", proxy_addrs[i].ip, proxy_addrs[i].port, entry );
	return 0;
}

int EXPORT plugin_cleanup( webserver* srv )
{
	loop_cleanup( &loop_clients );
	return 0;
}
